/*
 ============================================================================
 Name        : PokeDexServidor.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Servidor PokéDex
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
//#include <Utility_Library/socket.h>
#include "socket.h" // BORRAR
#include "PokeDexServidor.h"

#define IP "127.0.0.1"
#define PUERTO "8080"
#define CANTIDAD_MAXIMA_CONEXIONES 1000

struct socket** clientes;

int main(void) {
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;
	int idHilo;
	void* valorRetorno;

	pthread_attr_init(&atributosHilo);
	idHilo = pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);

	if(idHilo == 0)
		pthread_join(hiloEnEscucha, &valorRetorno);

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
