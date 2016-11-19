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

typedef struct getattr {
	int tipoArchivo;
	int tamanioArchivo;
} t_getattr;

typedef struct block {
	char* block;
	int size;
} t_block;

// OperacionOSADA
typedef struct operacionOSADA {
	void* operacion;
	t_pokedex_cliente* socket;
} operacionOSADA;

//Mensaje Pokedex Cliente-Servidor
typedef struct mensajePokedex {
	uint32_t tipoMensaje;
	uint32_t ruta;
	uint32_t nombre;
} mensajeDePokedex;

// Mensajes
typedef enum tipoMensajePokedex {
	CONEXION_A_SERVIDOR
} tipoMensaje_pokedex;

/* Declaración de funciones */
void leerArchivo(int archivoID, char* ruta);
void escribirEstructura(int dirPadre, char* ruta);
t_getattr getattr_callback(const char *path);
char* readdir_callback(const char *path);
void pokedexCliente(t_pokedex_cliente* pokedex_cliente);
int getDirPadre(const char *path);
t_block read_callback(const char *path, int offset, int tamanioBuffer);
int mkdir_callback(const char *path);
int get_firstEntry();
int rmdir_callback(const char *path);
int unlink_callback(const char *path);
int mknod_callback(const char *path);
int getFirstBit();
int write_callback(const char* path, int offset, char* buffer, int tamanioBuffer);


// Acepta múltiples conexiones de clientes
void aceptarConexiones();
#endif /* POKEDEXSERVIDOR_H_ */
