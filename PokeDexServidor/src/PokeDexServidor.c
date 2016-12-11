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
#include <string.h>
#include <commons/string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <commons/log.h>
#include "socket.h" // BORRAR
#include "PokeDexServidor.h"
#include "osada.h"
#include <commons/bitarray.h>
#include <sys/mman.h>
#include <commons/collections/queue.h>
#include <semaphore.h>

#include "protocoloPokedexClienteServidor.h"

#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola
#define TAMANIO_MAXIMO_MENSAJE 50 // Tamaño máximo de un mensaje
#define IP_SERVIDOR "0.0.0.0"
#define PUERTO "6543"
#define CANTIDAD_MAXIMA_CONEXIONES 1000
#define TABLA_ARCHIVOS 2048
#define LOG_FILE_PATH "PokeDexServidor.log"

struct socket** clientes;

osada_file tablaArchivos[2048];		// TABLA DE ARCHIVOS
unsigned int * tablaAsignaciones;	// TABLA DE ASIGNACIONES
t_bitarray * bitarray;				// ARRAY BITMAP
char* pmapFS; 						//Puntero de mmap
int inicioBloqueDatos;				//INICIO BLOQUE DE DATOS
int ultimoBloqueEncontrado;

pthread_mutex_t mutex;				//SEMAFORO PARA SINCRONIZAR OPERACIONES
pthread_mutex_t mutexReady;
pthread_mutex_t mutexOper;
sem_t semReady;


/* Logger */
t_log* logger;
t_queue* colaOperaciones;

int main(void) {
	// Variables para el manejo del FileSystem OSADA
	int fileFS;
	struct stat statFS;
	osada_header cabeceraFS;		// HEADER
	char * bitmapS;					// BITMAP
	int bloquesTablaAsignaciones;	// CANTIDAD DE BLOQUES DE LA TABLA DE ASIGNACIONES
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&mutexReady, NULL);
	pthread_mutex_init(&mutexOper, NULL);
	sem_init(&semReady, 0, 0);

	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;

	//Carga variable de entorno del archivo .bin
	const char* rutaFS = getenv("osadaFile");

	logger = log_create(LOG_FILE_PATH, "POKEDEX_SERVIDOR", true, LOG_LEVEL_INFO);
	log_info(logger,"Cargando archivo: %s", rutaFS);

	// CARGA DE FS EN MEMORIA CON MMAP
	fileFS = open(rutaFS,O_RDWR);
	stat(rutaFS,&statFS);
	pmapFS = (char*)mmap(0, statFS.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileFS, 0); //puntero al primer byte del FS (array de bytes)
	log_info(logger,"MMAP: Tamaño del FS es %d ", (int)statFS.st_size);

	memcpy(&cabeceraFS, &pmapFS[0], sizeof(cabeceraFS));
	// Valida que sea un FS OSADA
	if (memcmp(cabeceraFS.magic_number, "OsadaFS", 7) != 0)	{
		log_info(logger,"NO es un FS Osada");
		return EXIT_FAILURE;
	}


	// INFORMACION HEADER
	log_info(logger,"-----	HEADER	-----");
	log_info(logger,"Identificador: %s", cabeceraFS.magic_number);
	log_info(logger,"Version:       %d", cabeceraFS.version);
	log_info(logger,"Tamano FS:     %d (bloques)", cabeceraFS.fs_blocks);
	log_info(logger,"Tamano Bitmap: %d (bloques)", cabeceraFS.bitmap_blocks);
	log_info(logger,"Inicio T.Asig: %d (bloque)", cabeceraFS.allocations_table_offset);
	log_info(logger,"Tamano Datos:  %d (bloques)", cabeceraFS.data_blocks);
	log_info(logger,"Relleno: %s", cabeceraFS.padding);


	// Aloca el espacio en memoria para el Bitmap
	bitmapS=malloc(cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);
	if (bitmapS== NULL)	{
		log_info(logger,"No se dispone de memoria para alocar el Bitmap");
		return EXIT_FAILURE;
	}

	// Lee el BITMAP
	memcpy(bitmapS, &pmapFS[1 * OSADA_BLOCK_SIZE], cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// Crea el array de bits
	bitarray = bitarray_create(bitmapS,cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);
	log_info(logger,"Cargando Bitmap");


	// Lee tabla de archivos
	memcpy(&tablaArchivos, &pmapFS[(1 + cabeceraFS.bitmap_blocks) * OSADA_BLOCK_SIZE], 1024 * OSADA_BLOCK_SIZE);

	log_info(logger,"Cargando Tabla de Archivos");

	/*log_info(logger,"---------------------------");
	log_info(logger,"	TABLA DE ARCHIVOS	");
	log_info(logger,"---------------------------");
	log_info(logger,"%s", "|    Pos    	|    State    	|    File Name    	|    Parent Dir |    Size    	|    lastModif    	|    First Block");
	log_info(logger,"%s", "--------------------------------------------------------------------------------------------------------");
	for (i = 0; i < TABLA_ARCHIVOS; i++)
	{
		if (tablaArchivos[i].state != 0)
		{
			log_info(logger,"|    %d    	", i);
			log_info(logger,"|    %d    	", tablaArchivos[i].state);
			log_info(logger,"|    %s    	", tablaArchivos[i].fname);
			log_info(logger,"|    %d    	", tablaArchivos[i].parent_directory);
			log_info(logger,"|    %d    	", tablaArchivos[i].file_size);
			log_info(logger,"|    %d    	", tablaArchivos[i].lastmod);
			log_info(logger,"|    %d    	", tablaArchivos[i].first_block);
			log_info(logger,"");
		}
	}*/


	// BLOQUE DE DATOS
	/*log_info(logger,"---------------------------");
	log_info(logger,"	BLOQUE DE DATOS	");
	log_info(logger,"---------------------------");*/
	inicioBloqueDatos = cabeceraFS.fs_blocks - cabeceraFS.data_blocks;
	ultimoBloqueEncontrado = inicioBloqueDatos;
	log_info(logger,"Inicio bloques de datos: %d (bloque)", inicioBloqueDatos);


	// INFORMACION BITMAP
	/*
	log_info(logger,"---------------------------");
	log_info(logger,"	BITMAP	");
	log_info(logger,"---------------------------");
	log_info(logger,"Pos %d = %d", inicioBloqueDatos - 4, bitarray_test_bit(bitarray, inicioBloqueDatos - 4));
	log_info(logger,"Pos %d = %d", inicioBloqueDatos - 3, bitarray_test_bit(bitarray, inicioBloqueDatos - 3));
	log_info(logger,"Pos %d = %d", inicioBloqueDatos - 2, bitarray_test_bit(bitarray, inicioBloqueDatos - 2));
	log_info(logger,"Pos %d = %d", inicioBloqueDatos - 1, bitarray_test_bit(bitarray, inicioBloqueDatos - 1));
	log_info(logger,"Pos %d = %d", inicioBloqueDatos, 	bitarray_test_bit(bitarray, inicioBloqueDatos));
	log_info(logger,"Pos %d = %d", inicioBloqueDatos + 1, bitarray_test_bit(bitarray, inicioBloqueDatos + 1));
	log_info(logger,"Pos %d = %d", bitarray_get_max_bit(bitarray), bitarray_test_bit(bitarray, bitarray_get_max_bit(bitarray)));
	 */


	// Calculo bloques
	bloquesTablaAsignaciones = cabeceraFS.fs_blocks - 1 - cabeceraFS.bitmap_blocks - 1024 - cabeceraFS.data_blocks;

	//Reserva espacio tabla de asignaciones
	tablaAsignaciones=malloc(bloquesTablaAsignaciones * OSADA_BLOCK_SIZE);
	if (tablaAsignaciones== NULL) {
		log_info(logger,"No se dispone de memoria para alocar la Tabla de Asignaciones");
		return EXIT_FAILURE;
	}

	// Lee la tabla de ASIGNACIONES
	memcpy(tablaAsignaciones, &pmapFS[(1 + cabeceraFS.bitmap_blocks + 1024) * OSADA_BLOCK_SIZE], bloquesTablaAsignaciones * OSADA_BLOCK_SIZE);


	// INFORMACION TABLA DE ASIGNACIONES
	/*log_info(logger,"---------------------------");
	log_info(logger,"	TABLA DE ASIGNACIONES	");
	log_info(logger,"---------------------------");*/
	log_info(logger,"Tamano Tabla de Asignaciones: %d (bloques)", bloquesTablaAsignaciones);

	// CREACIÓN DEL HILO EN ESCUCHA
	pthread_attr_init(&atributosHilo);
	pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);

	colaOperaciones = queue_create();

	while(1) {
		sem_wait(&semReady);
		if(queue_size(colaOperaciones)> 0)
		{
			operacionOSADA *operacionActual;
			socket_t* socketPokedex;

			void* mensajeRespuesta;

			pthread_mutex_lock(&mutex);
			operacionActual = queue_pop(colaOperaciones);
			pthread_mutex_unlock(&mutex);

			mensajeRespuesta = ((operacionOSADA*) operacionActual)->operacion;
			socketPokedex = ((operacionOSADA*) operacionActual)->socket->socket;
			switch(((mensaje_t*) mensajeRespuesta)->tipoMensaje) {
			case READDIR:
				log_info(logger, "Solicito READDIR del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);
				char* cadenaMensaje = readdir_callback(((mensaje1_t*) mensajeRespuesta)->path);

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
				free(paqueteLectura.paqueteSerializado);
				break;
			case GETATTR: ;
			log_info(logger, "Solicito GETATTR del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);
			t_getattr getattr = getattr_callback(((mensaje1_t*) mensajeRespuesta)->path);

			// Enviar mensaje GETATTR
			paquete_t paqueteGetAttr;
			mensaje3_t mensajeGETATTR_RESPONSE;

			mensajeGETATTR_RESPONSE.tipoMensaje = GETATTR_RESPONSE;
			mensajeGETATTR_RESPONSE.tipoArchivo = getattr.tipoArchivo;
			mensajeGETATTR_RESPONSE.tamanioArchivo = getattr.tamanioArchivo;
			mensajeGETATTR_RESPONSE.lastModif = getattr.lastModif;


			crearPaquete((void*) &mensajeGETATTR_RESPONSE, &paqueteGetAttr);
			if(paqueteGetAttr.tamanioPaquete == 0) {
				socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, socketPokedex->error);
				log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
				exit(EXIT_FAILURE);
			}

			enviarMensaje(socketPokedex, paqueteGetAttr);
			free(paqueteGetAttr.paqueteSerializado);
			break;
			case READ:
				log_info(logger, "Solicito READ del path: %s Cantidad de bytes: %d OFFSET: %d", ((mensaje4_t*) mensajeRespuesta)->path, ((mensaje4_t*) mensajeRespuesta)->tamanioBuffer, ((mensaje4_t*) mensajeRespuesta)->offset);

				// Enviar mensaje READ
				paquete_t paqueteREAD;
				mensaje5_t mensajeREAD_RESPONSE;
				t_block READ_RES;

				READ_RES = read_callback(((mensaje4_t*) mensajeRespuesta)->path,((mensaje4_t*) mensajeRespuesta)->offset,((mensaje4_t*) mensajeRespuesta)->tamanioBuffer);

				mensajeREAD_RESPONSE.tipoMensaje = READ_RESPONSE;
				mensajeREAD_RESPONSE.buffer = READ_RES.block;
				mensajeREAD_RESPONSE.tamanioBuffer = READ_RES.size;

				//log_info(logger, "READRESPONSE: Cantidad de bytes: %d, buffer: ", mensajeREAD_RESPONSE.tamanioBuffer, mensajeREAD_RESPONSE.buffer);

				crearPaquete((void*) &mensajeREAD_RESPONSE, &paqueteREAD);
				if(paqueteGetAttr.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteREAD);
				free(paqueteREAD.paqueteSerializado);
				free(READ_RES.block);
				break;
			case MKDIR:
				log_info(logger, "Solicito MKDIR del path: %s", ((mensaje6_t*) mensajeRespuesta)->path);

				// Enviar mensaje MKDIR

				paquete_t paqueteMKDIR;
				mensaje7_t mensajeMKDIR_RESPONSE;

				mensajeMKDIR_RESPONSE.tipoMensaje = MKDIR_RESPONSE;
				mensajeMKDIR_RESPONSE.res = mkdir_callback(((mensaje6_t*) mensajeRespuesta)->path);

				crearPaquete((void*) &mensajeMKDIR_RESPONSE, &paqueteMKDIR);
				if(paqueteGetAttr.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteMKDIR);
				free(paqueteMKDIR.paqueteSerializado);

				break;
			case RMDIR:
				log_info(logger, "Solicito RMDIR del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);

				// Enviar mensaje RMDIR

				paquete_t paqueteRMDIR;
				mensaje7_t mensajeRMDIR_RESPONSE;

				mensajeRMDIR_RESPONSE.tipoMensaje = RMDIR_RESPONSE;
				mensajeRMDIR_RESPONSE.res = rmdir_callback(((mensaje1_t*) mensajeRespuesta)->path);

				crearPaquete((void*) &mensajeRMDIR_RESPONSE, &paqueteRMDIR);
				if(paqueteGetAttr.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteRMDIR);

				break;
			case UNLINK:
				log_info(logger, "Solicito UNLINK del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);

				// Enviar mensaje RMDIR

				paquete_t paqueteUNLINK;
				mensaje7_t mensajeUNLINK_RESPONSE;

				mensajeUNLINK_RESPONSE.tipoMensaje = UNLINK_RESPONSE;
				mensajeUNLINK_RESPONSE.res = unlink_callback(((mensaje1_t*) mensajeRespuesta)->path);

				crearPaquete((void*) &mensajeUNLINK_RESPONSE, &paqueteUNLINK);
				if(paqueteUNLINK.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteUNLINK);
				free(paqueteUNLINK.paqueteSerializado);

				break;
			case MKNOD:
				log_info(logger, "Solicito MKNOD del path: %s", ((mensaje1_t*) mensajeRespuesta)->path);

				// Enviar mensaje MKNOD

				paquete_t paqueteMKNOD;
				mensaje7_t mensajeMKNOD_RESPONSE;

				mensajeMKNOD_RESPONSE.tipoMensaje = MKNOD_RESPONSE;
				mensajeMKNOD_RESPONSE.res = mknod_callback(((mensaje1_t*) mensajeRespuesta)->path);

				//log_info(logger, "MKNOD res: %d", mensajeMKNOD_RESPONSE.res);

				crearPaquete((void*) &mensajeMKNOD_RESPONSE, &paqueteMKNOD);
				if(paqueteMKNOD.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteMKNOD);
				free(paqueteMKNOD.paqueteSerializado);

				break;
			case WRITE:
				log_info(logger, "Solicito WRITE del path: %s Cantidad de bytes: %d OFFSET: %d", ((mensaje8_t*) mensajeRespuesta)->path, ((mensaje8_t*) mensajeRespuesta)->tamanioBuffer, ((mensaje8_t*) mensajeRespuesta)->offset);

				// Enviar mensaje respuesta

				paquete_t paqueteWRITE;
				mensaje7_t mensajeWRITE_RESPONSE;

				int res = write_callback(((mensaje8_t*) mensajeRespuesta)->path, ((mensaje8_t*) mensajeRespuesta)->offset, ((mensaje8_t*) mensajeRespuesta)->buffer, ((mensaje8_t*) mensajeRespuesta)->tamanioBuffer);
				mensajeWRITE_RESPONSE.tipoMensaje = WRITE_RESPONSE;
				mensajeWRITE_RESPONSE.res = res;


				crearPaquete((void*) &mensajeWRITE_RESPONSE, &paqueteWRITE);
				if(paqueteWRITE.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteWRITE);
				free(paqueteWRITE.paqueteSerializado);
				free(((mensaje8_t*) mensajeRespuesta)->buffer);
				free(((mensaje8_t*) mensajeRespuesta)->path);
				break;
			case RENAME:
				log_info(logger, "Solicito RENAME FROM: %s TO: %s", ((mensaje9_t*) mensajeRespuesta)->pathFrom, ((mensaje9_t*) mensajeRespuesta)->pathTo);

				// Enviar mensaje RENAME
				paquete_t paqueteRENAME;
				mensaje7_t mensajeRENAME_RESPONSE;


				mensajeRENAME_RESPONSE.tipoMensaje = RENAME_RESPONSE;
				mensajeRENAME_RESPONSE.res = rename_callback(((mensaje9_t*) mensajeRespuesta)->pathFrom,((mensaje9_t*) mensajeRespuesta)->pathTo);

				//log_info(logger, "RENAMERESPONSE: %d ", mensajeRENAME_RESPONSE.res);

				crearPaquete((void*) &mensajeRENAME_RESPONSE, &paqueteRENAME);
				if(paqueteGetAttr.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteRENAME);
				free(paqueteRENAME.paqueteSerializado);
				break;
			case TRUNCATE:
				log_info(logger, "Solicito TRUNCATE PATH: %s SIZE: %d", ((mensaje10_t*) mensajeRespuesta)->path, ((mensaje10_t*) mensajeRespuesta)->size);

				// Enviar mensaje TRUNCATE
				paquete_t paqueteTRUNCATE;
				mensaje7_t mensajeTRUNCATE_RESPONSE;


				mensajeTRUNCATE_RESPONSE.tipoMensaje = TRUNCATE_RESPONSE;
				mensajeTRUNCATE_RESPONSE.res = truncate_callback(((mensaje10_t*) mensajeRespuesta)->path, ((mensaje10_t*) mensajeRespuesta)->size);

				//log_info(logger, "TRUNCATE_RESPONSE: %d ", mensajeTRUNCATE_RESPONSE.res);

				crearPaquete((void*) &mensajeTRUNCATE_RESPONSE, &paqueteTRUNCATE);
				if(paqueteGetAttr.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteTRUNCATE);
				free(paqueteTRUNCATE.paqueteSerializado);
				break;
			case UTIMENS:
				log_info(logger, "Solicito UTIMENS PATH: %s TIME: %d", ((mensaje10_t*) mensajeRespuesta)->path, ((mensaje10_t*) mensajeRespuesta)->size);

				// Enviar mensaje UTIMENS
				paquete_t paqueteUTIMENS;
				mensaje7_t mensajeUTIMENS_RESPONSE;


				mensajeUTIMENS_RESPONSE.tipoMensaje = UTIMENS_RESPONSE;
				mensajeUTIMENS_RESPONSE.res = utimens_callback(((mensaje10_t*) mensajeRespuesta)->path, ((mensaje10_t*) mensajeRespuesta)->size);


				crearPaquete((void*) &mensajeUTIMENS_RESPONSE, &paqueteUTIMENS);
				if(paqueteGetAttr.tamanioPaquete == 0) {
					socketPokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, socketPokedex->error);
					log_info(logger, "Conexión mediante socket %d finalizada", socketPokedex->descriptor);
					exit(EXIT_FAILURE);
				}

				enviarMensaje(socketPokedex, paqueteUTIMENS);
				free(paqueteUTIMENS.paqueteSerializado);
				break;

			}
			free(operacionActual->operacion);
			free(operacionActual);
			//Luego de cada operaciones sinconizo los datos a disco
			// Copio el BITMAP a memoria
			memcpy(&pmapFS[1 * OSADA_BLOCK_SIZE], bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);
			// Copio tabla de archivos a memoria
			memcpy(&pmapFS[(1 + cabeceraFS.bitmap_blocks) * OSADA_BLOCK_SIZE], &tablaArchivos, 1024 * OSADA_BLOCK_SIZE);
			// Copio tabla de ASIGNACIONES a memoria
			memcpy(&pmapFS[(1 + cabeceraFS.bitmap_blocks + 1024) * OSADA_BLOCK_SIZE], tablaAsignaciones, bloquesTablaAsignaciones * OSADA_BLOCK_SIZE);
			msync(pmapFS, statFS.st_size, 2); //Sincronizo la memoria a disco

		}
	}


	////////////////////
	///// SHUTDOWN /////
	////////////////////

	// Copio el BITMAP a memoria
	memcpy(&pmapFS[1 * OSADA_BLOCK_SIZE], bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// Copio tabla de archivos a memoria
	memcpy(&pmapFS[(1 + cabeceraFS.bitmap_blocks) * OSADA_BLOCK_SIZE], &tablaArchivos, 1024 * OSADA_BLOCK_SIZE);

	// Copio tabla de ASIGNACIONES a memoria
	memcpy(&pmapFS[(1 + cabeceraFS.bitmap_blocks + 1024) * OSADA_BLOCK_SIZE], tablaAsignaciones, bloquesTablaAsignaciones * OSADA_BLOCK_SIZE);

	msync(pmapFS, statFS.st_size, 2); //Sincronizo el la memoria a disco

	munmap (pmapFS, statFS.st_size); //Bajo el FS de la memoria

	close(fileFS); //Cierro el archivo

	free(bitmapS);
	free(tablaAsignaciones);
	bitmapS=NULL;
	tablaAsignaciones=NULL;

	return EXIT_SUCCESS;
}





int getFirstBit() {
	pthread_mutex_lock(&mutexOper);
	int j = bitarray_get_max_bit(bitarray);
	int i;
	for (i = ultimoBloqueEncontrado; i < j; i++) {
		if (bitarray_test_bit(bitarray, i) == 0) {
			bitarray_set_bit(bitarray, i);
			ultimoBloqueEncontrado = i;
			pthread_mutex_unlock(&mutexOper);
			return i;
		}
	}
	for (i = inicioBloqueDatos; i < j; i++) {
		if (bitarray_test_bit(bitarray, i) == 0) {
			bitarray_set_bit(bitarray, i);
			ultimoBloqueEncontrado = i;
			pthread_mutex_unlock(&mutexOper);
			return i;
		}
	}
	log_info(logger, "Disco lleno");
	pthread_mutex_unlock(&mutexOper);
	return -2;
}

int utimens_callback(const char *path, int time) {

	int archivoID = getDirPadre(path);

	if (archivoID != -1) {
		tablaArchivos[archivoID].lastmod = time;
	}
	return archivoID;
}

int truncate_callback(const char *path, int size) {

	int archivoID = getDirPadre(path);

	if (archivoID != -1) {
		int primerBloque = tablaArchivos[archivoID].first_block;

		int sumOffset = 0;
		int sumSize = 0;
		int bloque = primerBloque;
		int bloqueAnterior = bloque;

		if (size > tablaArchivos[archivoID].file_size) {
			//Movimientos hasta el size
			while (sumSize<tablaArchivos[archivoID].file_size) {
				bloqueAnterior = bloque;
				bloque = tablaAsignaciones[bloque];
				sumSize = sumSize + OSADA_BLOCK_SIZE;
			}

			//Agrego bloques faltantes
			while (size > sumSize) {
				int nuevoBloque = getFirstBit();
				if (nuevoBloque == -2) { //Disco lleno
					tablaArchivos[archivoID].file_size = sumSize;
					//log_info(logger, "Salio del truncate por disco lleno, size: %d ", tablaArchivos[archivoID].file_size);
					return -2;
				} else {
					bloqueAnterior = bloque;
					bloque = nuevoBloque - inicioBloqueDatos;

					if (tablaArchivos[archivoID].first_block == -1) { //Archivo vacio
						tablaArchivos[archivoID].first_block = bloque;
						//log_info(logger, "Primer bloque: %d ", bloque);
					} else {
						tablaAsignaciones[bloqueAnterior] = bloque;
						//log_info(logger, "bloqueAnterior %d bloque: %d ", bloqueAnterior,bloque);
					}
					tablaAsignaciones[bloque] = -1;
					sumSize = sumSize + OSADA_BLOCK_SIZE;
				}
			}
		} else {

			//Borro bloques
			//Movimientos hasta el sizebloque
			while (sumOffset<size) {
				bloqueAnterior = bloque;
				bloque = tablaAsignaciones[bloque];
				sumOffset = sumOffset + OSADA_BLOCK_SIZE;
			}

			while (bloque != -1) {
				//Limpio el bitmap
				bitarray_clean_bit(bitarray, bloque + inicioBloqueDatos);
				//log_info(logger," Limpiando bitarray %d", bloque);

				bloqueAnterior = bloque;
				bloque = tablaAsignaciones[bloque];

				//Limpio tabla de asignaciones
				tablaAsignaciones[bloqueAnterior] = -1;
				//log_info(logger,"Limpiando bloque: %d /n", bloqueAnterior);
			}

			if (size==0) {
				tablaArchivos[archivoID].first_block = -1;
			}
		}
	}
	tablaArchivos[archivoID].file_size = size;
	return archivoID;
}

int rename_callback(const char *from, const char *to) {
	char** array;
	int i = 0;
	int res = -1;
	int dirPadre = 65535;
	if (!(string_equals_ignore_case((char *)to, "/"))) {
		array = string_split((char *)to, "/");
		while (array[i+1]) {
			res = buscarTablaAchivos(dirPadre,array[i]);
			dirPadre = res;
			i++;
		}
	} else {
		res = dirPadre;
	}

	int archivoID = getDirPadre(from);

	int n = string_length((char *)array[i]);
	if (n > 17) {
		return -2; }

	if (archivoID != -1) {
		tablaArchivos[archivoID].parent_directory=res;
		strcpy((char *)tablaArchivos[archivoID].fname, (const char *)array[i]);
		tablaArchivos[archivoID].lastmod = (int)time(NULL);
	}

	int j = 0;
	while (array[j]) {
		free(array[j]);
			j++;
	}

	return archivoID;
}

int write_callback(const char* path, int offset, char* buffer, int tamanioBuffer){
	int archivoID = getDirPadre(path);
	int primerBloque = tablaArchivos[archivoID].first_block;

	int sum = 0;
	int sumOffset = 0;
	int bloque = primerBloque;
	int bloqueAnterior;

	//Movimientos hasta el offset
	while (sumOffset<offset) {
		bloqueAnterior = bloque;
		bloque = tablaAsignaciones[bloque];
		sumOffset = sumOffset + OSADA_BLOCK_SIZE;
	}


	//Si estamos en el final del archivo
	//Si es un archivo vacio
	if (bloque == -1) {
		bloque = getFirstBit() - inicioBloqueDatos;
		if (bloque == -2) { //Disco lleno
			return -2;
		} else {
			if (tablaArchivos[archivoID].first_block == -1) {
				tablaArchivos[archivoID].first_block = bloque;
			} else {
				tablaAsignaciones[bloqueAnterior] = bloque;
			}
		}
	}

	while (sum<tamanioBuffer) {
		if (tamanioBuffer - sum > OSADA_BLOCK_SIZE) {
			pthread_mutex_lock(&mutexOper);
			memcpy(&pmapFS[(inicioBloqueDatos + bloque) * OSADA_BLOCK_SIZE], &buffer[sum / sizeof(char)], OSADA_BLOCK_SIZE * sizeof(char));

			sum = sum + OSADA_BLOCK_SIZE;
			pthread_mutex_unlock(&mutexOper);
			//log_info(logger," Escribio %d", OSADA_BLOCK_SIZE);
		} else {
			pthread_mutex_lock(&mutexOper);
			memcpy(&pmapFS[(inicioBloqueDatos + bloque) * OSADA_BLOCK_SIZE], &buffer[sum / sizeof(char)], (tamanioBuffer - sum) * sizeof(char));
			//log_info(logger," ------Copia Parcial ------ %d", bloque);
			//log_info(logger," Escribio %d", (tamanioBuffer - sum));
			sum = sum + (tamanioBuffer - sum);
			pthread_mutex_unlock(&mutexOper);
		}

		int bloqueAnterior = bloque;

		//Actualizo tabla de asignaciones si corresponde
		if (sum<tamanioBuffer) {
			if (tablaAsignaciones[bloqueAnterior] != -1) {
				bloque = tablaAsignaciones[bloqueAnterior];
			} else { //Busco un nuevo bloque si corresponde
				int nuevoBloque = getFirstBit();
				if (nuevoBloque == -2) { //Disco lleno
					tablaArchivos[archivoID].file_size = sum + offset;
					return -2;
				} else {
					bloque = nuevoBloque - inicioBloqueDatos;
					tablaAsignaciones[bloqueAnterior] = bloque;
					tablaAsignaciones[bloque] = -1;
				}
			}
		} else { //Finalizo la escritura
			tablaAsignaciones[bloqueAnterior] = -1;
		}
	}

	//Asigno nuevo tamaño
	tablaArchivos[archivoID].file_size = sum + offset;
	tablaArchivos[archivoID].lastmod = (int)time(NULL);

	return 1;
}

char* readdir_callback(const char *path) {
	char *cadenaMensaje = string_new();
	int i;
	int dirPadre = getDirPadre(path);
	for (i = 0; i < TABLA_ARCHIVOS; i++) {
		if (tablaArchivos[i].state != 0) {
			if ((tablaArchivos[i].state == 2)&&(tablaArchivos[i].parent_directory==dirPadre)) { //Directorios en el directorio
				string_append(&cadenaMensaje, (char *)tablaArchivos[i].fname);
				string_append(&cadenaMensaje, "/");
			} else {
				if ((tablaArchivos[i].state == 1)&&(tablaArchivos[i].parent_directory==dirPadre)) { //Archivos en el directorio
					string_append(&cadenaMensaje, (char *)tablaArchivos[i].fname);
					string_append(&cadenaMensaje, "/");
				}
			}
		}
	}
	return cadenaMensaje;
}

int mkdir_callback(const char *path) {
	char** array;
	int i = 0;
	int res = -1;
	int dirPadre = 65535;
	if (!(string_equals_ignore_case((char *)path, "/"))) {
		array = string_split((char *)path, "/");
		while (array[i+1]) {
			res = buscarTablaAchivos(dirPadre,array[i]);
			dirPadre = res;
			i++;
		}
	} else {
		res = dirPadre;
	}

	int id = get_firstEntry();

	if (id == -1) {
		return -1; }

	int n = string_length(array[i]);
	log_info(logger,"Size of name: %d", n);
	if (n > 17) {
		return -2; }
	osada_file newDir;
	strcpy((char *)newDir.fname, array[i]);
	newDir.state = 2;
	newDir.parent_directory = res;
	newDir.file_size = 0;
	newDir.first_block = -1;
	newDir.lastmod = (int)time(NULL);
	tablaArchivos[id] = newDir;

	int j = 0;
	while (array[j]) {
		free(array[j]);
			j++;
	}

	return id;
}

int mknod_callback(const char *path) {
	char** array;
	int i = 0;
	int res = -1;
	int dirPadre = 65535;
	if (!(string_equals_ignore_case((char *)path, "/"))) {
		array = string_split((char *)path, "/");
		while (array[i+1]) {
			res = buscarTablaAchivos(dirPadre,array[i]);
			dirPadre = res;
			i++;
		}
	} else {
		res = dirPadre;
	}

	int id = get_firstEntry();
	if (id == -1) {
		return -1; }

	int n = string_length(array[i]);
	log_info(logger,"Size of name: %d", n);
	if (n > 17) {
		return -2; }

	osada_file newDir;
	strcpy((char*)newDir.fname, (const char *)array[i]);
	newDir.state = 1;
	newDir.parent_directory = res;
	newDir.file_size = 0;
	newDir.first_block = -1;
	newDir.lastmod = (int)time(NULL);
	tablaArchivos[id] = newDir;

	int j = 0;
	while (array[j]) {
		free(array[j]);
			j++;
	}

	return id;
}

int rmdir_callback(const char *path) {
	int archivoID = getDirPadre(path);

	if (archivoID != -1) {
		tablaArchivos[archivoID].state = 0;
	}

	return archivoID;
}

int unlink_callback(const char *path) {

	int archivoID = getDirPadre(path);

	if (archivoID != -1) {
		int primerBloque = tablaArchivos[archivoID].first_block;
		int bloque = primerBloque;
		int bloqueAnterior = bloque;

		while (bloque != -1) {
			//Limpio el bitmap
			bitarray_clean_bit(bitarray, bloque + inicioBloqueDatos);
			//log_info(logger," Limpiando bitarray %d", bloque);

			bloqueAnterior = bloque;
			bloque = tablaAsignaciones[bloque];

			//Limpio tabla de asignaciones
			tablaAsignaciones[bloqueAnterior] = -1;
		}

		//Limpio tabla de archivos
		tablaArchivos[archivoID].state = 0;
	}

	return archivoID;
}

int get_firstEntry() {
	int i;
	for (i = 0; i < TABLA_ARCHIVOS; i++) {
		if ((tablaArchivos[i].state != 1)&&(tablaArchivos[i].state != 2)) {
			return i;
		}
	}
	return -1;
}

int getDirPadre(const char *path) {
	char** array;
	int i = 0;
	int res = -1;
	int dirPadre = 65535;
	if (!(string_equals_ignore_case((char *)path, "/"))) {
		array = string_split((char *)path, "/");
		while (array[i]) {
			res = buscarTablaAchivos(dirPadre,array[i]);
			dirPadre = res;
			i++;
		}
	} else {
		res = dirPadre; //Estamos en el root
	}

	int j = 0;
	while (j<i) {
		free(array[j]);
			j++;
	}

	return res;
}

t_getattr getattr_callback(const char *path) {
	t_getattr res;
	res.tipoArchivo = 0;
	res.tamanioArchivo = 0;
	if (!(string_equals_ignore_case((char *)path, "/"))) {
		int id = getDirPadre(path);
		if (tablaArchivos[id].state == 1) {
			res.tipoArchivo = 1;
			res.tamanioArchivo = tablaArchivos[id].file_size;
			res.lastModif = tablaArchivos[id].lastmod;
		}
		if (tablaArchivos[id].state == 2) {
			res.tipoArchivo = 2;
			res.lastModif = tablaArchivos[id].lastmod;
		}
	} else {
		res.tipoArchivo = 2;
	}
	return res;
}

t_block read_callback(const char *path, int offset, int tamanioBuffer){
	int archivoID = getDirPadre(path);
	int primerBloque = tablaArchivos[archivoID].first_block;
	int tamanioArchivo = tablaArchivos[archivoID].file_size;
	int limite;
	t_block res;
	//Obtengo el bloque de datos correspondiente
	int* block;
	if (tamanioArchivo-offset<tamanioBuffer) {
		limite = tamanioArchivo-offset;
	} else {
		limite = tamanioBuffer;
	}

	block = malloc(limite * sizeof(int));
	int sum = 0;
	int sumOffset = 0;
	int bloque = primerBloque;

	while (sumOffset<offset) {
		bloque = tablaAsignaciones[bloque];
		//log_info(logger," bloque offset %d", bloque);
		sumOffset = sumOffset + OSADA_BLOCK_SIZE;
	}

	//log_info(logger," sumOffset %d", sumOffset);
	//log_info(logger," tamanioBuffer %d", tamanioBuffer);

	while ((bloque != -1) && (sum<limite)) {
		if (tablaAsignaciones[bloque] != -1) {
			pthread_mutex_lock(&mutexOper);
			memcpy(&block[sum / sizeof(int)], &pmapFS[(inicioBloqueDatos + bloque) * OSADA_BLOCK_SIZE], OSADA_BLOCK_SIZE * sizeof(int));
			sum = sum + OSADA_BLOCK_SIZE;
			pthread_mutex_unlock(&mutexOper);
			//log_info(logger," Escribio %d", OSADA_BLOCK_SIZE);
		} else {
			pthread_mutex_lock(&mutexOper);
			memcpy(&block[sum / sizeof(int)], &pmapFS[(inicioBloqueDatos + bloque) * OSADA_BLOCK_SIZE], (limite - sum) * sizeof(int));
			//log_info(logger," ------Copia Parcial ------ %d", bloque);
			//log_info(logger," Escribio %d", (limite - sum));
			sum = sum + (limite - sum);
			pthread_mutex_unlock(&mutexOper);
		}

		//log_info(logger," Escribiendo bloque %d", bloque);
		//log_info(logger," Cantidad Bytes restantes %d", (limite - sum));
		//log_info(logger," Siguiente bloque %d", tablaAsignaciones[bloque]);
		pthread_mutex_lock(&mutexOper);
		bloque = tablaAsignaciones[bloque];
		pthread_mutex_unlock(&mutexOper);
	}

	//log_info(logger," sum %d", sum);
	res.block = (char*)block;
	res.size = limite;
	//free(block);
	return res;
}

int buscarTablaAchivos(int dirPadre, char* fname) {
	int i;
	int res = -1;
	for (i = 0; i < TABLA_ARCHIVOS; i++) {
		if (tablaArchivos[i].state != 0) {
			if ((tablaArchivos[i].state == 2)&&(tablaArchivos[i].parent_directory==dirPadre)&&(strcmp((const char *)tablaArchivos[i].fname, (const char *)fname) == 0)) { //Es un direcotrio
				res = i;

			} else {
				if ((tablaArchivos[i].state == 1)&&(tablaArchivos[i].parent_directory==dirPadre)&&(strcmp((const char *)tablaArchivos[i].fname, (const char *)fname) == 0)) { //Es un archivo
					res = i;
				}
			}
		}
	}
	return res;
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


		log_info(logger, "Se aceptó una conexión. Socket° %d.", pokedex_cliente->socket->descriptor);

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
			eliminarSocket(pokedex_cliente->socket);
		}
		((operacionOSADA*) operacionEjecutar)->operacion = mensajeRespuesta;
		((operacionOSADA*) operacionEjecutar)->socket = pokedex_cliente;

		//SE AGREGA LA OPERACION PENDIENTE
		pthread_mutex_lock(&mutex);
		queue_push(colaOperaciones, operacionEjecutar);
		pthread_mutex_unlock(&mutex);
		sem_post(&semReady);
	}
}
