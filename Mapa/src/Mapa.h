/*
 * Mapa.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef MAPA_H_
#define MAPA_H_
#include "socket.h"
#include <pkmn/battle.h>
#include <pkmn/factory.h>

/* Definición de estructuras */

typedef struct mapa {
	int TiempoChequeoDeadlock;
	int Batalla;
	char* Algoritmo;
	int Quantum;
	int Retardo;
	char* IP;
	char* Puerto;
} t_mapa_config;

typedef struct entrenador {
	char id;
	char idPokenestActual;
	char* nombre;
	int faltaEjecutar;
	int utEjecutadas;
	socket_t* socket;
	t_ubicacion ubicacion;
} t_entrenador;

typedef struct pokenest {
	char* tipo;
	t_ubicacion ubicacion;
	char id;
	int cantidad;
	t_list* metadatasPokemones;
} t_mapa_pokenest;

typedef struct recursosEntrenador {
	char id;
	t_list* recursos;
} t_recursosEntrenador;

typedef struct pokemonEntrenador{
	char id;
	char* nombre;
	char* tipo;
	int poder;
	int nivel;
	char idEntrenador;
	t_pokemon* pokemon;
}t_pokemonEntrenador;

typedef struct metadataPokemon{
	char* rutaArchivo;
	int nivel;
}t_metadataPokemon;

/* Constantes */

// Ruta al archivo de log
#define LOG_FILE_PATH "mapa.log"


/* Declaración de funciones */

void encolarEntrenador(t_entrenador* entrenador);
void reencolarEntrenador(t_entrenador* entrenador);
void calcularFaltante(t_entrenador entrenador);
void insertarOrdenado(t_entrenador* entrenador, t_queue* cola, pthread_mutex_t* mutex);
void insertarAlFinal(t_entrenador* entrenador, t_queue* cola, pthread_mutex_t* mutex);
void realizar_movimiento(t_list* items, t_entrenador personaje, char * mapa);
t_ubicacion calcularMovimiento(t_ubicacion ubicacionActual, t_ubicacion ubicacionFinal);
ITEM_NIVEL* find_by_id(t_list* lista, char idBuscado);
t_ubicacion buscarPokenest(t_list* lista, char pokemon);
t_list* cargarObjetivos();
t_list* cargarPokenests();
t_mapa_pokenest leerPokenest(char* metadata);
int cargarConfiguracion(t_mapa_config* structConfig);
void aceptarConexiones();
void eliminarEntrenador(t_entrenador* entrenador);
bool algoritmoDeteccion();
void eliminarRecursosEntrenador(t_recursosEntrenador* recursosEntrenador);
void liberarMemoriaAlocada();
void eliminarEntrenadorMapa(t_entrenador* entrenadorAEliminar);
void signal_handler();
void signal_termination_handler(int signum);
void chequearDeadlock();
t_pokemonEntrenador obtenerPokemonMayorNivel(t_entrenador* entrenador);
t_pokemonEntrenador* obtenerEntrenadorAEliminar(t_list* entrenadoresConPokemonesAPelear);
int obtenerCantidadRecursos(char* nombrePokemon, char* rutaPokenest, t_list* metadatasPokemones);
void liberarRecursosEntrenador(t_entrenador* entrenador);
void capturarPokemon(t_entrenador* entrenador);
void desbloquearJugadores();
void actualizarSolicitudes(t_entrenador* entrenador);

#endif /* MAPA_H_ */
