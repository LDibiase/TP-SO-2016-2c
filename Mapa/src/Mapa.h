/*
 * Mapa.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef MAPA_H_
#define MAPA_H_

/* Definición de estructuras */

typedef struct mapa
{
	int TiempoChequeoDeadlock;
	int Batalla;
	char* Algoritmo;
	int Quantum;
	int Retardo;
	char* IP;
	char* Puerto;
}t_mapa_config;

typedef struct ubicacion {
	int x;
	int y;
} t_mapa_pos;

typedef struct personaje {
	char id;
	t_mapa_pos pos;
} t_mapa_pj;

/* Constantes */


// Ruta al archivo de log
#define LOG_FILE_PATH "mapa.log"
// Ruta al archivo de configuración
#define CONFIG_FILE_PATH "config.cfg"


/* Declaración de funciones */

int cargarConfiguracion();
t_list* cargarPokenest();
t_mapa_pos buscarPokenest(t_list* lista, char pokemon);
void realizar_movimiento(t_list* items,t_mapa_pj personaje, char * mapa);
ITEM_NIVEL *find_by_id(t_list* lista, char idBuscado);


#endif /* MAPA_H_ */
