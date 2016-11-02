/*
 ============================================================================
 Name        : PokeDexServidor.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Servidor PokéDex
 ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <commons/log.h>
#include "socket.h" // BORRAR
#include "PokeDexServidor.h"
#include "osada.h"
#include <commons/bitarray.h>
#include <sys/mman.h>
#include <commons/collections/queue.h>

#include "protocoloPokedexClienteServidor.h"

#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola
#define TAMANIO_MAXIMO_MENSAJE 50 // Tamaño máximo de un mensaje
#define IP_SERVIDOR "127.0.0.1"
#define PUERTO "8080"
#define CANTIDAD_MAXIMA_CONEXIONES 1000
#define TABLA_ARCHIVOS 2048
#define LOG_FILE_PATH "PokeDexServidor.log"

struct socket** clientes;
char* rutaFS = "/home/utnso/workspace/tp-2016-2c-CodeTogether/challenge.bin";
char* rutaOsadaDir = "/home/utnso/workspace/tp-2016-2c-CodeTogether/Osada";

osada_file tablaArchivos[2048];		// TABLA DE ARCHIVOS
unsigned int * tablaAsignaciones;	// TABLA DE ASIGNACIONES
t_bitarray * bitarray;				// ARRAY BITMAP
char* pmapFS; 						//Puntero de mmap
int inicioBloqueDatos;				//INICIO BLOQUE DE DATOS

pthread_mutex_t mutex;				//SEMAFORO PARA SINCRONIZAR OPERACIONES
/* Logger */
t_log* logger;
t_queue* colaOperaciones;

int main(void) {
	// Variables para el manejo del FileSystem OSADA
	int fileFS;
	struct stat statFS;
	osada_header cabeceraFS;		// HEADER
	unsigned char * bitmapS;		// BITMAP
	int bloquesTablaAsignaciones;	// CANTIDAD DE BLOQUES DE LA TABLA DE ASIGNACIONES
	int i;
	pthread_mutex_init(&mutex, NULL);

	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;


	logger = log_create(LOG_FILE_PATH, "POKEDEX_SERVIDOR", true, LOG_LEVEL_INFO);


	// CARGA DE FS EN MEMORIA CON MMAP
	printf("\n---------------------------");
	printf("\n	MMAP	\n");
	printf("---------------------------\n");

	fileFS = open(rutaFS,O_RDWR);
	stat(rutaFS,&statFS);
	pmapFS = (char*)mmap(0, statFS.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileFS, 0); //puntero al primer byte del FS (array de bytes)
	printf("El tamaño del FS es %d \n", statFS.st_size);

	memcpy(&cabeceraFS, &pmapFS[0], sizeof(cabeceraFS));
	// Valida que sea un FS OSADA
	if (memcmp(cabeceraFS.magic_number, "OsadaFS", 7) != 0)	{
		printf("NO es un FS Osada\n");
		return EXIT_FAILURE;
	}


	// INFORMACION HEADER
	printf("\n---------------------------");
	printf("\n	HEADER	\n");
	printf("---------------------------\n");
	printf("Identificador: %s\n", cabeceraFS.magic_number);
	printf("Version:       %d\n", cabeceraFS.version);
	printf("Tamano FS:     %d (bloques)\n", cabeceraFS.fs_blocks);
	printf("Tamano Bitmap: %d (bloques)\n", cabeceraFS.bitmap_blocks);
	printf("Inicio T.Asig: %d (bloque)\n", cabeceraFS.allocations_table_offset);
	printf("Tamano Datos:  %d (bloques)\n", cabeceraFS.data_blocks);
	printf("Relleno: %s\n", cabeceraFS.padding);


	// Aloca el espacio en memoria para el Bitmap
	bitmapS=(char *)malloc(cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);
	if (bitmapS== NULL)	{
		printf("No se dispone de memoria para alocar el Bitmap\n");
		return EXIT_FAILURE;
	}

	// Lee el BITMAP
	memcpy(bitmapS, &pmapFS[1 * OSADA_BLOCK_SIZE], cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// Crea el array de bits
	bitarray = bitarray_create(bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// INFORMACION BITMAP
	printf("\n---------------------------");
	printf("\n	BITMAP	\n");
	printf("---------------------------\n");
	printf("Pos %d = %d\n", 0, bitarray_test_bit(bitarray, 0));
	printf("Pos %d = %d\n", 11501, bitarray_test_bit(bitarray, 11501));
	printf("Pos %d = %d\n", 11502, bitarray_test_bit(bitarray, 11502));
	printf("Pos %d = %d\n", 11551, bitarray_test_bit(bitarray, 11551));
	printf("Pos %d = %d\n", bitarray_get_max_bit(bitarray), bitarray_test_bit(bitarray, bitarray_get_max_bit(bitarray)));

	/*
	// Imprime todos los bits
	j = bitarray_get_max_bit(bitarray);
	for (i = 0; i < j; i++)
		printf("%d = %d\n", i, bitarray_test_bit(bitarray, i));
	 */

	// Lee tabla de archivos
	memcpy(&tablaArchivos, &pmapFS[(1 + cabeceraFS.bitmap_blocks) * OSADA_BLOCK_SIZE], 1024 * OSADA_BLOCK_SIZE);

	printf("\n---------------------------");
	printf("\n	TABLA DE ARCHIVOS	\n");
	printf("---------------------------\n");
	printf("%s\n", "|    Pos    	|    State    	|    File Name    	|    Parent Dir |    Size    	|    lastModif    	|    First Block");
	printf("%s\n", "--------------------------------------------------------------------------------------------------------");
	for (i = 0; i < TABLA_ARCHIVOS; i++)
	{
		if (tablaArchivos[i].state != 0)
		{
			printf("|    %d    	", i);
			printf("|    %d    	", tablaArchivos[i].state);
			printf("|    %s    	", tablaArchivos[i].fname);
			printf("|    %d    	", tablaArchivos[i].parent_directory);
			printf("|    %d    	", tablaArchivos[i].file_size);
			printf("|    %d    	", tablaArchivos[i].lastmod);
			printf("|    %d    	", tablaArchivos[i].first_block);
			printf("\n");
		}
	}


	// BLOQUE DE DATOS
	printf("\n---------------------------");
	printf("\n	BLOQUE DE DATOS	\n");
	printf("---------------------------\n");
	inicioBloqueDatos = cabeceraFS.fs_blocks - cabeceraFS.data_blocks;
	printf("Inicio Datos: %d (bloque)\n", inicioBloqueDatos);


	// Calculo bloques
	bloquesTablaAsignaciones = cabeceraFS.fs_blocks - 1 - cabeceraFS.bitmap_blocks - 1024 - cabeceraFS.data_blocks;
	printf("Leido Tabla de Archivos %d\n", bloquesTablaAsignaciones); // POSICION

	//Reserva espacio tabla de asignaciones
	tablaAsignaciones=(int *)malloc(bloquesTablaAsignaciones * OSADA_BLOCK_SIZE);
	if (tablaAsignaciones== NULL) {
		printf("No se dispone de memoria para alocar la Tabla de Asignaciones\n");
		return EXIT_FAILURE;
	}

	// Lee la tabla de ASIGNACIONES
	memcpy(tablaAsignaciones, &pmapFS[(1 + cabeceraFS.bitmap_blocks + 1024) * OSADA_BLOCK_SIZE], bloquesTablaAsignaciones * OSADA_BLOCK_SIZE);


	// INFORMACION TABLA DE ASIGNACIONES
	printf("\n---------------------------");
	printf("\n	TABLA DE ASIGNACIONES	\n");
	printf("---------------------------\n");
	printf("Tamano T.Asig: %d (bloques)\n\n", bloquesTablaAsignaciones);


	//Imprime tabla de asignaciones
	/*for (i = 0; i <  50 ;i++)
		{
			if (tablaAsignaciones[i] != 0)
			{
				printf("|    %d    	", i);
				printf("|    %d    	", tablaAsignaciones[i]);
				printf("\n");
			}
		}*/

	char* ruta = "/";
	escribirEstructura(65535,ruta); //Funcion recursiva, comienza en la raiz

	getattr_callback("/Pallet Town/Pokemons/Desafios/special.mp4");
	readdir_callback("/Pallet Town/Pokemons");




	// CREACIÓN DEL HILO EN ESCUCHA
	pthread_attr_init(&atributosHilo);
	pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);

	colaOperaciones = queue_create();

	while(1) {
		if(queue_size(colaOperaciones)> 0)
		{
			operacionOSADA *operacionActual;
			socket_t* socketPokedex;

			void* mensajeRespuesta = malloc(TAMANIO_MAXIMO_MENSAJE);

			pthread_mutex_lock(&mutex);
			operacionActual = queue_pop(colaOperaciones);
			pthread_mutex_unlock(&mutex);

			mensajeRespuesta = ((operacionOSADA*) operacionActual)->operacion;
			socketPokedex = ((operacionOSADA*) operacionActual)->socket->socket;
			switch(((mensaje_t*) mensajeRespuesta)->tipoMensaje) {
			case READDIR:
				log_info(logger, "Solicito READDIR del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);
				char* cadenaMensaje = readdir_callback(((mensaje1_t*) mensajeRespuesta)->path);
				printf("CADENA RESPUESTA: %s /n", cadenaMensaje);
				// Enviar mensaje READ
					paquete_t paqueteLectura;
					mensaje2_t mensajeREADDIR_RESPONSE;

					mensajeREADDIR_RESPONSE.tipoMensaje = READDIR_RESPONSE;
					mensajeREADDIR_RESPONSE.tamanioMensaje = strlen(cadenaMensaje) + 1;
					mensajeREADDIR_RESPONSE.mensaje = cadenaMensaje;

					crearPaquete((void*) &mensajeREADDIR_RESPONSE, &paqueteLectura);
					if(paqueteLectura.tamanioPaquete == 0) {
						socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
						log_info(logger, socketPokedex->error);
						log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
						exit(EXIT_FAILURE);
					}

					enviarMensaje(socketPokedex, paqueteLectura);
				break;
			case GETATTR: ;
					log_info(logger, "Solicito GETATTR del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);

					// Enviar mensaje GETATTR
					paquete_t paqueteGetAttr;
					mensaje3_t mensajeGETATTR_RESPONSE;

					mensajeGETATTR_RESPONSE.tipoMensaje = GETATTR_RESPONSE;
					mensajeGETATTR_RESPONSE.tamanioArchivo = 50;
					mensajeGETATTR_RESPONSE.tipoArchivo = 1;

					crearPaquete((void*) &mensajeGETATTR_RESPONSE, &paqueteGetAttr);
					if(paqueteGetAttr.tamanioPaquete == 0) {
						socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
						log_info(logger, socketPokedex->error);
						log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
						exit(EXIT_FAILURE);
					}

					enviarMensaje(socketPokedex, paqueteGetAttr);
				break;
			}
		}
	}

	munmap (pmapFS, statFS.st_size); //Bajo el FS de la memoria
	close(fileFS); //Cierro el archivo

	//Test de escritura en OSADA
	//FILE *fp;
	//fp = fopen("test.txt", "w+");
	//fread(buffer,)


	////////////////////
	///// SHUTDOWN /////
	////////////////////
	free(bitmapS);
	free(tablaAsignaciones);
	bitmapS=NULL;
	tablaAsignaciones=NULL;

	return EXIT_SUCCESS;
}

char* readdir_callback(const char *path) {
	char *cadenaMensaje = string_new();
	int i;
	int dirPadre = getattr_callback(path);
	for (i = 0; i < TABLA_ARCHIVOS; i++) {
		if (tablaArchivos[i].state != 0) {
			if ((tablaArchivos[i].state == 2)&&(tablaArchivos[i].parent_directory==dirPadre)) { //Directorios en el directorio
				printf("DIRECTORIO: %s Tipo: %d  \n ", tablaArchivos[i].fname, tablaArchivos[i].state);
				string_append(&cadenaMensaje, tablaArchivos[i].fname);
				string_append(&cadenaMensaje, "#");
			} else {
				if ((tablaArchivos[i].state == 1)&&(tablaArchivos[i].parent_directory==dirPadre)) { //Archivos en el directorio
					printf("ARCHIVO: %s Tipo: %d  Tamaño: %d \n ", tablaArchivos[i].fname, tablaArchivos[i].state, tablaArchivos[i].file_size);
					string_append(&cadenaMensaje, tablaArchivos[i].fname);
					string_append(&cadenaMensaje, "#");
				}
			}
		}
	}
	return cadenaMensaje;
}

int getattr_callback(const char *path) { //, struct stat *stbuf) {
	// memset(stbuf, 0, sizeof(struct stat));

	printf("\nBuscando ruta: %s \n", path);
	char** array;
	int i = 0;
	int res = -1;
	int dirPadre = 65535;
	if (!(string_equals_ignore_case(path, "/"))) {
		array = string_split(path, "/");
		while (array[i]) {
			char* fname = array[i];
			printf("Buscando nombre: %s \n", array[i]);
			res = buscarTablaAchivos(dirPadre,array[i]);
			printf("Encontrado id: %d \n", res);
			dirPadre = res;
			i++;
		}
		if (tablaArchivos[res].state == 1) {
			printf("Es un ARCHIVO: %s Tipo: %d  Tamaño: %d \n ", tablaArchivos[res].fname, tablaArchivos[res].state, tablaArchivos[res].file_size);
		}
		if (tablaArchivos[res].state == 2) {
			printf("Es un DIRECTORIO: %s Tipo: %d \n", tablaArchivos[res].fname, tablaArchivos[res].state);
		}
	} else {
		res = dirPadre; //Estamos en el root
	}
	return res;
}
/*
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  if (strcmp(path, filepath) == 0) {
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(filecontent);
    return 0;
  }

  return -1;*/

int buscarTablaAchivos(int dirPadre, char* fname) {
	int i;
	int res = -1;
	for (i = 0; i < TABLA_ARCHIVOS; i++) {
		if (tablaArchivos[i].state != 0) {
			if ((tablaArchivos[i].state == 2)&&(tablaArchivos[i].parent_directory==dirPadre)&&(strcmp(tablaArchivos[i].fname, fname) == 0)) { //Es un direcotrio
				res = i;

			} else {
				if ((tablaArchivos[i].state == 1)&&(tablaArchivos[i].parent_directory==dirPadre)&&(strcmp(tablaArchivos[i].fname, fname) == 0)) { //Es un archivo
					res = i;
				}
			}
		}
	}
	return res;
}

void escribirEstructura(int dirPadre, char* ruta) {
	int i;

	for (i = 0; i < TABLA_ARCHIVOS; i++) {
		if (tablaArchivos[i].state != 0) {
			if ((tablaArchivos[i].state == 2)&&(tablaArchivos[i].parent_directory==dirPadre)) { //Directorios en el directorio
				char *str = string_new();
				string_append(&str, rutaOsadaDir);
				string_append(&str, ruta);
				string_append(&str, tablaArchivos[i].fname);
				mkdir(str, 0700);
				printf("\nCreando DIRECTORIO %s \n", str);

				char *str2 = string_new();
				string_append(&str2, ruta);
				string_append(&str2, tablaArchivos[i].fname);
				string_append(&str2, "/");

				escribirEstructura(i,str2); //Recursividad

			} else {
				if ((tablaArchivos[i].state == 1)&&(tablaArchivos[i].parent_directory==dirPadre)) { //Archivos en el directorio
					leerArchivo(i,ruta);
				}
			}
		}
	}
}

void leerArchivo(int archivoID, char* ruta){
	char* archivoNombre = tablaArchivos[archivoID].fname;
	int primerBloque = tablaArchivos[archivoID].first_block;
	int tamanioArchivo = tablaArchivos[archivoID].file_size;
	//printf("\nLeyendo el archivo %s \n", archivoNombre);

	//Obtengo el bloque de datos correspondiente
	int *block;
	block =(int *)malloc(tamanioArchivo * sizeof(int));
	//printf("\nTamaño del archivo %d \n", tamanioArchivo);

	int sum = 0;
	int i= 0;
	int bloque = primerBloque;
	//printf("\nPrimer bloque %d \n", bloque);

	while (bloque != -1) {
		if (tablaAsignaciones[bloque] != -1) {
			pthread_mutex_lock(&mutex);
			memcpy(&block[sum / sizeof(int)], &pmapFS[(inicioBloqueDatos + bloque) * OSADA_BLOCK_SIZE], OSADA_BLOCK_SIZE * sizeof(int));
			sum = sum + OSADA_BLOCK_SIZE;
			pthread_mutex_unlock(&mutex);
			//printf("\n Escribio %d", OSADA_BLOCK_SIZE);
		} else {
			pthread_mutex_lock(&mutex);
			memcpy(&block[sum / sizeof(int)], &pmapFS[(inicioBloqueDatos + bloque) * OSADA_BLOCK_SIZE], (tamanioArchivo - sum) * sizeof(int));
			//printf("\n ------Copia Parcial ------ %d", bloque);
			//printf("\n Escribio %d", (tamanioArchivo - sum));
			sum = sum + (tamanioArchivo - sum);
			pthread_mutex_unlock(&mutex);
		}

		//printf("\n Escribiendo bloque %d", inicioBloqueDatos + bloque);
		//printf("\n Cantidad Bytes restantes %d", (tamanioArchivo - sum));
		//printf("\n Siguiente bloque %d", tablaAsignaciones[bloque]);
		pthread_mutex_lock(&mutex);
		bloque = tablaAsignaciones[bloque];
		pthread_mutex_unlock(&mutex);
	}

	//Escribo el archivo obtenido
	FILE* pFile;
	char *str = string_new();
	string_append(&str, rutaOsadaDir);
	string_append(&str, ruta);
	string_append(&str, archivoNombre);

	pFile = fopen(str,"wb");
	fwrite(block, tamanioArchivo, 1, pFile);
	printf("Creando archivo %s \n", str);
	fclose(pFile);
	free(block);
	block=NULL;
}


void aceptarConexiones() {
	struct socket* mi_socket_s;

	int returnValue;
	int conectado;

	int error = -1;

	mi_socket_s = crearServidor(IP_SERVIDOR, PUERTO);
	if(mi_socket_s->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, mi_socket_s->error);
		exit(error);
	}

	returnValue = escucharConexiones(*mi_socket_s, BACKLOG);
	if(returnValue != 0)
	{
		log_info(logger, strerror(returnValue));
		eliminarSocket(mi_socket_s);
		exit(error);
	}

	conectado = 1;

	while(conectado) {
		struct socket* cli_socket_s;
		t_pokedex_cliente* pokedex_cliente;

		pokedex_cliente = malloc(sizeof(t_pokedex_cliente));

		log_info(logger, "Escuchando conexiones");
		cli_socket_s = aceptarConexion(*mi_socket_s);
		if(cli_socket_s->descriptor == 0)
		{
			log_info(logger, "Se rechaza conexión");
			log_info(logger, cli_socket_s->error);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			exit(error);
		}

		///////////////////////
		////// HANDSHAKE //////
		///////////////////////

		// Recibir mensaje CONEXION_POKEDEX_CLIENTE
		mensajeDePokedex mensajeConexionPokdexCliente;

		mensajeConexionPokdexCliente.tipoMensaje = CONEXION_POKEDEX_CLIENTE;

		recibirMensaje(cli_socket_s, &mensajeConexionPokdexCliente);
		if(cli_socket_s->error != NULL)
		{
			log_info(logger, cli_socket_s->error);
			eliminarSocket(cli_socket_s);
		}

		if(mensajeConexionPokdexCliente.tipoMensaje != CONEXION_POKEDEX_CLIENTE) {
			// Enviar mensaje RECHAZA_CONEXION
			paquete_t paquete;
			mensaje_t mensajeRechazaConexion;

			mensajeRechazaConexion.tipoMensaje = RECHAZA_CONEXION;
			crearPaquete((void*) &mensajeRechazaConexion, &paquete);
			if(paquete.tamanioPaquete == 0)
			{
				log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
				eliminarSocket(cli_socket_s);
				conectado = 0;
				eliminarSocket(mi_socket_s);
				exit(error);
			}

			enviarMensaje(cli_socket_s, paquete);
			if(cli_socket_s->error != NULL)
			{
				log_info(logger, cli_socket_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
				eliminarSocket(cli_socket_s);
				conectado = 0;
				eliminarSocket(mi_socket_s);
				exit(error);
			}

			free(paquete.paqueteSerializado);

			log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
			eliminarSocket(cli_socket_s);
		}

		log_info(logger, "Socket %d", cli_socket_s->descriptor);

		pokedex_cliente->socket = cli_socket_s;

		// Enviar mensaje ACEPTA_CONEXION
		paquete_t paquete2;
		mensaje_t mensajeAceptaConexion;

		mensajeAceptaConexion.tipoMensaje = ACEPTA_CONEXION;

		crearPaquete((void*) &mensajeAceptaConexion, &paquete2);
		if(paquete2.tamanioPaquete == 0)
		{
			log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex_cliente->socket->descriptor);
			eliminarSocket(pokedex_cliente->socket);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			exit(error);
		}

		enviarMensaje(pokedex_cliente->socket, paquete2);
		if(pokedex_cliente->socket->error != NULL)
		{
			log_info(logger, pokedex_cliente->socket->error);
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex_cliente->socket->descriptor);
			eliminarSocket(pokedex_cliente->socket);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			exit(error);
		}

		free(paquete2.paqueteSerializado);


		log_info(logger, "Se aceptó una conexión. Socket° %d.\n", pokedex_cliente->socket->descriptor);

		// CREACIÓN DEL HILO POKEDEXCLIENTE
		pthread_t hiloCliente;
		pthread_attr_t atributosHiloCliente;

		pthread_attr_init(&atributosHiloCliente);
		pthread_create(&hiloCliente, &atributosHiloCliente, (void*) pokedexCliente, (void*)pokedex_cliente);
		pthread_attr_destroy(&atributosHiloCliente);



	}
}

void pokedexCliente(t_pokedex_cliente* pokedex_cliente) {
	while (1) {
		operacionOSADA* operacionEjecutar = malloc(sizeof(operacionOSADA));

		void* mensajeRespuesta = malloc(TAMANIO_MAXIMO_MENSAJE);
		((mensaje_t*) mensajeRespuesta)->tipoMensaje = INDEFINIDO;

		recibirMensaje(pokedex_cliente->socket, mensajeRespuesta);

		if(pokedex_cliente->socket->error != NULL)
		{
			free(mensajeRespuesta);
			log_info(logger, pokedex_cliente->socket->error);
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex_cliente->socket->descriptor);
			//TODO Cerrar todos los sockets y salir
			eliminarSocket(pokedex_cliente->socket);
		}
		((operacionOSADA*) operacionEjecutar)->operacion = mensajeRespuesta;
		((operacionOSADA*) operacionEjecutar)->socket = pokedex_cliente;

		//SE AGREGA LA OPERACION PENDIENTE
		pthread_mutex_lock(&mutex);
		queue_push(colaOperaciones, operacionEjecutar);
		pthread_mutex_unlock(&mutex);
	}
}
