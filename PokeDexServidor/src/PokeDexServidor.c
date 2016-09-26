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
#include <commons/log.h>
//#include <Utility_Library/socket.h>
#include "socket.h" // BORRAR
#include "PokeDexServidor.h"
#include "osada.h"
#include <commons/bitarray.h>

#define IP "127.0.0.1"			 // IP
#define PUERTO "8080"			 // Puerto al que conectarán los Clientes de PokéDex
#define CONEXIONES_PENDIENTES 10 // Cantidad de conexiones pendientes que se pueden mantener en simultáneo
#define LOG_FILE_PATH "PokeDexServidor.log"

/* Variables */
t_log* logger;
struct socket** clientes;

int main(void) {
	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;

	// Variables para el manejo del FileSystem OSADA
	FILE *fileFS;
	osada_header cabeceraFS;
	unsigned char * bitmapS;
	t_bitarray * bitarray;
	int i;
	int j;

	//CREACIÓN DEL ARCHIVO DE LOG
	logger = log_create(LOG_FILE_PATH, "MAPA", true, LOG_LEVEL_INFO);

	// Creación del hilo en escucha
	pthread_attr_init(&atributosHilo);
	pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);

	// Abre el archivo de File System
	if ((fileFS=fopen("/home/utnso/workspace/Osada1.bin","r"))==NULL)
	{
		puts("Error al abrir el archivo de FS.\n");
		return EXIT_FAILURE;
	}

	// Valida que sea un FS OSADA
	if (strncmp(cabeceraFS.magic_number, "OsadaFS", 7) == 0)
		puts("Es un FS Osada\n");
	else
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

	printf("aloco %d\n", sizeof(bitmapS));
	printf("%d\n", cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// Lee el BITMAP
	fread(bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE, 1 , fileFS);

	puts("leyo\n");

	// Crea el array de bits
	bitarray = bitarray_create(bitmapS, cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE);

	// Imprime todos los bits
	j = cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE * 8;
	for (i = 0; i < j; i++)
		printf("%d = %d\n", i, bitarray_test_bit(bitarray, i));

	// Lee la cabecera del archivo
	fread(&cabeceraFS, sizeof(cabeceraFS), 1, fileFS);

	puts(cabeceraFS.magic_number);
	printf("%d\n", cabeceraFS.version);
	printf("%d\n", cabeceraFS.fs_blocks);
	printf("%d\n", cabeceraFS.bitmap_blocks);
	printf("%d\n", cabeceraFS.allocations_table_offset);
	printf("%d\n", cabeceraFS.data_blocks);
	puts(cabeceraFS.padding);

	printf("%d\n", CHAR_BIT);
	printf("%d\n", cabeceraFS.bitmap_blocks * OSADA_BLOCK_SIZE * 8);
	printf("%d\n", bitarray_get_max_bit(bitarray));

	free(bitmapS);
	bitmapS=NULL;

	fclose(fileFS);

	log_destroy(logger);
	// TODO Cerrar la conexión del servidor
	return EXIT_SUCCESS;
}

void aceptarConexiones() {
	struct socket* mi_socket_s;

	int returnValue;
	int conectado;
	int cantidadClientes;

	int error = -1;

	mi_socket_s = crearServidor(IP, PUERTO);
	if(mi_socket_s->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, mi_socket_s->error);
		exit(error);
	}

	returnValue = escucharConexiones(*mi_socket_s, CONEXIONES_PENDIENTES);
	if(returnValue != 0)
	{
		log_info(logger, strerror(returnValue));
		eliminarSocket(mi_socket_s);
		exit(error);
	}

	conectado = 1;
	cantidadClientes = 0;

	while(conectado) {
		struct socket* cli_socket_s;

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

		cantidadClientes = cantidadElementosArray((void**) clientes);
		clientes = realloc(clientes, (cantidadClientes + 1) * sizeof(struct socket));
		clientes[cantidadClientes] = malloc(sizeof(struct socket));
		clientes[cantidadClientes] = cli_socket_s;
		clientes[cantidadClientes + 1] = NULL;

		log_info(logger, "Se aceptó una conexión. Socket° %d.\n", cli_socket_s->descriptor);
	}
}
