/*
 * Entrenador.h
 *
 *  Created on: 30/8/2016
 *      Author: utnso
 */

#ifndef ENTRENADOR_H_
#define ENTRENADOR_H_

/* Definición de estructuras */

typedef struct entrenador
{
	char* Nombre;
	char* Simbolo;
	char** HojaDeViaje;
	char** Objetivos;
	int Vidas;
	int Reintentos;

}t_entrenador_config;

/* Constantes */

// Ruta al archivo de log
#define LOG_FILE_PATH "entrenadorLog.log"
// Ruta al archivo de configuración
#define CONFIG_FILE_PATH "config.cfg"

/* Declaración de funciones */

#endif /* ENTRENADOR_H_ */
