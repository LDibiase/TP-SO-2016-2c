/*
 * Mapa.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef MAPA_H_
#define MAPA_H_
#include "socket.h"

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
} t_mapa_pokenest;


/* Constantes */

// Ruta al archivo de log
#define LOG_FILE_PATH "mapa.log"
// Ruta al archivo de configuración
#define CONFIG_FILE_PATH "config.cfg"


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

#endif /* MAPA_H_ */
