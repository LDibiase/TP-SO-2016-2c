/*
 * Entrenador.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef ENTRENADOR_H_
#define ENTRENADOR_H_
#include <commons/collections/list.h>
#include "socket.h" // BORRAR

/* Definición de estructuras */

typedef struct ciudadObjetivos {
	char* Nombre;
	char** Objetivos;
} t_ciudad_objetivos;

typedef struct entrenador {
	char* Nombre;
	char* Simbolo;
	t_list* CiudadesYObjetivos;
	int Vidas;
	int Reintentos;
} t_entrenador_config;


/* Constantes */

// Ruta al archivo de log
#define LOG_FILE_PATH "entrenador.log"
// Ruta al archivo de configuración
#define CONFIG_FILE_PATH "config.cfg"


/* Declaración de funciones */

int cargarConfiguracion(t_entrenador_config* structConfig);
socket_t* conectarAMapa(char* ip, char* puerto);
void solicitarUbicacionPokeNest(socket_t* mapa_s, char idPokeNest, t_ubicacion* ubicacionPokeNest);
direccion_t calcularMovimiento(t_ubicacion ubicacionEntrenador, t_ubicacion ubicacionPokeNest, char* ejeAnterior);
void solicitarDesplazamiento(socket_t* mapa_s, t_ubicacion* ubicacion, t_ubicacion ubicacionPokeNest, char* ejeAnterior);
void solicitarCaptura(socket_t* mapa_s, int* conectado);
void signal_handler(int signal);

#endif /* ENTRENADOR_H_ */
