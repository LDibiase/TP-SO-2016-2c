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
/* Definición de estructuras */

static struct fuse_operations fuse_oper = {
		.getattr = fuse_getattr,
		.readdir = fuse_readdir,
		.open = fuse_open,
		.read = fuse_read,
};

/* Declaración de funciones */



#endif /* POKEDEXCLIENTE_H_ */
