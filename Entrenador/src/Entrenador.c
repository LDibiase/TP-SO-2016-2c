/*
 ============================================================================
 Name        : Entrenador.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Entrenador
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "Entrenador.h"
#include "nivel.h"
#include <string.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

/* Variables */
t_log* logger;
t_entrenador_config configEntrenador;

int main(void) {

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	/*Cargar Configuración*/
	log_info(logger, "Cargando archivo configuración");
	int res = cargarConfiguracion(&configEntrenador);

	//VAMOS A VER SI FUNCIONA
	printf("El nombre es: %s \n", configEntrenador.Nombre);
	printf("El simbolo es: %s \n", configEntrenador.Simbolo);
	printf("Las vidas son: %d \n", configEntrenador.Vidas);
	printf("Los reintentos son: %d \n", configEntrenador.Reintentos);

	t_ciudad_objetivos* test = list_get(configEntrenador.CiudadesYObjetivos, 0);
	printf("La ciudad es: %s", test->Nombre);


	//puts("Proceso Entrenador"); /* prints Proceso Entrenador */
	log_destroy(logger);
	return EXIT_SUCCESS;
}

int cargarConfiguracion(t_entrenador_config* structConfig)
{
	t_config* config;
	config = config_create(CONFIG_FILE_PATH);

	if(config_has_property(config, "nombre")
			&& config_has_property(config, "simbolo")
			&& config_has_property(config, "hojaDeViaje")
			&& config_has_property(config, "vidas")
			&& config_has_property(config, "reintentos"))
	{

		structConfig->CiudadesYObjetivos = list_create();
		char** hojaDeViaje = config_get_array_value(config, "hojaDeViaje");

		structConfig->Nombre = strdup(config_get_string_value(config, "nombre"));
		structConfig->Simbolo = strdup(config_get_string_value(config, "simbolo"));
		structConfig->Vidas = config_get_int_value(config, "vidas");
		structConfig->Reintentos = config_get_int_value(config, "reintentos");

		//SE BUSCAN LOS OBJETIVOS DE CADA CIUDAD
		void _auxIterate(char* ciudad)
		{
			char* stringObjetivo = string_from_format("obj[%s]", ciudad);
			t_ciudad_objetivos* ciudadesObjetivos;
			ciudadesObjetivos = malloc(sizeof(t_ciudad_objetivos));

			char* arrayObjetivos;
			arrayObjetivos = (char*)config_get_array_value(config, stringObjetivo);

			ciudadesObjetivos->Nombre = strdup(ciudad);
			ciudadesObjetivos->Objetivos = arrayObjetivos;

			list_add(structConfig->CiudadesYObjetivos, ciudadesObjetivos);
			free(stringObjetivo);
		}
		string_iterate_lines(hojaDeViaje, (void*)_auxIterate);
		log_info(logger, "El archivo de configuración se cargo correctamente");
		config_destroy(config);
		return 0;
	}
	else
	{
		log_error(logger, "El archivo de configuración tiene un formato inválido");
		config_destroy(config);
		return 1;
	}
}
