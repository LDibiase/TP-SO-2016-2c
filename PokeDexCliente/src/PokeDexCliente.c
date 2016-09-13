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
#include "socket.h" // BORRAR
#include "PokeDexCliente.h"

int main(void) {
	struct socket* serv_socket_s;

//	puts("Proceso Cliente del PokéDex"); /* prints Proceso Cliente del PokéDex */

	serv_socket_s = conectarAServidor("127.0.0.1", "8080");
	if(serv_socket_s->descriptor == 0)
	{
		puts("Conexión fallida");
		puts(serv_socket_s->error);
		return EXIT_FAILURE;
	}

	puts("Conexión exitosa");

	eliminarSocket(serv_socket_s);

	return EXIT_SUCCESS;
}
