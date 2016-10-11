/*
 * PokeDexServidor.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef POKEDEXSERVIDOR_H_
#define POKEDEXSERVIDOR_H_

/* Definición de estructuras */

/* Declaración de funciones */
void leerArchivo(int archivoID, char* ruta);
void escribirEstructura(int dirPadre, char* ruta);

// Acepta múltiples conexiones de clientes
void aceptarConexiones();

#endif /* POKEDEXSERVIDOR_H_ */
