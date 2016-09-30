/*
 * protocoloMapaEntrenador.h
 *
 *  Created on: 17/9/2016
 *      Author: utnso
 */

#ifndef UTILITY_LIBRARY_PROTOCOLOMAPAENTRENADOR_H_
#define UTILITY_LIBRARY_PROTOCOLOMAPAENTRENADOR_H_
#include <stdint.h>

/* PROTOCOLO DE COMUNICACIÓN */

/*
 *	Mensaje 1: Informa al mapa que un entrenador se ha conectado
 *	-->	mensaje1_t
 *
 *	Mensaje 2: Informa al entrenador que el mapa ha aceptado su conexión
 *	-->	mensaje_t
 *
 *	Mensaje 3: Informa al entrenador que el mapa ha rechazado su conexión
 *	-->	mensaje_t
 *
 *	Mensaje 4: Informa al entrenador que el mapa le ha concedido un turno
 *	-->	mensaje_t
 *
 *	Mensaje 5: Informa al mapa que el entrenador le ha solicitado la ubicación de una PokéNest
 *	-->	mensaje5_t
 *
 *	Mensaje 6: Informa al entrenador que el mapa le ha brindado la ubicación solicitada
 *	-->	mensaje6_t
 *
 *	Mensaje 7: Informa al mapa que el entrenador le ha solicitado desplazamiento en determinada dirección
 *	-->	mensaje7_t
 *
 *	Mensaje 8: Informa al entrenador que el mapa le ha brindado su nueva ubicación tras haber realizado el desplazamiento solicitado
 *	-->	mensaje8_t
 *
 *	Mensaje 9: Informa al mapa que el entrenador le ha solicitado capturar un Pokémon
 *	-->	mensaje_t
 *
 *	Mensaje 10: Informa al entrenador que el mapa ha confirmado la captura del Pokémon solicitado
 *	-->	mensaje_t
 *
 *	Mensaje 11: Informa al mapa que el entrenador ha confirmado la compleción de sus objetivos dentro de él
 *	-->	mensaje_t
 */



/* ESTRUCTURAS */

// Mensajes
typedef enum tipoMensaje {
	CONEXION_ENTRENADOR,
	ACEPTA_CONEXION,
	RECHAZA_CONEXION,
	TURNO,
	SOLICITA_UBICACION,
	BRINDA_UBICACION,
	SOLICITA_DESPLAZAMIENTO,
	CONFIRMA_DESPLAZAMIENTO,
	SOLICITA_CAPTURA,
	CONFIRMA_CAPTURA,
	OBJETIVOS_COMPLETADOS
} tipoMensaje_t;

// Mensaje genérico (sin operandos)
typedef struct mensaje {
	uint32_t tipoMensaje;
} mensaje_t;

// Mensaje 1
typedef struct mensaje1 {
	uint32_t tipoMensaje;
	uint32_t tamanioNombreEntrenador;
	char* nombreEntrenador;
	char simboloEntrenador;
} mensaje1_t;

// Mensaje 5
typedef struct mensaje5 {
	uint32_t tipoMensaje;
	char idPokeNest;
} mensaje5_t;

// Mensaje 6
typedef struct mensaje6 {
	uint32_t tipoMensaje;
	uint32_t ubicacionX;
	uint32_t ubicacionY;
} mensaje6_t;

// Mensaje 7
typedef struct mensaje7 {
	uint32_t tipoMensaje;
	uint32_t direccion;
} mensaje7_t;

// Mensaje 8
typedef struct mensaje8 {
	uint32_t tipoMensaje;
	uint32_t ubicacionX;
	uint32_t ubicacionY;
} mensaje8_t;

// Direcciones de movimiento
typedef enum direccion {IZQUIERDA, DERECHA, ARRIBA, ABAJO} direccion_t;

// Ubicación
typedef struct ubicacion {
	int x;
	int y;
} t_ubicacion;

#endif /* UTILITY_LIBRARY_PROTOCOLOMAPAENTRENADOR_H_ */
