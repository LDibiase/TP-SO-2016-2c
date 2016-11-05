/*
 * protocoloPokedexClienteServidor.h
 *
 *  Created on: 17/9/2016
 *      Author: utnso
 */

#ifndef UTILITY_LIBRARY_PROTOCOLOPOKEDEXCLIENTESERVIDOR_H_
#define UTILITY_LIBRARY_PROTOCOLOPOKEDEXCLIENTESERVIDOR_H_
#include <stdint.h>


/* PROTOCOLO DE COMUNICACIÓN */

/*
 *	Mensaje 1: Path
 *	-->	mensaje1_t
 */

/* ESTRUCTURAS */

// Mensajes
typedef enum tipoMensaje {
	INDEFINIDO,
	ACEPTA_CONEXION,
	RECHAZA_CONEXION,
	CONEXION_POKEDEX_CLIENTE,
	READDIR,
	READDIR_RESPONSE,
	GETATTR,
	GETATTR_RESPONSE,
	READ,
	READ_RESPONSE
} tipoMensaje_t;



// Mensaje genérico (sin operandos)
typedef struct mensaje {
	uint32_t tipoMensaje;
} mensaje_t;

// Mensaje 1
typedef struct mensaje1 {
	uint32_t tipoMensaje;
	uint32_t tamanioPath;
	char* path;
} mensaje1_t;


// Mensaje 2
typedef struct mensaje2 {
	uint32_t tipoMensaje;
	uint32_t tamanioMensaje;
	char* mensaje;
} mensaje2_t;

// Mensaje 3
typedef struct mensaje3 {
	uint32_t tipoMensaje;
	int tipoArchivo;
	int tamanioArchivo;
} mensaje3_t;

typedef struct mensaje4 {
	uint32_t tipoMensaje;
	uint32_t tamanioPath;
	char* path;
	int tamanioBuffer;
	int offset;
} mensaje4_t;

typedef struct mensaje5 {
	uint32_t tipoMensaje;
	int tamanioBuffer;
	char* buffer;
} mensaje5_t;

#endif /* UTILITY_LIBRARY_PROTOCOLOPOKEDEXCLIENTESERVIDOR_H_ */
