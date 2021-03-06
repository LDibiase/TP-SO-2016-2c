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
	case GETATTR:
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
	case GETATTR_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje3_t*) mensaje)->tipoArchivo) + sizeof(((mensaje3_t*) mensaje)->tamanioArchivo) + sizeof(((mensaje3_t*) mensaje)->lastModif);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje3_t*) mensaje)->tipoArchivo);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje3_t*) mensaje)->tipoArchivo), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje3_t*) mensaje)->tamanioArchivo);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje3_t*) mensaje)->tamanioArchivo), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje3_t*) mensaje)->lastModif);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje3_t*) mensaje)->lastModif), tamanioOperando);
		offset = offset + tamanioOperando;
		break;
	case READ:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje4_t*) mensaje)->tamanioPath) + sizeof(((mensaje4_t*) mensaje)->tamanioBuffer) + sizeof(((mensaje4_t*) mensaje)->offset) + ((mensaje4_t*) mensaje)->tamanioPath;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje4_t*) mensaje)->tamanioPath);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje4_t*) mensaje)->tamanioPath), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje4_t*) mensaje)->tamanioPath;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje4_t*) mensaje)->path, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje4_t*) mensaje)->tamanioBuffer);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje4_t*) mensaje)->tamanioBuffer, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje4_t*) mensaje)->offset);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje4_t*) mensaje)->offset, tamanioOperando);
		offset = offset + tamanioOperando;
		break;
	case READ_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje5_t*) mensaje)->tamanioBuffer) + ((mensaje5_t*) mensaje)->tamanioBuffer;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje5_t*) mensaje)->tamanioBuffer);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje5_t*) mensaje)->tamanioBuffer), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje5_t*) mensaje)->tamanioBuffer;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje5_t*) mensaje)->buffer, tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case MKDIR:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje6_t*) mensaje)->tamanioPath) + sizeof(((mensaje6_t*) mensaje)->modo) + ((mensaje6_t*) mensaje)->tamanioPath;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje6_t*) mensaje)->tamanioPath);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje6_t*) mensaje)->tamanioPath), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje6_t*) mensaje)->tamanioPath;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje6_t*) mensaje)->path, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje6_t*) mensaje)->modo);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje6_t*) mensaje)->modo, tamanioOperando);
		offset = offset + tamanioOperando;
		break;
	case MKDIR_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;


		break;
	case RMDIR:
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
	case RMDIR_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;


		break;
	case UNLINK:
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
	case UNLINK_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case MKNOD:
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
	case MKNOD_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case WRITE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje8_t*) mensaje)->tamanioPath) + sizeof(((mensaje8_t*) mensaje)->tamanioBuffer) + sizeof(((mensaje8_t*) mensaje)->offset) + ((mensaje8_t*) mensaje)->tamanioPath + ((mensaje8_t*) mensaje)->tamanioBuffer;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje8_t*) mensaje)->tamanioPath);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje4_t*) mensaje)->tamanioPath), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje8_t*) mensaje)->tamanioPath;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje8_t*) mensaje)->path, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje8_t*) mensaje)->tamanioBuffer);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje8_t*) mensaje)->tamanioBuffer, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje8_t*) mensaje)->tamanioBuffer;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje8_t*) mensaje)->buffer, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje8_t*) mensaje)->offset);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje8_t*) mensaje)->offset, tamanioOperando);
		offset = offset + tamanioOperando;
		break;
	case WRITE_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case RENAME:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje9_t*) mensaje)->tamanioPathFrom) + ((mensaje9_t*) mensaje)->tamanioPathFrom + sizeof(((mensaje9_t*) mensaje)->tamanioPathTo) + ((mensaje9_t*) mensaje)->tamanioPathTo;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje9_t*) mensaje)->tamanioPathFrom);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje9_t*) mensaje)->tamanioPathFrom), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje9_t*) mensaje)->tamanioPathFrom;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje9_t*) mensaje)->pathFrom, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje9_t*) mensaje)->tamanioPathTo);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje9_t*) mensaje)->tamanioPathTo), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje9_t*) mensaje)->tamanioPathTo;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje9_t*) mensaje)->pathTo, tamanioOperando);
		offset = offset + tamanioOperando;
		break;
	case RENAME_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case TRUNCATE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje10_t*) mensaje)->tamanioPath) + sizeof(((mensaje10_t*) mensaje)->size) + ((mensaje10_t*) mensaje)->tamanioPath;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje10_t*) mensaje)->tamanioPath);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje10_t*) mensaje)->tamanioPath), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje10_t*) mensaje)->tamanioPath;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje10_t*) mensaje)->path, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje10_t*) mensaje)->size);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje10_t*) mensaje)->size, tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case TRUNCATE_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case UTIMENS:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje10_t*) mensaje)->tamanioPath) + sizeof(((mensaje10_t*) mensaje)->size) + ((mensaje10_t*) mensaje)->tamanioPath;
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje10_t*) mensaje)->tamanioPath);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje10_t*) mensaje)->tamanioPath), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje10_t*) mensaje)->tamanioPath;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje10_t*) mensaje)->path, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje10_t*) mensaje)->size);
		memcpy(paquete->paqueteSerializado + offset, &((mensaje10_t*) mensaje)->size, tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case UTIMENS_RESPONSE:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->res);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->res);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->res), tamanioOperando);
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

	bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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
		case GETATTR:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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
		case READDIR_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje2_t*) mensaje)->tamanioMensaje);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje2_t*) mensaje)->tamanioMensaje), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje2_t*) mensaje)->tamanioMensaje;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje2_t*) mensaje)->mensaje = malloc(tamanioBuffer);
			memcpy(((mensaje2_t*) mensaje)->mensaje, buffer, tamanioBuffer);
			free(buffer);

			break;
		case GETATTR_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje3_t*) mensaje)->tipoArchivo);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje3_t*) mensaje)->tipoArchivo), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = sizeof(((mensaje3_t*) mensaje)->tamanioArchivo);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje3_t*) mensaje)->tamanioArchivo = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje3_t*) mensaje)->tamanioArchivo, buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = sizeof(((mensaje3_t*) mensaje)->lastModif);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje3_t*) mensaje)->lastModif = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje3_t*) mensaje)->lastModif, buffer, tamanioBuffer);

			free(buffer);

			break;
		case READ:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje4_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje4_t*) mensaje)->tamanioPath), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje4_t*) mensaje)->tamanioPath;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje4_t*) mensaje)->path = malloc(tamanioBuffer);
			memcpy(((mensaje4_t*) mensaje)->path, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje4_t*) mensaje)->tamanioBuffer);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje4_t*) mensaje)->tamanioBuffer = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje4_t*) mensaje)->tamanioBuffer, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje4_t*) mensaje)->offset);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje4_t*) mensaje)->offset = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje4_t*) mensaje)->offset, buffer, tamanioBuffer);
			free(buffer);

			break;
		case READ_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje5_t*) mensaje)->tamanioBuffer);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje5_t*) mensaje)->tamanioBuffer), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje5_t*) mensaje)->tamanioBuffer;

			if (tamanioBuffer>0) {
				buffer = malloc(tamanioBuffer);

				bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

				((mensaje5_t*) mensaje)->buffer = malloc(tamanioBuffer);
				memcpy(((mensaje5_t*) mensaje)->buffer, buffer, tamanioBuffer);
				free(buffer);
			}

			break;
		case MKDIR:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje6_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje6_t*) mensaje)->tamanioPath), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje6_t*) mensaje)->tamanioPath;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje6_t*) mensaje)->path = malloc(tamanioBuffer);
			memcpy(((mensaje6_t*) mensaje)->path, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje6_t*) mensaje)->modo);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje6_t*) mensaje)->modo = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje6_t*) mensaje)->modo, buffer, tamanioBuffer);
			free(buffer);

			break;
		case MKDIR_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case RMDIR:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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
		case RMDIR_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case UNLINK:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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
		case UNLINK_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case MKNOD:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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
		case MKNOD_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case WRITE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje8_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje8_t*) mensaje)->tamanioPath), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje8_t*) mensaje)->tamanioPath;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje8_t*) mensaje)->path = malloc(tamanioBuffer);
			memcpy(((mensaje8_t*) mensaje)->path, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje8_t*) mensaje)->tamanioBuffer);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje8_t*) mensaje)->tamanioBuffer = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje8_t*) mensaje)->tamanioBuffer, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = ((mensaje8_t*) mensaje)->tamanioBuffer;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje8_t*) mensaje)->buffer = malloc(tamanioBuffer);
			memcpy(((mensaje8_t*) mensaje)->buffer, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje8_t*) mensaje)->offset);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje8_t*) mensaje)->offset = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje8_t*) mensaje)->offset, buffer, tamanioBuffer);
			free(buffer);

			break;
		case WRITE_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case RENAME:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje9_t*) mensaje)->tamanioPathFrom);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje9_t*) mensaje)->tamanioPathFrom), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje9_t*) mensaje)->tamanioPathFrom;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje9_t*) mensaje)->pathFrom = malloc(tamanioBuffer);
			memcpy(((mensaje9_t*) mensaje)->pathFrom, buffer, tamanioBuffer);
			free(buffer);
			tamanioBuffer = sizeof(((mensaje9_t*) mensaje)->tamanioPathTo);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje9_t*) mensaje)->tamanioPathTo), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje9_t*) mensaje)->tamanioPathTo;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje9_t*) mensaje)->pathTo = malloc(tamanioBuffer);
			memcpy(((mensaje9_t*) mensaje)->pathTo, buffer, tamanioBuffer);
			free(buffer);

			break;
		case RENAME_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case TRUNCATE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje10_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje10_t*) mensaje)->tamanioPath), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje10_t*) mensaje)->tamanioPath;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje10_t*) mensaje)->path = malloc(tamanioBuffer);
			memcpy(((mensaje10_t*) mensaje)->path, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje10_t*) mensaje)->size);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje10_t*) mensaje)->size = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje10_t*) mensaje)->size, buffer, tamanioBuffer);
			free(buffer);

			break;
		case TRUNCATE_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		case UTIMENS:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje10_t*) mensaje)->tamanioPath);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje10_t*) mensaje)->tamanioPath), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje10_t*) mensaje)->tamanioPath;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje10_t*) mensaje)->path = malloc(tamanioBuffer);
			memcpy(((mensaje10_t*) mensaje)->path, buffer, tamanioBuffer);
			free(buffer);

			tamanioBuffer = sizeof(((mensaje10_t*) mensaje)->size);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			((mensaje10_t*) mensaje)->size = (int)malloc(tamanioBuffer);
			memcpy(&((mensaje10_t*) mensaje)->size, buffer, tamanioBuffer);
			free(buffer);

			break;
		case UTIMENS_RESPONSE:
			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->res);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, MSG_WAITALL);
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

			memcpy(&(((mensaje7_t*) mensaje)->res), buffer, tamanioBuffer);

			break;
		}
	}

	((mensaje_t*) mensaje)->tipoMensaje = tipoMensaje;
}
