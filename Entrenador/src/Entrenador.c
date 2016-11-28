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
#include "Entrenador.h"
#include "nivel.h"

/* Variables globales */
t_log* logger;							// Archivo de log
t_entrenador_config configEntrenador;	// Datos de configuración
int activo;							 	// Flag de actividad del entrenador
char* puntoMontajeOsada;         	  	// Punto de montaje del FS
char* rutaDirectorioEntrenador;         // Ruta del directorio del entrenador

socket_t* mapa_s;

//SEMÁFORO PARA SINCRONIZAR EL ARCHIVO DE LOG
//pthread_mutex_t mutexLog;

int main(int argc, char **argv) {
	t_ubicacion ubicacion;
	char ejeAnterior;
	int objetivosCompletados;
	int victima;

	//DANDOLE FORMA A LOS PARAMETROS RECIBIDOS
	puntoMontajeOsada = strdup(argv[1]);
	rutaDirectorioEntrenador = strdup(argv[1]);
	string_append(&rutaDirectorioEntrenador, "/Entrenadores/");
	string_append(&rutaDirectorioEntrenador, argv[2]);
	string_append(&rutaDirectorioEntrenador, "/");

	void _obtenerObjetivo(char* objetivo) {
		if(!victima)
		{
			// Inicializar ubicación de la PokéNest a la que se desea llegar
			t_ubicacion ubicacionPokeNest;

			ubicacionPokeNest.x = 0;
			ubicacionPokeNest.y = 0;

			// Mientras que no se haya alcanzado la ubicación de la PokéNest a la que se desea llegar
			while((ubicacion.x != ubicacionPokeNest.x || ubicacion.y != ubicacionPokeNest.y) && activo) {
				if(ubicacionPokeNest.x == 0 && ubicacionPokeNest.y == 0)
				{
					// Determinar ubicación de la PokéNest a la que se desea llegar
					solicitarUbicacionPokeNest(mapa_s, *objetivo, &ubicacionPokeNest);
					if(mapa_s->error != NULL)
					{
						eliminarSocket(mapa_s);
						log_destroy(logger);
//						pthread_mutex_destroy(&mutexLog);
						abort();
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
//						pthread_mutex_destroy(&mutexLog);
						exit(EXIT_FAILURE);
					}
				}
			}

			if(activo == 0)
			{
				log_info(logger, "El jugador ha abandonado el juego");

				eliminarSocket(mapa_s);
				log_destroy(logger);
//				pthread_mutex_destroy(&mutexLog);
				exit(EXIT_FAILURE);
			}

			// Una vez alcanzada la ubicación de la PokéNest, capturar Pokémon
			solicitarCaptura(mapa_s, &victima);
			if(mapa_s->errorCode != NO_ERROR)
			{
				eliminarSocket(mapa_s);
				log_destroy(logger);
//				pthread_mutex_destroy(&mutexLog);
				exit(EXIT_FAILURE);
			}
		}
	}

	void _obtenerObjetivosCiudad(t_ciudad_objetivos* ciudad) {
		char* ip = string_new();
		char* puerto = string_new();

		objetivosCompletados = 0;

		// Obtener datos de conexión del mapa
		obtenerDatosConexion(ciudad->Nombre, ip, puerto);

		while(configEntrenador.Vidas > 0 && activo && !objetivosCompletados) {
			// Conexión al mapa
			mapa_s = conectarAMapa(ip, puerto);

			if(mapa_s->errorCode != NO_ERROR)
			{
				eliminarSocket(mapa_s);
				log_destroy(logger);
//				pthread_mutex_destroy(&mutexLog);
				exit(EXIT_FAILURE);
			}

			// Determinar la ubicación inicial del entrenador en el mapa
			ubicacion.x = 1;
			ubicacion.y = 1;

			// Determinar el eje de movimiento anterior arbitrariamente
			ejeAnterior = 'x';

			string_iterate_lines(ciudad->Objetivos, (void*) _obtenerObjetivo);

			if(victima == 0)
			{
				objetivosCompletados = 1;

				// Al finalizar la recolección de objetivos dentro del mapa, el entrenador se desconecta
				log_info(logger, "Se han completado todos los objetivos dentro del mapa %s", ciudad->Nombre);
				log_info(logger, "Conexión mediante socket %d finalizada", mapa_s->descriptor);

				eliminarSocket(mapa_s);
			}
		}

		free(ip);
		free(puerto);

		if(configEntrenador.Vidas == 0)
		{
			log_info(logger, "¿Desea reiniciar el juego? (Y/N)");

			// TODO: A continuación se debería aguardar por la respuesta del usuario; en caso de que ésta sea negativa...
			activo = 0;
		}

		if(activo == 0)
		{
			log_info(logger, "El jugador ha abandonado el juego");

			eliminarSocket(mapa_s);
			log_destroy(logger);
//			pthread_mutex_destroy(&mutexLog);
			exit(EXIT_FAILURE);
		}
	}

	// Variables para la creación del hilo para el manejo de señales
//	pthread_t hiloSignalHandler;
//	pthread_attr_t atributosHiloSignalHandler;

	//INICIALIZACIÓN DE LOS SEMÁFOROS
//	pthread_mutex_init(&mutexLog, NULL);

	// Creación del archivo de log
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	// Cargar configuración
	log_info(logger, "Cargando archivo de configuración");

	if (cargarConfiguracion(&configEntrenador) == 1)
	{
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");
		log_destroy(logger);
//		pthread_mutex_destroy(&mutexLog);
		return EXIT_FAILURE;
	}

	log_info(logger, "Nombre: %s", configEntrenador.Nombre);
	log_info(logger, "Símbolo: %s", configEntrenador.Simbolo);
	log_info(logger, "Vidas restantes: %d", configEntrenador.Vidas);

	// Se setea en 1 (on) el flag de actividad
	activo = 1;

	// Creación del hilo para el manejo de señales
//	pthread_attr_init(&atributosHiloSignalHandler);
//	pthread_create(&hiloSignalHandler, &atributosHiloSignalHandler, (void*) signal_handler, NULL);
//	pthread_attr_destroy(&atributosHiloSignalHandler);

	// Obtención de los objetivos propios de cada mapa (ciudad) incluido en la Hoja de Viaje del entrenador
	list_iterate(configEntrenador.CiudadesYObjetivos, (void*) _obtenerObjetivosCiudad);

	log_info(logger, "La ejecución del proceso Entrenador ha finalizado correctamente");

	log_destroy(logger);
//	pthread_mutex_destroy(&mutexLog);
	return EXIT_SUCCESS;
}

int cargarConfiguracion(t_entrenador_config* structConfig) {
	t_config* config;
	char* rutaMetadataEntrenador;

	rutaMetadataEntrenador = strdup(rutaDirectorioEntrenador);
	string_append(&rutaMetadataEntrenador, METADATA);

	config = config_create(rutaMetadataEntrenador);

	if(config != NULL)
	{
		free(rutaMetadataEntrenador);

		if(config_has_property(config, "nombre")
				&& config_has_property(config, "simbolo")
				&& config_has_property(config, "hojaDeViaje")
				&& config_has_property(config, "vidas"))
		{
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

			structConfig->CiudadesYObjetivos = list_create();
			char** hojaDeViaje = config_get_array_value(config, "hojaDeViaje");

			structConfig->Nombre = strdup(config_get_string_value(config, "nombre"));
			structConfig->Simbolo = strdup(config_get_string_value(config, "simbolo"));
			structConfig->Vidas = config_get_int_value(config, "vidas");

			//SE BUSCAN LOS OBJETIVOS DE CADA CIUDAD
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
	else
	{
		log_error(logger, "La ruta de archivo de configuración indicada no existe");
		free(rutaMetadataEntrenador);
		return 1;
	}
}

socket_t* conectarAMapa(char* ip, char* puerto) {
	mapa_s = conectarAServidor(ip, puerto);
	if(mapa_s->errorCode != 0)
	{
		switch(mapa_s->errorCode) {
		case ERR_SERVER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_CLIENT_CANNOT_CONNECT:
			log_info(logger, "No ha sido posible establecer conexión con el servidor");

			break;
		}

		log_info(logger, mapa_s->error);

		return mapa_s;
	}

	log_info(logger, "El proceso Entrenador se ha conectado de manera exitosa al proceso Mapa (socket n° %d)", mapa_s->descriptor);

	///////////////////////
	////// HANDSHAKE //////
	///////////////////////

	// Enviar mensaje CONEXION_ENTRENADOR
	paquete_t paquete;
	mensaje1_t mensajeConexionEntrenador;

	mensajeConexionEntrenador.tipoMensaje = CONEXION_ENTRENADOR;
	mensajeConexionEntrenador.tamanioNombreEntrenador = strlen(configEntrenador.Nombre) + 1;
	mensajeConexionEntrenador.nombreEntrenador = strdup(configEntrenador.Nombre);
	mensajeConexionEntrenador.simboloEntrenador = *configEntrenador.Simbolo;

	crearPaquete((void*) &mensajeConexionEntrenador, &paquete);

	free(mensajeConexionEntrenador.nombreEntrenador);

	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return mapa_s;
	}

	enviarMensaje(mapa_s, paquete);

	free(paquete.paqueteSerializado);

	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return mapa_s;
	}

	// Recibir mensaje ACEPTA_CONEXION
	mensaje_t mensajeAceptaConexion;

	mensajeAceptaConexion.tipoMensaje = ACEPTA_CONEXION;
	recibirMensaje(mapa_s, &mensajeAceptaConexion);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return mapa_s;
	}

	switch(mensajeAceptaConexion.tipoMensaje) {
	case RECHAZA_CONEXION:
		mapa_s->errorCode = RECHAZA_CONEXION;

		log_info(logger, "Socket %d: su conexión ha sido rechazada", mapa_s->descriptor);
		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

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
	mensaje4_t mensajeSolicitaUbicacion;

	mensajeSolicitaUbicacion.tipoMensaje = SOLICITA_UBICACION;
	mensajeSolicitaUbicacion.idPokeNest = idPokeNest;

	crearPaquete((void*) &mensajeSolicitaUbicacion, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	enviarMensaje(mapa_s, paquete);

	free(paquete.paqueteSerializado);

	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	log_info(logger, "Se solicita la ubicación de la PokéNest %c", idPokeNest);

	// Recibir mensaje BRINDA_UBICACION
	mensaje5_t mensajeBrindaUbicacion;

	mensajeBrindaUbicacion.tipoMensaje = BRINDA_UBICACION;
	recibirMensaje(mapa_s, &mensajeBrindaUbicacion);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	if(mensajeBrindaUbicacion.tipoMensaje == BRINDA_UBICACION)
	{
		ubicacionPokeNest->x = mensajeBrindaUbicacion.ubicacionX;
		ubicacionPokeNest->y = mensajeBrindaUbicacion.ubicacionY;

		log_info(logger, "Socket %d: la ubicación de la PokéNest %c es (%d,%d)", mapa_s->descriptor, idPokeNest, ubicacionPokeNest->x, ubicacionPokeNest->y);
	}
	else
		log_info(logger, "Se esperaba un mensaje distinto como respuesta del socket %d", mapa_s->descriptor);
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
	mensaje6_t mensajeSolicitaDesplazamiento;

	mensajeSolicitaDesplazamiento.tipoMensaje = SOLICITA_DESPLAZAMIENTO;
	mensajeSolicitaDesplazamiento.direccion = movimiento;

	crearPaquete((void*) &mensajeSolicitaDesplazamiento, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	enviarMensaje(mapa_s, paquete);

	free(paquete.paqueteSerializado);

	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	log_info(logger, "Se solicita desplazamiento");

	// Recibir mensaje CONFIRMA_DESPLAZAMIENTO
	mensaje7_t mensajeConfirmaDesplazamiento;

	mensajeConfirmaDesplazamiento.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;
	recibirMensaje(mapa_s, &mensajeConfirmaDesplazamiento);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	if(mensajeConfirmaDesplazamiento.tipoMensaje == CONFIRMA_DESPLAZAMIENTO)
	{
		log_info(logger, "Socket %d: ha sido desplazado", mapa_s->descriptor);

		ubicacion->x = mensajeConfirmaDesplazamiento.ubicacionX;
		ubicacion->y = mensajeConfirmaDesplazamiento.ubicacionY;

		log_info(logger, "Socket %d: su ubicación actual es (%d,%d)", mapa_s->descriptor, ubicacion->x, ubicacion->y);
	}
	else
		log_info(logger, "Se esperaba un mensaje distinto como respuesta del socket %d", mapa_s->descriptor);
}

 void solicitarCaptura(socket_t* mapa_s, int* victima) {
	// Enviar mensaje SOLICITA_CAPTURA
	paquete_t paquete;
	mensaje_t mensajeSolicitaCaptura;

	mensajeSolicitaCaptura.tipoMensaje = SOLICITA_CAPTURA;
	crearPaquete((void*) &mensajeSolicitaCaptura, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	enviarMensaje(mapa_s, paquete);

	free(paquete.paqueteSerializado);

	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	log_info(logger, "Se solicita capturar un Pokémon");

	// Recibir mensaje CONFIRMA_CAPTURA
	mensaje9_t mensajeConfirmaCaptura;

	mensajeConfirmaCaptura.tipoMensaje = CONFIRMA_CAPTURA;
	recibirMensaje(mapa_s, &mensajeConfirmaCaptura);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El socket del servidor se encuentra desconectado");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje");

			break;
		}

		log_info(logger, mapa_s->error);
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea");

		return;
	}

	if(mensajeConfirmaCaptura.tipoMensaje == CONFIRMA_CAPTURA)
	{
		log_info(logger, "Socket %d: ha capturado el Pokémon solicitado", mapa_s->descriptor);

		char* rutaPokemon = strdup(puntoMontajeOsada);
		string_append(&rutaPokemon, mensajeConfirmaCaptura.nombreArchivoMetadata);

		char* rutaEntrenador = strdup(rutaDirectorioEntrenador);
		string_append(&rutaEntrenador, "/");
		string_append(&rutaEntrenador, "Dir de Bill/");

		execl( "/bin/cp", "-p", rutaPokemon, rutaEntrenador);
	}
	else if(mensajeConfirmaCaptura.tipoMensaje == INFORMA_MUERTE)
	{
		log_info(logger, "Socket %d: ha resultado víctima en un combate Pokémon", mapa_s->descriptor);

		char* rutaBorrado = strdup("rm -rf /");
		string_append(&rutaBorrado, rutaDirectorioEntrenador);
		string_append(&rutaBorrado, "/");
		string_append(&rutaBorrado, "Dir de Bill/*");

		system(rutaBorrado);
		// Se activa el flag Víctima
		*victima = 1;
	}
	else
		log_info(logger, "Se esperaba un mensaje distinto como respuesta del socket %d", mapa_s->descriptor);
}

void signal_handler() {
 	 struct sigaction sa;

 	 // Print PID
 	 log_info(logger, "PID del proceso entrenador: %d\n", getpid());

 	 // Setup the sighub handler
 	 sa.sa_handler = &signal_termination_handler;

 	 // Restart the system call
 	 sa.sa_flags = SA_RESTART;

 	 // Block every signal received during the handler execution
 	 sigfillset(&sa.sa_mask);

 	 if (sigaction(SIGUSR1, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGUSR1"); // Should not happen

 	 /*
 	 if (sigaction(SIGTERM, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGTERM"); // Should not happen

 	 if (sigaction(SIGINT, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGINT"); // Should not happen
 	 */
}

void signal_termination_handler(int signum) {
 	switch (signum) {
 	 case SIGUSR1:
 	   	configEntrenador.Vidas++;
 	   	log_info(logger, "SIGUSR1: Se ha obtenido una vida");
 	   	log_info(logger, "Vidas restantes: %d", configEntrenador.Vidas);

 	   	break;
/*
 	 case SIGTERM:
  		configEntrenador.Vidas--;
  	    log_info(logger, "SIGTERM: Se ha perdido una vida");
  	    log_info(logger, "Vidas restantes: %d", configEntrenador.Vidas);

  	    break;
 	 case SIGINT:
 		activo = 0;

 		 break;
*/
 	 default:
 	    log_info(logger, "Código inválido: %d", signum);

 	    return;
 	}
}

void obtenerDatosConexion(char* nombreCiudad, char* ip, char* puerto) {
	t_config* metadata;

	char* rutaMetadataMapa = strdup(puntoMontajeOsada);
	string_append(&rutaMetadataMapa, "Mapas/");
	string_append(&rutaMetadataMapa, nombreCiudad);
	string_append(&rutaMetadataMapa, "/");
	string_append(&rutaMetadataMapa, METADATA);

	metadata = config_create(rutaMetadataMapa);
	if(metadata != NULL)
	{
		if(config_has_property(metadata, "TiempoChequeoDeadlock")
				&& config_has_property(metadata, "Batalla")
				&& config_has_property(metadata, "algoritmo")
				&& config_has_property(metadata, "quantum")
				&& config_has_property(metadata, "retardo")
				&& config_has_property(metadata, "IP")
				&& config_has_property(metadata, "Puerto"))
		{
			ip = strdup(config_get_string_value(metadata, "IP"));
			puerto = strdup(config_get_string_value(metadata, "Puerto"));

			config_destroy(metadata);
		}
		else
		{
			ip = NULL;
			puerto = NULL;

			log_error(logger, "El archivo de metadata del mapa tiene un formato inválido");
			config_destroy(metadata);
		}
	}
	else
		ip = NULL;
		puerto = NULL;

		log_error(logger, "La ruta de archivo de metadata indicada no existe");
}
