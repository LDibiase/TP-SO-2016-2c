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
#include <pthread.h>
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
	t_ubicacion ubicacion;
	char ejeAnterior;

	void _obtenerObjetivo(char* objetivo) {
		// Inicializar ubicación de la PokéNest a la que se desea llegar
		t_ubicacion ubicacionPokeNest;

		ubicacionPokeNest.x = 0;
		ubicacionPokeNest.y = 0;

		// Mientras que no se haya alcanzado la ubicación de la PokéNest a la que se desea llegar
		while((ubicacion.x != ubicacionPokeNest.x || ubicacion.y != ubicacionPokeNest.y)) {
			// Recibir mensaje TURNO
			mensaje_t mensajeTurno;

			mensajeTurno.tipoMensaje = TURNO;
			recibirMensaje(mapa_s, &mensajeTurno);
			if(mapa_s->error != NULL)
			{
				log_info(logger, mapa_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
				eliminarSocket(mapa_s);
				log_destroy(logger);
				exit(EXIT_FAILURE);
			}

			if(mensajeTurno.tipoMensaje == TURNO)
			{
				log_info(logger, "Socket %d: se le ha concedido un turno", mapa_s->descriptor);

				if(ubicacionPokeNest.x == 0 && ubicacionPokeNest.y == 0)
				{
					// Determinar ubicación de la PokéNest a la que se desea llegar
					solicitarUbicacionPokeNest(mapa_s, *objetivo, &ubicacionPokeNest);
					if(mapa_s->error != NULL)
					{
						eliminarSocket(mapa_s);
						log_destroy(logger);
						exit(EXIT_FAILURE);
					}
				}
				else
				{
					// Desplazar entrenador
					solicitarDesplazamiento(mapa_s, &ubicacion, ubicacionPokeNest, &ejeAnterior);
					if(mapa_s->error != NULL)
					{
						eliminarSocket(mapa_s);
						log_destroy(logger);
						exit(EXIT_FAILURE);
					}
				}

				// TODO Analizar otros posibles casos
			}
		}

		// Una vez alcanzada la ubicación de la PokéNest

		// Recibir mensaje TURNO
		mensaje_t mensajeTurno;

		mensajeTurno.tipoMensaje = TURNO;
		recibirMensaje(mapa_s, &mensajeTurno);
		if(mapa_s->error != NULL)
		{
			log_info(logger, mapa_s->error);
			log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
			eliminarSocket(mapa_s);
			log_destroy(logger);
			exit(EXIT_FAILURE);
		}

		if(mensajeTurno.tipoMensaje == TURNO)
		{
			log_info(logger, "Socket %d: se le ha concedido un turno", mapa_s->descriptor);

			// Capturar Pokémon
			solicitarCaptura(mapa_s);
			if(mapa_s->error != NULL)
			{
				eliminarSocket(mapa_s);
				log_destroy(logger);
				exit(EXIT_FAILURE);
			}
		}
	}

	void _obtenerObjetivosCiudad(t_ciudad_objetivos* ciudad) {
		int conectado;

		// Conexión al mapa
		mapa_s = conectarAMapa("127.0.0.1", "3490");
		if(mapa_s->error != NULL)
		{
			eliminarSocket(mapa_s);
			log_destroy(logger);
			exit(EXIT_FAILURE);
		}

		// El entrenador está conectado al mapa
		conectado = 1;

		pthread_t hiloSignal;
		pthread_attr_t atributosHiloSignal;

		//Lanzo hilo de seniales
		pthread_attr_init(&atributosHiloSignal);
		pthread_create(&hiloSignal, &atributosHiloSignal, (void*) signal_handler, NULL);
		pthread_attr_destroy(&atributosHiloSignal);

		// Determinar la ubicación inicial del entrenador en el mapa
		ubicacion.x = 1;
		ubicacion.y = 1;

		// Determinar el eje de movimiento anterior arbitrariamente
		ejeAnterior = 'x';

		while(conectado) {
			string_iterate_lines(ciudad->Objetivos, (void*) _obtenerObjetivo);

			// Al finalizar la recolección de objetivos dentro del mapa, el entrenador informa al mapa sobre la
			// situación y se desconecta

			// Recibir mensaje TURNO
			mensaje_t mensajeTurno;

			mensajeTurno.tipoMensaje = TURNO;
			recibirMensaje(mapa_s, &mensajeTurno);
			if(mapa_s->error != NULL)
			{
				log_info(logger, mapa_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
				eliminarSocket(mapa_s);
				log_destroy(logger);
				exit(EXIT_FAILURE);
			}

			if(mensajeTurno.tipoMensaje == TURNO)
			{
				log_info(logger, "Socket %d: se le ha concedido un turno", mapa_s->descriptor);

				// Enviar mensaje OBJETIVOS_COMPLETADOS
				paquete_t paquete;
				mensaje_t mensajeObjetivosCompletos;

				mensajeObjetivosCompletos.tipoMensaje = OBJETIVOS_COMPLETADOS;
				crearPaquete((void*) &mensajeObjetivosCompletos, &paquete);
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

				log_info(logger, "Se han completado todos los objetivos dentro del mapa %s", ciudad->Nombre);

				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
				eliminarSocket(mapa_s);

				conectado = 0;
			}
		}

		while(1);
	}

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	/* Cargar configuración */
	log_info(logger, "Cargando archivo de configuración");
	if (cargarConfiguracion(&configEntrenador) == 1)
		return EXIT_FAILURE;

	log_info(logger, "El nombre es: %s \n", configEntrenador.Nombre);
	log_info(logger, "El simbolo es: %s \n", configEntrenador.Simbolo);
	log_info(logger, "Las vidas son: %d \n", configEntrenador.Vidas);
	log_info(logger, "Los reintentos son: %d \n", configEntrenador.Reintentos);

	list_iterate(configEntrenador.CiudadesYObjetivos, (void*) _obtenerObjetivosCiudad);

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
	mensaje1_t mensajeConexionEntrenador;

	mensajeConexionEntrenador.tipoMensaje = CONEXION_ENTRENADOR;
	mensajeConexionEntrenador.tamanioNombreEntrenador = strlen(configEntrenador.Nombre) + 1;
	mensajeConexionEntrenador.nombreEntrenador = configEntrenador.Nombre;
	mensajeConexionEntrenador.simboloEntrenador = *configEntrenador.Simbolo;

	crearPaquete((void*) &mensajeConexionEntrenador, &paquete);
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
	mensaje_t mensajeAceptaConexion;

	mensajeAceptaConexion.tipoMensaje = ACEPTA_CONEXION;
	recibirMensaje(mapa_s, &mensajeAceptaConexion);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return mapa_s;
	}

	switch(mensajeAceptaConexion.tipoMensaje) {
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
	mensaje5_t mensajeSolicitaUbicacion;

	mensajeSolicitaUbicacion.tipoMensaje = SOLICITA_UBICACION;
	mensajeSolicitaUbicacion.idPokeNest = idPokeNest;

	crearPaquete((void*) &mensajeSolicitaUbicacion, &paquete);
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

	log_info(logger, "Se solicita la ubicación de la PokéNest %c", idPokeNest);

	// Recibir mensaje BRINDA_UBICACION
	mensaje6_t mensajeBrindaUbicacion;

	mensajeBrindaUbicacion.tipoMensaje = BRINDA_UBICACION;
	recibirMensaje(mapa_s, &mensajeBrindaUbicacion);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	if(mensajeBrindaUbicacion.tipoMensaje == BRINDA_UBICACION)
	{
		ubicacionPokeNest->x = mensajeBrindaUbicacion.ubicacionX;
		ubicacionPokeNest->y = mensajeBrindaUbicacion.ubicacionY;

		log_info(logger, "Socket %d: la ubicación de la PokéNest %c es (%d,%d)", mapa_s->descriptor, idPokeNest, ubicacionPokeNest->x, ubicacionPokeNest->y);
	}

	// TODO Analizar otros posibles casos
}

direccion_t calcularMovimiento(t_ubicacion ubicacionEntrenador, t_ubicacion ubicacionPokeNest, char* ejeAnterior) {
	direccion_t direccion;

	if(ubicacionEntrenador.x != ubicacionPokeNest.x && ubicacionEntrenador.y != ubicacionPokeNest.y)
	{
		if(*ejeAnterior == 'x')
		{
			if(ubicacionEntrenador.y > ubicacionPokeNest.y)
				direccion = ARRIBA;
			else
				direccion = ABAJO;
			*ejeAnterior = 'y';
		}
		else if(*ejeAnterior == 'y')
		{
			if(ubicacionEntrenador.x > ubicacionPokeNest.x)
				direccion = IZQUIERDA;
			else
				direccion = DERECHA;
			*ejeAnterior = 'x';
		}
	}
	else if(ubicacionEntrenador.x == ubicacionPokeNest.x)
	{
		if(ubicacionEntrenador.y > ubicacionPokeNest.y)
			direccion = ARRIBA;
		else
			direccion = ABAJO;
	}
	else
	{
		if(ubicacionEntrenador.x > ubicacionPokeNest.x)
			direccion = IZQUIERDA;
		else
			direccion = DERECHA;
	}

	return direccion;
}

void solicitarDesplazamiento(socket_t* mapa_s, t_ubicacion* ubicacion, t_ubicacion ubicacionPokeNest, char* ejeAnterior) {
	direccion_t movimiento;
	movimiento = calcularMovimiento(*ubicacion, ubicacionPokeNest, ejeAnterior);

	// Enviar mensaje SOLICITA_DESPLAZAMIENTO
	paquete_t paquete;
	mensaje7_t mensajeSolicitaDesplazamiento;

	mensajeSolicitaDesplazamiento.tipoMensaje = SOLICITA_DESPLAZAMIENTO;
	mensajeSolicitaDesplazamiento.direccion = movimiento;

	crearPaquete((void*) &mensajeSolicitaDesplazamiento, &paquete);
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

	log_info(logger, "Se solicita desplazamiento");

	// Recibir mensaje CONFIRMA_DESPLAZAMIENTO
	mensaje8_t mensajeConfirmaDesplazamiento;

	mensajeConfirmaDesplazamiento.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;
	recibirMensaje(mapa_s, &mensajeConfirmaDesplazamiento);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	if(mensajeConfirmaDesplazamiento.tipoMensaje == CONFIRMA_DESPLAZAMIENTO)
	{
		log_info(logger, "Socket %d: ha sido desplazado", mapa_s->descriptor);

		ubicacion->x = mensajeConfirmaDesplazamiento.ubicacionX;
		ubicacion->y = mensajeConfirmaDesplazamiento.ubicacionY;

		log_info(logger, "Socket %d: su ubicación actual es (%d,%d)", mapa_s->descriptor, ubicacion->x, ubicacion->y);
	}

	// TODO Analizar otros posibles casos
}

 void solicitarCaptura(socket_t* mapa_s) {
	// Enviar mensaje SOLICITA_CAPTURA
	paquete_t paquete;
	mensaje_t mensajeSolicitaCaptura;

	mensajeSolicitaCaptura.tipoMensaje = SOLICITA_CAPTURA;
	crearPaquete((void*) &mensajeSolicitaCaptura, &paquete);
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

	log_info(logger, "Se solicita capturar un Pokémon", mapa_s->descriptor);

	// Recibir mensaje CONFIRMA_CAPTURA
	mensaje_t mensajeConfirmaCaptura;

	mensajeConfirmaCaptura.tipoMensaje = CONFIRMA_CAPTURA;
	recibirMensaje(mapa_s, &mensajeConfirmaCaptura);
	if(mapa_s->error != NULL)
	{
		log_info(logger, mapa_s->error);
		log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);
		return;
	}

	if(mensajeConfirmaCaptura.tipoMensaje == CONFIRMA_CAPTURA)
	{
		log_info(logger, "Socket %d: ha capturado el Pokémon solicitado", mapa_s->descriptor);

		// TODO Agregar lógica para obtener metadata del pokémon que hemos atrapado (determinar si el mapa nos la provee o si la debemos recuperar del FS)
	}
 }

void signal_handler() {
 	 struct sigaction sa;
 	 // Print pid, so that we can send signals from other shells
 	 printf("My pid is: %d\n", getpid());

 	 // Setup the sighub handler
 	 sa.sa_handler = &signal_termination_handler;

 	 // Restart the system call, if at all possible
 	 sa.sa_flags = SA_RESTART;

 	 // Block every signal during the handler
 	 sigfillset(&sa.sa_mask);

 	 if (sigaction(SIGUSR1, &sa, NULL) == -1) {
 	    perror("Error: cannot handle SIGUSR1"); // Should not happen
 	 }


 	 if (sigaction(SIGTERM, &sa, NULL) == -1) {
 	    perror("Error: cannot handle SIGINT"); // Should not happen
 	 }
}

void signal_termination_handler(int signum) {
 	switch (signum) {
 	        case SIGTERM:
 	        	configEntrenador.Vidas--;
 	        	log_info(logger, "Vida perdida por signal: %d \n", configEntrenador.Vidas);
 	        	printf("Vidas Restantes: %d\n", configEntrenador.Vidas);
 	            break;
 	        case SIGUSR1:
 	        	configEntrenador.Vidas++;
 	        	log_info(logger, "Vida obtenida por signal: %d \n", configEntrenador.Vidas);
 	        	printf("Vidas Restantes: %d\n", configEntrenador.Vidas);
 	            break;
 	        default:
 	            fprintf(stderr, "Codigo Invalido: %d\n", signum);
 	            return;
 	}
}
