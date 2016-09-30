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
#include "protocoloMapaEntrenador.h"

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
	case CONEXION_ENTRENADOR:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje1_t*) mensaje)->tamanioNombreEntrenador) + ((mensaje1_t*) mensaje)->tamanioNombreEntrenador + sizeof(((mensaje1_t*) mensaje)->simboloEntrenador);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje1_t*) mensaje)->tamanioNombreEntrenador);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje1_t*) mensaje)->tamanioNombreEntrenador), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = ((mensaje1_t*) mensaje)->tamanioNombreEntrenador;
		memcpy(paquete->paqueteSerializado + offset, ((mensaje1_t*) mensaje)->nombreEntrenador, tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje1_t*) mensaje)->simboloEntrenador);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje1_t*) mensaje)->simboloEntrenador), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case SOLICITA_UBICACION:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje5_t*) mensaje)->idPokeNest);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje5_t*) mensaje)->idPokeNest);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje5_t*) mensaje)->idPokeNest), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case BRINDA_UBICACION:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje6_t*) mensaje)->ubicacionX) + sizeof(((mensaje6_t*) mensaje)->ubicacionY);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje6_t*) mensaje)->ubicacionX);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje6_t*) mensaje)->ubicacionX), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje6_t*) mensaje)->ubicacionY);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje6_t*) mensaje)->ubicacionY), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case SOLICITA_DESPLAZAMIENTO:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje7_t*) mensaje)->direccion);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje7_t*) mensaje)->direccion);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje7_t*) mensaje)->direccion), tamanioOperando);
		offset = offset + tamanioOperando;

		break;
	case CONFIRMA_DESPLAZAMIENTO:
		punteroAuxiliar = paquete->paqueteSerializado;

		paquete->tamanioPaquete = paquete->tamanioPaquete + sizeof(((mensaje8_t*) mensaje)->ubicacionX) + sizeof(((mensaje8_t*) mensaje)->ubicacionY);
		paquete->paqueteSerializado = (char*) realloc((void*) paquete->paqueteSerializado, paquete->tamanioPaquete);
		if(paquete->paqueteSerializado == NULL)
		{
			free(punteroAuxiliar);
			paquete->tamanioPaquete = 0;
			return;
		}

		tamanioOperando = sizeof(((mensaje8_t*) mensaje)->ubicacionX);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje8_t*) mensaje)->ubicacionX), tamanioOperando);
		offset = offset + tamanioOperando;

		tamanioOperando = sizeof(((mensaje8_t*) mensaje)->ubicacionY);
		memcpy(paquete->paqueteSerializado + offset, &(((mensaje8_t*) mensaje)->ubicacionY), tamanioOperando);
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
	void* punteroAuxiliar;
	ssize_t bytesRecibidos;
	size_t tamanioBuffer;
	char* buffer;

	// Se recibe el tipo de mensaje
	uint32_t tipoMensaje;
	tamanioBuffer = sizeof(tipoMensaje);
	buffer = malloc(tamanioBuffer);

	bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
	if(bytesRecibidos == -1)
		socket->error = strerror(errno);

	memcpy(&tipoMensaje, buffer, tamanioBuffer);

	if(((mensaje_t*) mensaje)->tipoMensaje == tipoMensaje || ((mensaje_t*) mensaje)->tipoMensaje == 0)
	{
		switch(tipoMensaje) {
		case CONEXION_ENTRENADOR:
			if(((mensaje1_t*) mensaje)->tipoMensaje == 0)
			{
				punteroAuxiliar = mensaje;
				mensaje = realloc(mensaje, sizeof(mensaje1_t));
				if(mensaje == NULL)
				{
					mensaje = punteroAuxiliar;
					((mensaje1_t*) mensaje)->tipoMensaje = 0;
				}
			}

			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->tamanioNombreEntrenador);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje1_t*) mensaje)->tamanioNombreEntrenador), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = ((mensaje1_t*) mensaje)->tamanioNombreEntrenador;
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			((mensaje1_t*) mensaje)->nombreEntrenador = malloc(tamanioBuffer);
			memcpy(((mensaje1_t*) mensaje)->nombreEntrenador, buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = sizeof(((mensaje1_t*) mensaje)->simboloEntrenador);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje1_t*) mensaje)->simboloEntrenador), buffer, tamanioBuffer);

			break;
		case SOLICITA_UBICACION:
			if(((mensaje5_t*) mensaje)->tipoMensaje == 0)
			{
				punteroAuxiliar = mensaje;
				mensaje = realloc(mensaje, sizeof(mensaje5_t));
				if(mensaje == NULL)
				{
					mensaje = punteroAuxiliar;
					((mensaje5_t*) mensaje)->tipoMensaje = 0;
				}
			}

			free(buffer);
			tamanioBuffer = sizeof(((mensaje5_t*) mensaje)->idPokeNest);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje5_t*) mensaje)->idPokeNest), buffer, tamanioBuffer);

			break;
		case BRINDA_UBICACION:
			if(((mensaje6_t*) mensaje)->tipoMensaje == 0)
			{
				punteroAuxiliar = mensaje;
				mensaje = realloc(mensaje, sizeof(mensaje6_t));
				if(mensaje == NULL)
				{
					mensaje = punteroAuxiliar;
					((mensaje6_t*) mensaje)->tipoMensaje = 0;
				}
			}

			free(buffer);
			tamanioBuffer = sizeof(((mensaje6_t*) mensaje)->ubicacionX);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje6_t*) mensaje)->ubicacionX), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = sizeof(((mensaje6_t*) mensaje)->ubicacionY);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje6_t*) mensaje)->ubicacionY), buffer, tamanioBuffer);

			break;
		case SOLICITA_DESPLAZAMIENTO:
			if(((mensaje7_t*) mensaje)->tipoMensaje == 0)
			{
				punteroAuxiliar = mensaje;
				mensaje = realloc(mensaje, sizeof(mensaje7_t));
				if(mensaje == NULL)
				{
					mensaje = punteroAuxiliar;
					((mensaje7_t*) mensaje)->tipoMensaje = 0;
				}
			}

			free(buffer);
			tamanioBuffer = sizeof(((mensaje7_t*) mensaje)->direccion);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje7_t*) mensaje)->direccion), buffer, tamanioBuffer);

			break;
		case CONFIRMA_DESPLAZAMIENTO:
			if(((mensaje8_t*) mensaje)->tipoMensaje == 0)
			{
				punteroAuxiliar = mensaje;
				mensaje = realloc(mensaje, sizeof(mensaje8_t));
				if(mensaje == NULL)
				{
					mensaje = punteroAuxiliar;
					((mensaje8_t*) mensaje)->tipoMensaje = 0;
				}
			}

			free(buffer);
			tamanioBuffer = sizeof(((mensaje8_t*) mensaje)->ubicacionX);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje8_t*) mensaje)->ubicacionX), buffer, tamanioBuffer);

			free(buffer);
			tamanioBuffer = sizeof(((mensaje8_t*) mensaje)->ubicacionY);
			buffer = malloc(tamanioBuffer);

			bytesRecibidos = recv(socket->descriptor, buffer, tamanioBuffer, 0);
			if(bytesRecibidos == -1)
				socket->error = strerror(errno);

			memcpy(&(((mensaje8_t*) mensaje)->ubicacionY), buffer, tamanioBuffer);

			break;
		}
	}
}
