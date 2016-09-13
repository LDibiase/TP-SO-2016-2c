/*
 ============================================================================
 Name        : PokeDexCliente.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Cliente del PokéDex
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
//#include <Utility_Library/socket.h>
#include <commons/log.h>
#include "socket.h" // BORRAR
#include "PokeDexCliente.h"

#define LOG_FILE_PATH "PokeDexCliente.log"

/* Variables */
t_log* logger;

int main(void) {
	struct socket* serv_socket_s;

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	serv_socket_s = conectarAServidor("127.0.0.1", "8080");
	if(serv_socket_s->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, serv_socket_s->error);
		return EXIT_FAILURE;
	}

	log_info(logger, "Conexión exitosa");

	while(1);

	eliminarSocket(serv_socket_s);

	return EXIT_SUCCESS;
}
