/*
 * socket.c
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "socket.h"

//////////////ESTRUCTURAS PRIVADAS//////////////
typedef struct {
	int descriptor;
	struct addrinfo* informacion;
	char* error;
} socketInformation_t;

//////////////FUNCIONES PRIVADAS//////////////
socketInformation_t* nuevoSocketInformation() {
	socketInformation_t* socketInformation_s;

	socketInformation_s = malloc(sizeof(socketInformation_t));
	socketInformation_s->descriptor = 0;
	socketInformation_s->informacion = NULL;
	socketInformation_s->error = NULL;

	return socketInformation_s;
}

socketInformation_t* crearSocket(char* ip, char* puerto) {
	socketInformation_t* socketInformation_s;
	int returnValue;

	struct addrinfo hints;
	struct addrinfo* informacion;

	socketInformation_s = nuevoSocketInformation();

	// Se determinan propiedades del socket a crearse
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;	 // Address family (IPv4/IPv6)
	hints.ai_socktype = SOCK_STREAM; // Protocolo TCP

	returnValue = getaddrinfo(ip, puerto, &hints, &informacion);
	if(returnValue != 0)
	{
		socketInformation_s->error = strdup(gai_strerror(returnValue));
		return socketInformation_s;
	}

	// Se crea el socket
	returnValue = socket(informacion->ai_family, informacion->ai_socktype, informacion->ai_protocol);
	if(returnValue == -1)
	{
		freeaddrinfo(informacion);
		socketInformation_s->error = strdup(strerror(errno));
		return socketInformation_s;
	}

	socketInformation_s->descriptor = returnValue;
	socketInformation_s->informacion = informacion;

	return socketInformation_s;
}

//////////////FUNCIONES PÚBLICAS//////////////
socket_t* nuevoSocket() {
	socket_t* socket_s;

	socket_s = malloc(sizeof(socket_t));
	socket_s->descriptor = 0;
	socket_s->error = NULL;

	return socket_s;
}

void eliminarSocket(socket_t* socket_s) {
	free(socket_s->error);
	free(socket_s);
}

socket_t* crearServidor(char* ip, char* puerto) {
	socket_t* socket_s;
	socketInformation_t* socketInformation_s;
	int returnValue;

	int reuseaddr;

	socket_s = nuevoSocket();
	reuseaddr = 1;

	// Se crea el socket del servidor
	socketInformation_s = crearSocket(ip, puerto);
	if(socketInformation_s->descriptor == 0)
	{
		socket_s->error = socketInformation_s->error;
		free(socketInformation_s);
		return socket_s;
	}

	// Se configura la reutilización de direcciones para el socket creado
	setsockopt(socketInformation_s->descriptor, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

	// Se asocia el número de IP y el número de puerto recibidos al socket creado
	returnValue = bind(socketInformation_s->descriptor, socketInformation_s->informacion->ai_addr, socketInformation_s->informacion->ai_addrlen);
	if(returnValue == -1)
	{
		socket_s->error = strdup(strerror(errno));
		freeaddrinfo(socketInformation_s->informacion);
		free(socketInformation_s);
		return socket_s;
	}

	socket_s->descriptor = socketInformation_s->descriptor;

	freeaddrinfo(socketInformation_s->informacion);
	free(socketInformation_s);

	return socket_s;
}

int escucharConexiones(socket_t servidor, int cantidadMaximaConexiones) {
	int returnValue;

	// Escuchar conexiones
	returnValue = listen(servidor.descriptor, cantidadMaximaConexiones);
	if(returnValue == -1)
		return errno;
	else
		return returnValue; // listen() devuelve 0 en caso de finalizar exitosamente
}

socket_t* aceptarConexion(socket_t servidor) {
	socket_t* cliente;
	int returnValue;

	struct sockaddr_in addr_cliente;
	socklen_t addr_cliente_size;

	cliente = nuevoSocket();
	addr_cliente_size = sizeof(addr_cliente);

	returnValue = accept(servidor.descriptor, (struct sockaddr*) &addr_cliente, &addr_cliente_size);
	if(returnValue == -1)
	{
		cliente->error = strdup(strerror(errno));
		return cliente;
	}
	cliente->descriptor = returnValue;

	return cliente;
}

socket_t* conectarAServidor(char* ip, char* puerto) {
	socket_t* socket_s;
	socketInformation_t* socketInformation_s;
	int returnValue;

	socket_s = nuevoSocket();

	// Se obtiene el socket del servidor
	socketInformation_s = crearSocket(ip, puerto);
	if(socketInformation_s->descriptor == 0)
	{
		socket_s->error = socketInformation_s->error;
		free(socketInformation_s);
		return socket_s;
	}

	// Se conecta al cliente al socket del servidor
	returnValue = connect(socketInformation_s->descriptor, socketInformation_s->informacion->ai_addr, socketInformation_s->informacion->ai_addrlen);
	if(returnValue == -1)
	{
		socket_s->error = strdup(strerror(errno));
		freeaddrinfo(socketInformation_s->informacion);
		free(socketInformation_s);
		return socket_s;
	}

	socket_s->descriptor = socketInformation_s->descriptor;

	freeaddrinfo(socketInformation_s->informacion);
	free(socketInformation_s);

	return socket_s;
}

void* crearMensaje(void** componentes) {
	void* mensaje;

	mensaje = strdup("Prueba");

	return mensaje;
}

void enviarMensaje(socket_t* socket, char* mensaje) {
	ssize_t bytesEnviados;
	size_t tamanioMensaje;

	tamanioMensaje = strlen(mensaje) + 1;

	bytesEnviados = send(socket->descriptor, mensaje, tamanioMensaje, 0);
	if (bytesEnviados == -1)
		socket->error = strerror(errno);

	free(mensaje);
}

char* recibirMensaje(socket_t* socket) {
	ssize_t bytesRecibidos;
	size_t tamanioMensaje;
	char* mensaje;

	tamanioMensaje = 255;
	mensaje = malloc(tamanioMensaje);

	bytesRecibidos = recv(socket->descriptor, mensaje, tamanioMensaje, 0);
	if (bytesRecibidos == -1)
		socket->error = strerror(errno);

	return mensaje;
}
