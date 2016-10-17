/*
 * PokeDexServidor.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef POKEDEXSERVIDOR_H_
#define POKEDEXSERVIDOR_H_

/* Definición de estructuras */
typedef struct pokedex_cliente {
	char* ruta;
	char* nombre;
	int tamanio;
	socket_t* socket;
} t_pokedex_cliente;

//Mensaje Pokedex Cliente-Servidor
typedef struct mensajePokedex {
	uint32_t tipoMensaje;
	uint32_t ruta;
	uint32_t nombre;
} mensajeDePokedex;

// Mensajes
typedef enum tipoMensajePokedex {
	CONEXION_A_SERVIDOR,
	LEER,
	LISTAR,
	ESCRIBIR,
	ABRIR
} tipoMensaje_pokedex;

/* Declaración de funciones */
void leerArchivo(int archivoID, char* ruta);
void escribirEstructura(int dirPadre, char* ruta);

// Acepta múltiples conexiones de clientes
void aceptarConexiones();
#endif /* POKEDEXSERVIDOR_H_ */
