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



/* Declaración de funciones */



#endif /* POKEDEXCLIENTE_H_ */
