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
 *	Mensaje 4: Informa al mapa que el entrenador le ha solicitado la ubicación de una PokéNest
 *	-->	mensaje4_t
 *
 *	Mensaje 5: Informa al entrenador que el mapa le ha brindado la ubicación solicitada
 *	-->	mensaje5_t
 *
 *	Mensaje 6: Informa al mapa que el entrenador le ha solicitado desplazamiento en determinada dirección
 *	-->	mensaje6_t
 *
 *	Mensaje 7: Informa al entrenador que el mapa le ha brindado su nueva ubicación tras haber realizado el desplazamiento solicitado
 *	-->	mensaje7_t
 *
 *	Mensaje 8: Informa al mapa que el entrenador le ha solicitado capturar un Pokémon
 *	-->	mensaje_t
 *
 *	Mensaje 9: Informa al entrenador que el mapa ha confirmado la captura del Pokémon solicitado
 *	-->	mensaje9_t
 *
 *	Mensaje 10: Informa al entrenador que se encuentra involucrado en un interbloqueo con otro entrenador a causa de la solicitud realizada
 *	-->	mensaje_t
 *
 *	Mensaje 11: Informa al entrenador que debe elegir un Pokémon con el cual participar del o los Combates Pokémon
 *	-->	mensaje_t
 *
 *	Mensaje 12: Informa al mapa qué Pokémon ha elegido el entrenador para participar del o los Combates Pokémon
 *	-->	mensaje12_t
 *
 *	Mensaje 13: Informa al entrenador que ha ganado un Combate Pokémon
 *	-->	mensaje13_t
 *
 *	Mensaje 14: Informa al entrenador que ha perdido un Combate Pokémon
 *	-->	mensaje14_t
 *
 *	Mensaje 15: Informa al entrenador que ha resultado víctima en el o los Combates Pokémon
 *	-->	mensaje_t
 *
 *	Mensaje 16: Informa al mapa que el entrenador se ha desconectado
 *	-->	mensaje_t
 */



/* ESTRUCTURAS */

// Mensajes
typedef enum tipoMensaje {
	INDEFINIDO,
	CONEXION_ENTRENADOR,
	ACEPTA_CONEXION,
	RECHAZA_CONEXION,
	SOLICITA_UBICACION,
	BRINDA_UBICACION,
	SOLICITA_DESPLAZAMIENTO,
	CONFIRMA_DESPLAZAMIENTO,
	SOLICITA_CAPTURA,
	CONFIRMA_CAPTURA,
	INFORMA_INTERBLOQUEO,
	SOLICITA_ELECCION_POKEMON,
	INFORMA_POKEMON_ELEGIDO,
	INFORMA_VICTORIA,
	INFORMA_DERROTA,
	SOLICITA_DESCONEXION,
	DESCONEXION_ENTRENADOR
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

// Mensaje 4
typedef struct mensaje4 {
	uint32_t tipoMensaje;
	char idPokeNest;
} mensaje4_t;

// Mensaje 5
typedef struct mensaje5 {
	uint32_t tipoMensaje;
	uint32_t ubicacionX;
	uint32_t ubicacionY;
} mensaje5_t;

// Mensaje 6
typedef struct mensaje6 {
	uint32_t tipoMensaje;
	uint32_t direccion;
} mensaje6_t;

// Mensaje 7
typedef struct mensaje7 {
	uint32_t tipoMensaje;
	uint32_t ubicacionX;
	uint32_t ubicacionY;
} mensaje7_t;

// Mensaje 9
typedef struct mensaje9 {
	uint32_t tipoMensaje;
	uint32_t nivel;
	uint32_t tamanioNombreArchivoMetadata;
	char* nombreArchivoMetadata;
} mensaje9_t;

// Mensaje 12
typedef struct mensaje12 {
	uint32_t tipoMensaje;
	uint32_t tamanioNombrePokemon;
	char* nombrePokemon;
	uint32_t nivel;
} mensaje12_t;

// Mensaje 13
typedef struct mensaje13 {
	uint32_t tipoMensaje;
	uint32_t tamanioNombreAdversario;
	char* nombreAdversario;
} mensaje13_t;

// Mensaje 14
typedef struct mensaje14 {
	uint32_t tipoMensaje;
	uint32_t tamanioNombreAdversario;
	char* nombreAdversario;
} mensaje14_t;

// Direcciones de movimiento
typedef enum direccion {IZQUIERDA, DERECHA, ARRIBA, ABAJO} direccion_t;

// Ubicación
typedef struct ubicacion {
	int x;
	int y;
} t_ubicacion;

#endif /* UTILITY_LIBRARY_PROTOCOLOMAPAENTRENADOR_H_ */
