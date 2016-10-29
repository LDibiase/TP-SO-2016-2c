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
	READDIR
} tipoMensaje_t;

// Mensaje genérico (sin operandos)
typedef struct mensaje {
	uint32_t tipoMensaje;
} mensaje_t;

// Mensaje 1
typedef struct mensaje1 {
	uint32_t tamanioPath;
	uint32_t tipoMensaje;
	char* path;
} mensaje1_t;



#endif /* UTILITY_LIBRARY_PROTOCOLOPOKEDEXCLIENTESERVIDOR_H_ */
