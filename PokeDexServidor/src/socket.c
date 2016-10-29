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
#include <commons/string.h>
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
	close(socket_s->descriptor);
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

void crearPaquete(void* mensaje, paquete_t* paquete) {
	char* punteroAuxiliar;
	int offset;
	size_t tamanioOperando;

	// Se almacena el tipo de mensaje
	uint32_t tipoMensaje;
	tipoMensaje = ((mensaje_t*) mensaje)->tipoMensaje;

	offset = 0;

	paquete->tamanioPaquete = sizeof(tipoMensaje);
	paquete->paqueteSerializado = malloc(paquete->tamanioPaquete);
	memcpy(paquete->paqueteSerializado + offset, &tipoMensaje, paquete->tamanioPaquete);
	offset = offset + paquete->tamanioPaquete;

	// La lógica de serialización varía de acuerdo al tipo del mensaje a enviarse
	switch(tipoMensaje) {
	case READDIR:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje1_t*) mensaje)->tamanioPath) + ((mensaje1_t*) mensaje)->tamanioPath;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje1_t*) mensaje)->tamanioPath);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje1_t*) mensaje)->tamanioPath), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje1_t*) mensaje)->tamanioPath;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje1_t*) mensaje)->path, tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case READDIR_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje2_t*) mensaje)->tamanioMensaje) + ((mensaje2_t*) mensaje)->tamanioMensaje;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
				{
					free(punteroAuxiliar);
					paquete->tamanioPaquete = 0;
					return;
				}

				tamanioOperando = sizeof(((mensaje2_t*) mensaje)->tamanioMensaje);
				memcpy(paquete->paqueteSerializado + offset, &(((mensaje2_t*) mensaje)->tamanioMensaje), tamanioOperando);
				offset = offset + tamanioOperando;

				tamanioOperando = ((mensaje2_t*) mensaje)->tamanioMensaje;
				memcpy(paquete->paqueteSerializado + offset, ((mensaje2_t*) mensaje)->mensaje, tamanioOperando);
				offset = offset + tamanioOperando;
		break;
	}
}

void enviarMensaje(socket_t* socket, paquete_t paquete) {
	ssize_t bytesEnviados;

	bytesEnviados = send(socket->descriptor, paquete.paqueteSerializado, paquete.tamanioPaquete, 0);
	if(bytesEnviados == -1)
		socket->error = strerror(errno);
}

void recibirMensaje(socket_t* socket, void* mensaje) {
	ssize_t bytesRecibidos;
	size_t tamanioBuffer;
	char* buffer;

	// Se recibe el tipo de mensaje
	uint32_t tipoMensaje;
	tamanioBuffer = sizeof(tipoMensaje);
	buffer = malloc(tamanioBuffer);

	bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
	if(bytesRecibidos == 0)
	{
		socket->error = strdup("El receptor a quien se desea enviar el mensaje se ha desconectado");
		free(buffer);
		return;
	}
	else if(bytesRecibidos == -1)
	{
		socket->error = strerror(errno);
		free(buffer);
		return;
	}

	memcpy(&tipoMensaje, buffer, tamanioBuffer);

	if(((mensaje_t*) mensaje)->tipoMensaje == tipoMensaje || ((mensaje_t*) mensaje)->tipoMensaje == INDEFINIDO)
	{
		switch(tipoMensaje) {
		case READDIR:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == 0)
			{
				socket->error = strdup("El receptor a quien se desea enviar el mensaje se ha desconectado");
				free(buffer);
				return;
			}
			else if(bytesRecibidos == -1)
			{
				socket->error = strerror(errno);
				free(buffer);
				return;
			}

			memcpy(&(((mensaje1_t*) mensaje)->tamanioPath), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje1_t*) mensaje)->tamanioPath;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == 0)
			{
				socket->error = strdup("El receptor a quien se desea enviar el mensaje se ha desconectado");
				free(buffer);
				return;
			}
			else if(bytesRecibidos == -1)
			{
				socket->error = strerror(errno);
				free(buffer);
				return;
			}

			((mensaje1_t*) mensaje)->path = malloc(tamanioBuffer);
			memcpy(((mensaje1_t*) mensaje)->path, buffer, tamanioBuffer);

			free(buffer);

			break;
		}
	}

	((mensaje_t*) mensaje)->tipoMensaje = tipoMensaje;
}
