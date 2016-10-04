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
#include "protocoloMapaEntrenador.h" // BORRAR
#include "Entrenador.h"
#include "nivel.h"

/* Variables globales */
t_log* logger;
t_entrenador_config configEntrenador;

int main(void) {
	socket_t* mapa_s;
	int conectado;
	t_ubicacion ubicacion;
	char ejeAnterior;

	int error;
	error = -1;

	void _obtenerObjetivo(char* objetivo) {
		// Inicializar ubicación de la PokéNest a la que se desea llegar
		t_ubicacion ubicacionPokeNest;

		ubicacionPokeNest.x = 0;
		ubicacionPokeNest.y = 0;

		// Mientras que no se haya alcanzado la ubicación de la PokéNest a la que se desea llegar
		//while(ubicacion.x != ubicacionPokeNest.x || ubicacion.y != ubicacionPokeNest.y) {
			// Recibir mensaje TURNO
			mensaje_t mensaje4;

			mensaje4.tipoMensaje = TURNO;
			recibirMensaje(mapa_s, &mensaje4);
			if(mapa_s->error != NULL)
			{
				log_info(logger, mapa_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
				eliminarSocket(mapa_s);
				log_destroy(logger);
				exit(error);
			}

			if(mensaje4.tipoMensaje == TURNO)
			{
				if(ubicacionPokeNest.x == 0 && ubicacionPokeNest.y == 0)
				{
					// Determinar ubicación de la PokéNest a la que se desea llegar
					solicitarUbicacionPokeNest(mapa_s, *objetivo, &ubicacionPokeNest);
					if(mapa_s->error != NULL)
					{
						eliminarSocket(mapa_s);
						log_destroy(logger);
						exit(error);
					}
				}
				else
				{
					solicitarDesplazamiento(mapa_s, &ubicacion, ubicacionPokeNest, &ejeAnterior);
					if(mapa_s->error != NULL)
					{
						eliminarSocket(mapa_s);
						log_destroy(logger);
						exit(error);
					}
				}

				// TODO Analizar otros posibles casos
			}
		//}

		// Una vez alcanzada la ubicación de la PokéNest

		// Recibir mensaje TURNO
		//mensaje_t mensaje4;

		mensaje4.tipoMensaje = TURNO;
		recibirMensaje(mapa_s, &mensaje4);
		if(mapa_s->error != NULL)
		{
			log_info(logger, mapa_s->error);
			log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
			eliminarSocket(mapa_s);
			log_destroy(logger);
			exit(error);
		}

		if(mensaje4.tipoMensaje == TURNO)
		{
			solicitarCaptura(mapa_s);
			if(mapa_s->error != NULL)
			{
				eliminarSocket(mapa_s);
				log_destroy(logger);
				exit(error);
			}
		}
	}

	void _obtenerObjetivosCiudad(t_ciudad_objetivos* ciudad) {
		// El entrenador está conectado al mapa
		conectado = 1;

		// Determinar la ubicación inicial del entrenador en el mapa
		ubicacion.x = 1;
		ubicacion.y = 1;

		// Determinar el eje de movimiento anterior arbitrariamente
		ejeAnterior = 'x';

		while(conectado) {
			string_iterate_lines(ciudad->Objetivos, (void*) _obtenerObjetivo);

			// Al finalizar la recolección de objetivos dentro del mapa, el entrenador se desconecta

			// Enviar mensaje OBJETIVOS_COMPLETADOS
			paquete_t paquete;
			mensaje_t mensaje11;

			mensaje11.tipoMensaje = OBJETIVOS_COMPLETADOS;
			crearPaquete((void*) &mensaje11, &paquete);
			if(paquete.tamanioPaquete == 0)
			{
				mapa_s->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, mapa_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
				return;
			}

			enviarMensaje(mapa_s, paquete);
			if(mapa_s->error != NULL)
			{
				log_info(logger, mapa_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
			    return;
			}

			free(paquete.paqueteSerializado);

			conectado = 0;
		}
	}

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	/* Cargar configuración */
	log_info(logger, "Cargando archivo de configuración");
	cargarConfiguracion(&configEntrenador);

	log_info(logger, "El nombre es: %s \n", configEntrenador.Nombre);
	log_info(logger, "El simbolo es: %s \n", configEntrenador.Simbolo);
	log_info(logger, "Las vidas son: %d \n", configEntrenador.Vidas);
	log_info(logger, "Los reintentos son: %d \n", configEntrenador.Reintentos);

	// Conexión al mapa
	mapa_s = conectarAMapa("127.0.0.1", "3490");
	if(mapa_s->error != NULL)
	{
		eliminarSocket(mapa_s);
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	list_iterate(configEntrenador.CiudadesYObjetivos, (void*) _obtenerObjetivosCiudad);

	log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
	eliminarSocket(mapa_s);
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

socket_t* conectarAMapa(char* ip, char* puerto) {
	socket_t* mapa_s;

	mapa_s = conectarAServidor(ip, puerto);
	if(mapa_s->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, mapa_s->error);
		return mapa_s;
	}

	log_info(logger, "Conexión exitosa");

	///////////////////////
	////// HANDSHAKE //////
	///////////////////////

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
		mapa_s->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return mapa_s;
	}

	enviarMensaje(mapa_s, paquete);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return mapa_s;
	}

	free(paquete.paqueteSerializado);

	// Recibir mensaje ACEPTA_CONEXION
	mensaje_t mensaje;

	mensaje.tipoMensaje = ACEPTA_CONEXION;
	recibirMensaje(mapa_s, &mensaje);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return mapa_s;
	}

	switch(mensaje.tipoMensaje) {
	case RECHAZA_CONEXION:
		mapa_s->error = string_from_format("Socket %d: su conexión ha sido rechazada", mapa_s->descriptor);
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);

		break;
	case ACEPTA_CONEXION:
		log_info(logger, "Socket %d: su conexión ha sido aceptada", mapa_s->descriptor);

		break;
	}

	return mapa_s;
}

void solicitarUbicacionPokeNest(socket_t* mapa_s, char idPokeNest, t_ubicacion* ubicacionPokeNest) {
	// Enviar mensaje SOLICITA_UBICACION
	paquete_t paquete;
	mensaje5_t mensaje5;

	mensaje5.tipoMensaje = SOLICITA_UBICACION;
	mensaje5.idPokeNest = idPokeNest;

	crearPaquete((void*) &mensaje5, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	enviarMensaje(mapa_s, paquete);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	free(paquete.paqueteSerializado);

	// Recibir mensaje BRINDA_UBICACION
	mensaje6_t mensaje6;

	mensaje6.tipoMensaje = BRINDA_UBICACION;
	recibirMensaje(mapa_s, &mensaje6);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	if(mensaje6.tipoMensaje == BRINDA_UBICACION)
	{
		ubicacionPokeNest->x = mensaje6.ubicacionX;
		ubicacionPokeNest->y = mensaje6.ubicacionY;
	}

	log_info(logger, "La ubicación de la PokéNest es (%d,%d)", ubicacionPokeNest->x, ubicacionPokeNest->y);

	// TODO Analizar otros posibles casos
}

direccion_t calcularMovimiento(t_ubicacion ubicacionEntrenador, t_ubicacion ubicacionPokeNest, char* ejeAnterior) {
	direccion_t direccion;

	if(string_equals_ignore_case(ejeAnterior, "x"))
	{
		if(ubicacionEntrenador.y > ubicacionPokeNest.y)
			direccion = ARRIBA;
		else
			direccion = ABAJO;
		ejeAnterior = "y";

	}
	else if(string_equals_ignore_case(ejeAnterior, "y"))
	{
		if(ubicacionEntrenador.x > ubicacionPokeNest.x)
			direccion = DERECHA;
		else
			direccion = IZQUIERDA;
		ejeAnterior = "x";
	}

	return direccion;
}

void solicitarDesplazamiento(socket_t* mapa_s, t_ubicacion* ubicacion, t_ubicacion ubicacionPokeNest, char* ejeAnterior) {
	direccion_t movimiento;
	movimiento = calcularMovimiento(*ubicacion, ubicacionPokeNest, ejeAnterior);

	// Enviar mensaje SOLICITA_DESPLAZAMIENTO
	paquete_t paquete;
	mensaje7_t mensaje7;

	mensaje7.tipoMensaje = SOLICITA_DESPLAZAMIENTO;
	mensaje7.direccion = movimiento;

	crearPaquete((void*) &mensaje7, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	enviarMensaje(mapa_s, paquete);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
	    return;
	}

	free(paquete.paqueteSerializado);

	// Recibir mensaje CONFIRMA_DESPLAZAMIENTO
	mensaje8_t mensaje8;

	mensaje8.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;
	recibirMensaje(mapa_s, &mensaje8);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	if(mensaje8.tipoMensaje == CONFIRMA_DESPLAZAMIENTO)
	{
		ubicacion->x = mensaje8.ubicacionX;
		ubicacion->y = mensaje8.ubicacionY;
	}

	log_info(logger, "La ubicación actual del entrenador es (%d,%d)", ubicacion->x, ubicacion->y);

	// TODO Analizar otros posibles casos
}

 void solicitarCaptura(socket_t* mapa_s) {
	// Enviar mensaje SOLICITA_CAPTURA
	paquete_t paquete;
	mensaje_t mensaje9;

	mensaje9.tipoMensaje = SOLICITA_CAPTURA;
	crearPaquete((void*) &mensaje9, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	enviarMensaje(mapa_s, paquete);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
	    return;
	}

	free(paquete.paqueteSerializado);

	// Recibir mensaje CONFIRMA_CAPTURA
	mensaje_t mensaje10;

	mensaje10.tipoMensaje = CONFIRMA_CAPTURA;
	recibirMensaje(mapa_s, &mensaje10);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	if(mensaje10.tipoMensaje == CONFIRMA_CAPTURA)
	{
		// TODO Agregar lógica para obtener metadata del pokémon que hemos atrapado (determinar si el mapa nos la provee o si la debemos recuperar del FS)
	}
 }

void signal_handler(int signal) {
//    const char *signal_name;
//    sigset_t pending;

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
