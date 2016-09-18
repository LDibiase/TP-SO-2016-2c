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
	int cantidad;
} t_mapa_pos;

typedef struct personaje {
	char id;
	char* nombre;
	int faltaEjecutar;
	socket_t* socket;
	t_mapa_pos pos;
} t_mapa_pj;


/* Constantes */

// Ruta al archivo de log
#define LOG_FILE_PATH "mapa.log"
// Ruta al archivo de configuración
#define CONFIG_FILE_PATH "config.cfg"


/* Declaración de funciones */

void realizar_movimiento(t_list* items, t_mapa_pj personaje, char * mapa);
t_mapa_pos calcularMovimiento(t_mapa_pos posActual, t_mapa_pos posFinal);
ITEM_NIVEL *find_by_id(t_list* lista, char idBuscado);
t_mapa_pos buscarPokenest(t_list* lista, char pokemon);
t_list* cargarObjetivos();
t_list* cargarPokenest();
int cargarConfiguracion(t_mapa_config* structConfig);
void aceptarConexiones();

#endif /* MAPA_H_ */
