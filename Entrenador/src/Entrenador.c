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

int main(void) {
	t_entrenador_config configEntrenador;
	//CARGAR CONFIGURACION
	//int res = cargarConfiguracion(&configEntrenador);
	puts("Proceso Entrenador"); /* prints Proceso Entrenador */
	return EXIT_SUCCESS;
}

int cargarConfiguracion(t_entrenador_config* structConfig)
{
	t_config* config;
	config = config_create("");

	if(config_has_property(config, "nombre")
			&& config_has_property(config, "simbolo")
			&& config_has_property(config, "hojaDeViaje")
			&& config_has_property(config, "vidas")
			&& config_has_property(config, "reintentos"))
	{
		char** hojaDeViaje = config_get_array_value(config, "hojaDeViaje");

		structConfig->Nombre = config_get_string_value(config, "nombre");
		structConfig->Simbolo = config_get_string_value(config, "simbolo");
		structConfig->HojaDeViaje = hojaDeViaje;
		structConfig->Vidas = config_get_int_value(config, "vidas");
		structConfig->Reintentos = config_get_int_value(config, "reintentos");

		//SE BUSCAN LOS OBJETIVOS DE CADA CIUDAD
		void _auxIterate(char* ciudad)
		{
			char* stringObjetivo = string_new();
			string_append(&stringObjetivo, "obj[");
			string_append(&stringObjetivo, ciudad);
			string_append(&stringObjetivo,"]");

			int i = 0;

			char* arrayObjetivos;
			arrayObjetivos = (char*)config_get_array_value(config, stringObjetivo);

			while(structConfig->Objetivos[i] != NULL)
			{
				i++;
			}
			structConfig->Objetivos = realloc(structConfig->Objetivos, (i + 1)*sizeof(char*));
			structConfig->Objetivos[i] = arrayObjetivos;

			free(stringObjetivo);
		}
		string_iterate_lines(hojaDeViaje, (void*)_auxIterate);
		return 0;
	}
	else
	{
		return 1;
	}
}
