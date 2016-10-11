/*
 * PokeDexCliente.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef POKEDEXCLIENTE_H_
#define POKEDEXCLIENTE_H_
#define DEFAULT_FILE_PATH "/" DEFAULT_FILE_NAME
#define DEFAULT_FILE_NAME "Osada"
#define DEFAULT_FILE_CONTENT "Is Empty!\n"
#define CUSTOM_FUSE_OPT_KEY(t, p, v) { t, offsetof(struct t_runtime_options, p), v }
/* Definición de estructuras */


struct t_runtime_options {
	char* welcome_msg;
} runtime_options;

//Mensaje Pokedex Cliente-Servidor
typedef struct mensajePokedex {
	uint32_t tipoMensaje;
	uint32_t ruta;
} mensajePokedex;

// Mensajes
typedef enum tipoMensajePokedex {
	ES_OSADA,
	NO_ES_OSADA
} tipoMensaje_pokedex;

// Mensaje genérico (sin operandos)
typedef struct mensaje {
	uint32_t tipoMensaje;
} mensaje_t;

/* Declaración de funciones */



#endif /* POKEDEXCLIENTE_H_ */
