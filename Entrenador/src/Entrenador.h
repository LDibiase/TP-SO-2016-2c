/*
 * Entrenador.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef ENTRENADOR_H_
#define ENTRENADOR_H_
#include <commons/collections/list.h>

/* Definición de estructuras */

typedef struct ciudadObjetivos
{
	char* Nombre;
	char** Objetivos;
}t_ciudad_objetivos;

typedef struct entrenador
{
	char* Nombre;
	char* Simbolo;
	t_list* CiudadesYObjetivos;
	int Vidas;
	int Reintentos;
}t_entrenador_config;


/* Constantes */

// Ruta al archivo de log
#define LOG_FILE_PATH "entrenador.log"
// Ruta al archivo de configuración
#define CONFIG_FILE_PATH "config.cfg"


/* Declaración de funciones */

int cargarConfiguracion(t_entrenador_config* structConfig);

#endif /* ENTRENADOR_H_ */
