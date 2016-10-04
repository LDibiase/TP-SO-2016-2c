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
//#include <Utility_Library/socket.h>
#include "socket.h" // BORRAR
#include "PokeDexServidor.h"
#include "osada.h"
#include <commons/bitarray.h>

#define IP "127.0.0.1"
#define PUERTO "8080"
#define CANTIDAD_MAXIMA_CONEXIONES 1000
#define TABLA_ARCHIVOS 2048

struct socket** clientes;

int main(void) {
	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;
	int idHilo;
	void* valorRetorno;

	// Variables para el manejo del FileSystem OSADA
	FILE *fileFS; 					// ARCHIVO
	osada_header cabeceraFS;		// HEADER
	t_bitarray * bitarray;			// ARRAY BITMAP
	unsigned char * bitmapS;		// BITMAP
	osada_file tablaArchivos[2048];	// TABLA DE ARCHIVOS
	unsigned int * tablaAsignaciones;		// TABLA DE ASIGNACIONES
	int bloquesTablaAsignaciones;	// CANTIDAD DE BLOQUES DE LA TABLA DE ASIGNACIONES

	unsigned int * aux;
	int i;
	int j;

	/*
	// Creación del hilo en escucha
	pthread_attr_init(&atributosHilo);
	idHilo = pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);

	if(idHilo == 0)
		pthread_join(hiloEnEscucha, &valorRetorno);
	*/

	// Abre el archivo de File System
	printf("\n---------------------------");
	printf("\n	INICIO	\n");
	printf("---------------------------\n");
	if ((fileFS=fopen("/home/utnso/workspace/tp-2016-2c-CodeTogether/basic.bin","r"))==NULL)
	{
		puts("Error al abrir el archivo de FS.\n");
		return EXIT_FAILURE;
	}

	// Lee la cabecera del archivo
	fread(&cabeceraFS, sizeof(cabeceraFS), 1, fileFS);

	printf("Leido Header %d\n", ftell(fileFS)); // POSICION

	// Valida que sea un FS OSADA
	if (memcmp(cabeceraFS.magic_number, "OsadaFS", 7) != 0)
	{
		puts("NO es un FS Osada\n");
		return EXIT_FAILURE;
	}

	// Aloca el espacio en memoria para el Bitmap
	bitmapS=(char *)malloc(cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// Verifica que se haya reservado el espacio
	if (bitmapS== NULL)
	{
		fclose(fileFS);
		puts("No se dispone de memoria para alocar el Bitmap\n");
		return EXIT_FAILURE;
	}

	// Lee el BITMAP
	fread(bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE, 1, fileFS);

	printf("Leido Bitmap %d\n", ftell(fileFS)); // POSICION

	// Crea el array de bits
	bitarray = bitarray_create(bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

/*
	// Imprime todos los bits
	j = bitarray_get_max_bit(bitarray);
	for (i = 0; i < j; i++)
		printf("%d = %d\n", i, bitarray_test_bit(bitarray, i));
*/

	// Lee la tabla de archivos
	fread(tablaArchivos, sizeof(osada_file), TABLA_ARCHIVOS, fileFS);

	printf("Leido Tabla de Archivos %d\n", ftell(fileFS)); // POSICION

	// Arma la estructura para la tabla de asignaciones
	bloquesTablaAsignaciones = cabeceraFS.fs_blocks - 1 - cabeceraFS.bitmap_blocks - 1024 - cabeceraFS.data_blocks;
	printf("Leido Tabla de Archivos %d\n", bloquesTablaAsignaciones); // POSICION
	tablaAsignaciones=(int *)malloc((bloquesTablaAsignaciones * OSADA_BLOCK_SIZE) / sizeof(int));

	// Verifica que se haya reservado el espacio
	if (tablaAsignaciones== NULL)
	{
		fclose(fileFS);
		puts("No se dispone de memoria para alocar la Tabla de Asignaciones\n");
		return EXIT_FAILURE;
	}

	// Lee la tabla de ASIGNACIONES
	//fread(tablaAsignaciones, sizeof(int), (bloquesTablaAsignaciones * OSADA_BLOCK_SIZE) / sizeof(int), fileFS);
	//printf("Leido Tabla Asignaciones %d\n", ftell(fileFS)); // POSICION
	printf("Leido Tabla Asignaciones PENDIENTE\n");

/*
	for (i = 0; i < bloquesTablaAsignaciones; i++)
		printf("%d = %d\n", i, tablaAsignaciones[i]);
*/
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

	// INFORMACION BITMAP
	printf("\n---------------------------");
	printf("\n	BITMAP	\n");
	printf("---------------------------\n");
	printf("Primer Bloque disponible: %d\n", cabeceraFS.fs_blocks - cabeceraFS.data_blocks);
	printf("Pos %d = %d\n", 0, bitarray_test_bit(bitarray, i));
	printf("Pos %d = %d\n", 2015, bitarray_test_bit(bitarray, 2015));
	printf("Pos %d = %d\n", 2016, bitarray_test_bit(bitarray, 2016));
	printf("Pos %d = %d\n", bitarray_get_max_bit(bitarray), bitarray_test_bit(bitarray, bitarray_get_max_bit(bitarray)));

	// INFORMACION TABLA DE ARCHIVOS
	printf("\n---------------------------");
	printf("\n	TABLA DE ARCHIVOS	\n");
	printf("---------------------------\n");
	printf("%s\n", "|    State    	|    File Name    	|    Parent Dir |    Size    	|    lastModif    	|    First Block");
	printf("%s\n", "--------------------------------------------------------------------------------------------------------");
	for (i = 0; i < TABLA_ARCHIVOS; i++)
	{
		if (tablaArchivos[i].state != 0)
		{
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
		printf("Inicio Datos: %d (bloque)\n", cabeceraFS.fs_blocks - cabeceraFS.data_blocks);

	// INFORMACION TABLA DE ASIGNACIONES
	printf("\n---------------------------");
	printf("\n	TABLA DE ASIGNACIONES	\n");
	printf("---------------------------\n");
	//printf("Tamano T.Asig: %d (bloques)\n", bloquesTablaAsignaciones);
	printf("Tamano T.Asig: PENDIENTE\n\n");

	free(bitmapS);
	bitmapS=NULL;

	fclose(fileFS);

	return EXIT_SUCCESS;
}

void aceptarConexiones() {
	struct socket* mi_socket_s;

	int returnValue;
	int conectado;
	int cantidadClientes;

	int error = -1;
	int exito = 0;

	mi_socket_s = crearServidor(IP, PUERTO);
	if(mi_socket_s->descriptor == 0)
	{
		puts("Conexión fallida");
		puts(mi_socket_s->error);
		pthread_exit(&error);
	}

	returnValue = escucharConexiones(*mi_socket_s, CANTIDAD_MAXIMA_CONEXIONES);
	if(returnValue != 0)
	{
		puts(strerror(returnValue));
		pthread_exit(&error);
	}

	puts("Escuchando conexiones");

	conectado = 1;
	cantidadClientes = 0;

	while(conectado) {
		cantidadClientes = sizeof(clientes) / sizeof(struct socket);
		if(cantidadClientes < CANTIDAD_MAXIMA_CONEXIONES)
		{
			struct socket* cli_socket_s;

			cli_socket_s = aceptarConexion(*mi_socket_s);
			if(cli_socket_s->descriptor == 0)
			{
				puts("Se rechaza conexión");
				puts(cli_socket_s->error);
			}

			clientes = realloc(clientes, (cantidadClientes + 1) * sizeof(struct socket));
			clientes[cantidadClientes + 1] = cli_socket_s;


			printf("Se aceptó una conexión. Socket° %d.\n", cli_socket_s->descriptor);
		}
		else
		{
			conectado = 0;
		}

		puts("Escuchando conexiones");
	}

	eliminarSocket(mi_socket_s);
	pthread_exit(&exito);
}
