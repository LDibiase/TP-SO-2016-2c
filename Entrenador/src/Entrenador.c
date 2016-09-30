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
#include <string.h>
#include <signal.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "socket.h" // BORRAR
#include "protocoloMapaEntrenador.h" // BORRAR
#include "Entrenador.h"
#include "nivel.h"

/* Variables */
t_log* logger;
t_entrenador_config configEntrenador;

int main(void) {
	struct socket* serv_socket_s;

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	/*Cargar Configuración*/
	log_info(logger, "Cargando archivo de configuración");
	cargarConfiguracion(&configEntrenador);

	//VAMOS A VER SI FUNCIONA
	log_info(logger, "El nombre es: %s \n", configEntrenador.Nombre);
	log_info(logger, "El simbolo es: %s \n", configEntrenador.Simbolo);
	log_info(logger, "Las vidas son: %d \n", configEntrenador.Vidas);
	log_info(logger, "Los reintentos son: %d \n", configEntrenador.Reintentos);

	t_ciudad_objetivos* test = list_get(configEntrenador.CiudadesYObjetivos, 0);
	log_info(logger, "La ciudad es: %s \n", test->Nombre);

	serv_socket_s = conectarAServidor("127.0.0.1", "3490");
	if(serv_socket_s->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, serv_socket_s->error);
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	log_info(logger, "Conexión exitosa");

	// HANDSHAKE

	// Enviar mensaje CONEXION_ENTRENADOR
	paquete_t paquete;
	mensaje1_t mensaje1;

	mensaje1.tipoMensaje = CONEXION_ENTRENADOR;
	mensaje1.tamanioNombreEntrenador = strlen(configEntrenador.Nombre) + 1;
	mensaje1.nombreEntrenador = configEntrenador.Nombre;
	mensaje1.simboloEntrenador = *configEntrenador.Simbolo;

	crearPaquete((void*) &mensaje1, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, "Conexión mediante socket %d finalizada", serv_socket_s->descriptor);
		eliminarSocket(serv_socket_s);
		log_destroy(logger);
		return EXIT_FAILURE; // TODO Decidir si sale o si se realiza alguna otra acción y simplemente se limpia el error asociado
	}

	enviarMensaje(serv_socket_s, paquete);
	if(serv_socket_s->error != NULL)
	{
		log_info(logger, serv_socket_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", serv_socket_s->descriptor);
		eliminarSocket(serv_socket_s);
		log_destroy(logger);
		return EXIT_FAILURE; // TODO Decidir si sale o si se realiza alguna otra acción y simplemente se limpia el error asociado al envío
	}

	// Recibir mensaje ACEPTA_CONEXION
	mensaje_t mensaje;

	mensaje.tipoMensaje = ACEPTA_CONEXION;
	recibirMensaje(serv_socket_s, &mensaje);
	if(serv_socket_s->error != NULL)
	{
		log_info(logger, serv_socket_s->error); // TODO Decidir si sale o si se realiza alguna otra acción y simplemente se limpia el error asociado a la recepción
	}

	if(mensaje.tipoMensaje == RECHAZA_CONEXION)
		log_info(logger, "Socket %d: su conexión ha sido rechazada", serv_socket_s->descriptor);
	else if(mensaje.tipoMensaje == ACEPTA_CONEXION)
		log_info(logger, "Socket %d: su conexión ha sido aceptada", serv_socket_s->descriptor);

	while(1); // Para pruebas

	log_info(logger, "Conexión mediante socket %d finalizada", serv_socket_s->descriptor);
	eliminarSocket(serv_socket_s);
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
			t_ciudad_objetivos* ciudadObjetivos;
			ciudadObjetivos = malloc(sizeof(t_ciudad_objetivos));

			char** arrayObjetivos;
			arrayObjetivos = config_get_array_value(config, stringObjetivo);

			ciudadObjetivos->Nombre = strdup(ciudad);
			ciudadObjetivos->Objetivos = arrayObjetivos;

			list_add(structConfig->CiudadesYObjetivos, ciudadObjetivos);
			free(stringObjetivo);
		}
		string_iterate_lines(hojaDeViaje, (void*) _auxIterate);
		log_info(logger, "El archivo de configuración se cargó correctamente");
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

void signal_handler(int signal) {
    const char *signal_name;
    sigset_t pending;

    switch (signal) {
        case SIGTERM:
            //Sacar una vida
            break;
        case SIGUSR1:
            //Dar una vida
            break;
        default:
            fprintf(stderr, "Codigo Invalido: %d\n", signal);
            return;
    }
}
