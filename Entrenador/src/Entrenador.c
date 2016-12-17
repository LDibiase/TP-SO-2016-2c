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
#include <time.h>
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

#define TAMANIO_MAXIMO_MENSAJE 150	// Tamaño máximo de un mensaje

// VARIABLES GLOBALES
t_log* logger;							// Archivo de log
t_entrenador_config configEntrenador;	// Datos de configuración
int activo;							 	// Flag de actividad del entrenador
int interbloqueo;						// Flag de interbloqueo del entrenador
char* puntoMontajeOsada;         	  	// Punto de montaje del FS
char* rutaDirectorioEntrenador;         // Ruta del directorio del entrenador
t_list* pokemonesAtrapados;				// Pokémones atrapados

char* ip;								// IP del mapa
char* puerto;							// Puerto del mapa
socket_t* mapa_s;						// Socket del mapa
char* nombreCiudad;						// Nombre del mapa

//SEMÁFORO PARA SINCRONIZAR EL PROCESO DE DESBLOQUEO DE ENTRENADORES
pthread_mutex_t mutexRecursos;

int main(int argc, char** argv) {
	t_ubicacion ubicacion;
	char ejeAnterior;
	int objetivosCompletados;
	int victima;

	// Variables para la creación del hilo para el manejo de señales
	pthread_t hiloSignalHandler;
	pthread_attr_t atributosHiloSignalHandler;

	void _obtenerObjetivo(char* objetivo) {
		if(activo && !victima)
		{
			// Inicializar ubicación de la PokéNest a la que se desea llegar
			t_ubicacion ubicacionPokeNest;

			ubicacionPokeNest.x = 0;
			ubicacionPokeNest.y = 0;

			// Mientras que no se haya alcanzado la ubicación de la PokéNest a la que se desea llegar
			while((ubicacion.x != ubicacionPokeNest.x || ubicacion.y != ubicacionPokeNest.y)) {
				if(ubicacionPokeNest.x == 0 && ubicacionPokeNest.y == 0)
				{
					// Determinar ubicación de la PokéNest a la que se desea llegar
					solicitarUbicacionPokeNest(mapa_s, *objetivo, &ubicacionPokeNest);
					if(mapa_s->error != NULL)
					{
						activo = 0;

						break;
					}
				}
				else
				{
					// Desplazar entrenador
					solicitarDesplazamiento(mapa_s, &ubicacion, ubicacionPokeNest, &ejeAnterior);
					if(mapa_s->error != NULL)
					{
						activo = 0;

						break;
					}
				}
			}

			if(activo)
			{
				// Una vez alcanzada la ubicación de la PokéNest, capturar Pokémon
				solicitarCaptura(mapa_s, &victima, objetivo);
				if(!victima && mapa_s->errorCode != NO_ERROR)
					activo = 0;
			}
		}
	}

	void _cumplirObjetivosCiudad(t_ciudad_objetivos* ciudad) {
		if(activo && objetivosCompletados)
		{
			objetivosCompletados = 0;
			victima = 0;
			nombreCiudad = strdup(ciudad->Nombre);

			log_info(logger, "Se recuperan los datos de conexión del mapa %s.", nombreCiudad);

			// Obtener datos de conexión del mapa
			obtenerDatosConexion(nombreCiudad);

			while(configEntrenador.Vidas > 0 && !objetivosCompletados) {
				// Conexión al mapa
				mapa_s = conectarAMapa(ip, puerto);
				if(mapa_s->errorCode != NO_ERROR)
				{
					activo = 0;

					break;
				}

				// Determinar la ubicación inicial del entrenador en el mapa
				ubicacion.x = 1;
				ubicacion.y = 1;

				log_info(logger, "Mi ubicación inicial es (%d,%d).", ubicacion.x, ubicacion.y);

				// Determinar el eje de movimiento anterior arbitrariamente
				ejeAnterior = 'x';

				string_iterate_lines(ciudad->Objetivos, (void*) _obtenerObjetivo);

				if(!activo)
					break;

				if(!victima)
				{
					log_info(logger, "He completado mis objetivos dentro del mapa %s.", nombreCiudad);
					log_info(logger, "Se copia la medalla correspondiente al directorio del entrenador.");

					objetivosCompletados = 1;

					//Se copia la medalla del mapa al directorio del entrenador
					char* sysCall = strdup("cp ");

					string_append(&sysCall, puntoMontajeOsada);
					string_append(&sysCall, "/Mapas/");
					string_append(&sysCall, nombreCiudad);
					string_append(&sysCall, "/");
					string_append(&sysCall, "medalla-");
					string_append(&sysCall, nombreCiudad);
					string_append(&sysCall, ".jpg");

					string_append(&sysCall, " ");

					string_append(&sysCall, rutaDirectorioEntrenador);
					string_append(&sysCall, "medallas/");

					system(sysCall);

					free(sysCall);

					pthread_mutex_lock(&mutexRecursos);
					eliminarSocket(mapa_s);

					mapa_s = NULL;

					free(ip);
					free(puerto);
					free(nombreCiudad);

					ip = NULL;
					puerto = NULL;
					nombreCiudad = NULL;
					pthread_mutex_unlock(&mutexRecursos);
				}
				else
				{
					configEntrenador.Vidas--;
					configEntrenador.Muertes++;
					victima = 0;

					validarVidas();

					break;
				}
			}
		}
	}

	puntoMontajeOsada = strdup(argv[2]);
	rutaDirectorioEntrenador = strdup(argv[2]);
	string_append(&rutaDirectorioEntrenador, "/Entrenadores/");
	string_append(&rutaDirectorioEntrenador, argv[1]);
	string_append(&rutaDirectorioEntrenador, "/");

	//Inicialización de los semáforos
	pthread_mutex_init(&mutexRecursos, NULL);

	// Se almacena la fecha de ingreso
	configEntrenador.FechaIngreso = obtenerFechaActual();

	// Se crea la lista de pokémones atrapados
	pokemonesAtrapados = list_create();

	// Se crea el archivo de log
	char* nombreLog;

	nombreLog = strdup(argv[1]);
	string_append(&nombreLog, LOG_FILE_PATH);

	logger = log_create(nombreLog, "ENTRENADOR", true, LOG_LEVEL_INFO);

	free(nombreLog);

	// Se carga el archivo de metadata
	log_info(logger, "Cargando archivo de metadata...");

	if (cargarConfiguracion(&configEntrenador) == 1)
	{
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea.");

		pthread_mutex_lock(&mutexRecursos);
		liberarRecursos();
		return EXIT_FAILURE;
	}

	log_info(logger, "Nombre del entrenador: %s", configEntrenador.Nombre);
	log_info(logger, "Símbolo del entrenador: %s", configEntrenador.Simbolo);
	log_info(logger, "Vidas restantes: %d", configEntrenador.Vidas);

	// Se setea en 0 (off) el flag de interbloqueo del entrenador
	interbloqueo = 0;

	// Se setea en 1 (on) el flag de actividad del entrenador
	activo = 1;

	// Se crea el hilo para el manejo de señales
	pthread_attr_init(&atributosHiloSignalHandler);
	pthread_create(&hiloSignalHandler, &atributosHiloSignalHandler, (void*) signal_handler, NULL);
	pthread_attr_destroy(&atributosHiloSignalHandler);

	// Se inicializa el flag de objetivos completados
	objetivosCompletados = 0;

	while(activo && !objetivosCompletados) {
		// Se activa el flag únicamente para entrar a la iteración
		objetivosCompletados = 1;

		configEntrenador.Intentos++;

		// Se cumple con los objetivos de cada ciudad incluida en la Hoja de Viaje
		list_iterate(configEntrenador.CiudadesYObjetivos, (void*) _cumplirObjetivosCiudad);
	}

	if(!activo)
	{
		log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea.");

		pthread_mutex_lock(&mutexRecursos);
		liberarRecursos();
		return EXIT_FAILURE;
	}

	log_info(logger, "Has cumplido con todos los objetivos especificados en tu Hoja de Viaje.");
	log_info(logger, "Ahora eres un Maestro Pokémon.");

	// Se informan los tiempos de la aventura
	double tiempoTotal;
	tiempoTotal = obtenerDiferenciaTiempo(configEntrenador.FechaIngreso);

	log_info(logger, "Tiempo total que le ha tomado al entrenador completar la aventura: %f segundos", tiempoTotal);
	log_info(logger, "Tiempo total que el entrenador ha estado bloqueado: %f segundos", configEntrenador.TiempoBloqueado);
	log_info(logger, "Cantidad de intentos realizados: %d", configEntrenador.Intentos);
	log_info(logger, "Cantidad de muertes: %d", configEntrenador.Muertes);

	pthread_mutex_lock(&mutexRecursos);
	liberarRecursos();
	return EXIT_SUCCESS;
}

int cargarConfiguracion(t_entrenador_config* structConfig) {
	t_config* config;
	char* rutaMetadataEntrenador;

	void _auxIterate(char* ciudad)
	{
		char* stringObjetivo = string_from_format("obj[%s]", ciudad);

		t_ciudad_objetivos* ciudadObjetivos;
		ciudadObjetivos = malloc(sizeof(t_ciudad_objetivos));

		char** arrayObjetivos = config_get_array_value(config, stringObjetivo);

		ciudadObjetivos->Nombre = strdup(ciudad);
		ciudadObjetivos->Objetivos = arrayObjetivos;

		list_add(structConfig->CiudadesYObjetivos, ciudadObjetivos);

		free(stringObjetivo);
	}

	rutaMetadataEntrenador = strdup(rutaDirectorioEntrenador);
	string_append(&rutaMetadataEntrenador, METADATA);

	config = config_create(rutaMetadataEntrenador);
	free(rutaMetadataEntrenador);
	if(config != NULL)
	{
		if(config_has_property(config, "nombre")
				&& config_has_property(config, "simbolo")
				&& config_has_property(config, "hojaDeViaje")
				&& config_has_property(config, "vidas"))
		{
			structConfig->CiudadesYObjetivos = list_create();
			char** hojaDeViaje = config_get_array_value(config, "hojaDeViaje");

			structConfig->Nombre = strdup(config_get_string_value(config, "nombre"));
			structConfig->Simbolo = strdup(config_get_string_value(config, "simbolo"));
			structConfig->Vidas = config_get_int_value(config, "vidas");

			//Se buscan los objetivos de cada ciudad
			string_iterate_lines(hojaDeViaje, (void*) _auxIterate);

			log_info(logger, "El archivo de metadata se cargó correctamente.");

			int j=0;
			while(hojaDeViaje[j])
			{
				free(hojaDeViaje[j]);
				j++;
			}

			free(hojaDeViaje);
			config_destroy(config);

			return 0;
		}
		else
		{
			log_error(logger, "El archivo de metadata tiene un formato inválido.");

			config_destroy(config);

			return 1;
		}
	}
	else
	{
		log_error(logger, "La ruta de archivo de metadata indicada no existe.");

		return 1;
	}
}

socket_t* conectarAMapa(char* ip, char* puerto) {
	mapa_s = conectarAServidor(ip, puerto);
	if(mapa_s->errorCode != 0)
	{
		switch(mapa_s->errorCode) {
		case ERR_SERVER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_CLIENT_CANNOT_CONNECT:
			log_info(logger, "No ha sido posible establecer conexión con el mapa.");

			break;
		}

		log_info(logger, mapa_s->error);

		return mapa_s;
	}

	log_info(logger, "El entrenador se ha conectado de manera exitosa al mapa (socket %d).", mapa_s->descriptor);

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
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse.");

		return mapa_s;
	}

	enviarMensaje(mapa_s, paquete);
	free(paquete.paqueteSerializado);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

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
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return mapa_s;
	}

	switch(mensajeAceptaConexion.tipoMensaje) {
	case RECHAZA_CONEXION:
		mapa_s->errorCode = RECHAZA_CONEXION;

		log_info(logger, "Mapa %s (socket %d): tu conexión ha sido rechazada.", nombreCiudad, mapa_s->descriptor);
		log_info(logger, mapa_s->error);

		break;
	case ACEPTA_CONEXION:
		log_info(logger, "Mapa %s (socket %d): tu conexión ha sido aceptada.", nombreCiudad, mapa_s->descriptor);

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
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse.");

		return;
	}

	enviarMensaje(mapa_s, paquete);
	free(paquete.paqueteSerializado);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	log_info(logger, "Solicito la ubicación de la pokéNest %c.", idPokeNest);

	// Recibir mensaje BRINDA_UBICACION
	mensaje5_t mensajeBrindaUbicacion;

	mensajeBrindaUbicacion.tipoMensaje = BRINDA_UBICACION;
	recibirMensaje(mapa_s, &mensajeBrindaUbicacion);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	if(mensajeBrindaUbicacion.tipoMensaje == BRINDA_UBICACION)
	{
		ubicacionPokeNest->x = mensajeBrindaUbicacion.ubicacionX;
		ubicacionPokeNest->y = mensajeBrindaUbicacion.ubicacionY;

		log_info(logger, "Mapa %s (socket %d): la ubicación de la pokénest %c es (%d,%d).", nombreCiudad, mapa_s->descriptor, idPokeNest, ubicacionPokeNest->x, ubicacionPokeNest->y);
	}
	else
		log_info(logger, "Se esperaba un mensaje distinto como respuesta del mapa (socket %d).", mapa_s->descriptor);
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
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse.");

		return;
	}

	enviarMensaje(mapa_s, paquete);
	free(paquete.paqueteSerializado);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	switch(movimiento) {
	case IZQUIERDA:
		log_info(logger, "Solicito desplazamiento hacia la izquierda.");

		break;
	case DERECHA:
		log_info(logger, "Solicito desplazamiento hacia la derecha.");

		break;
	case ARRIBA:
		log_info(logger, "Solicito desplazamiento hacia arriba.");

		break;
	case ABAJO:
		log_info(logger, "Solicito desplazamiento hacia abajo.");

		break;
	}

	// Recibir mensaje CONFIRMA_DESPLAZAMIENTO
	mensaje7_t mensajeConfirmaDesplazamiento;

	mensajeConfirmaDesplazamiento.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;
	recibirMensaje(mapa_s, &mensajeConfirmaDesplazamiento);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_RECEIVED:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	if(mensajeConfirmaDesplazamiento.tipoMensaje == CONFIRMA_DESPLAZAMIENTO)
	{
		log_info(logger, "Mapa %s (socket %d): has sido desplazado.", nombreCiudad, mapa_s->descriptor);

		ubicacion->x = mensajeConfirmaDesplazamiento.ubicacionX;
		ubicacion->y = mensajeConfirmaDesplazamiento.ubicacionY;

		log_info(logger, "Mapa %s (socket %d): tu ubicación actual es (%d,%d).", nombreCiudad, mapa_s->descriptor, ubicacion->x, ubicacion->y);
	}
	else
		log_info(logger, "Se esperaba un mensaje distinto como respuesta del mapa (socket %d).", mapa_s->descriptor);
}

void solicitarCaptura(socket_t* mapa_s, int* victima, char* objetivo) {
	// Enviar mensaje SOLICITA_CAPTURA
	paquete_t paquete;
	mensaje_t mensajeSolicitaCaptura;

	mensajeSolicitaCaptura.tipoMensaje = SOLICITA_CAPTURA;
	crearPaquete((void*) &mensajeSolicitaCaptura, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse.");

		return;
	}

	configEntrenador.FechaUltimoBloqueo = obtenerFechaActual();

	enviarMensaje(mapa_s, paquete);
	free(paquete.paqueteSerializado);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	log_info(logger, "Solicito la captura de un pokémon %s.", objetivo);

	// Recibir mensaje CONFIRMA_CAPTURA
	void* mensajeConfirmaCaptura;

	while(activo) {
		mensajeConfirmaCaptura = malloc(TAMANIO_MAXIMO_MENSAJE);
		((mensaje_t*) mensajeConfirmaCaptura)->tipoMensaje = INDEFINIDO;
		recibirMensaje(mapa_s, mensajeConfirmaCaptura);
		if(mapa_s->errorCode != NO_ERROR)
		{
			free(mensajeConfirmaCaptura);

			switch(mapa_s->errorCode) {
			case ERR_PEER_DISCONNECTED:
				log_info(logger, "El mapa se encuentra desconectado.");

				break;
			case ERR_MSG_CANNOT_BE_RECEIVED:
				log_info(logger, "No se ha podido enviar un mensaje.");

				break;
			}

			log_info(logger, mapa_s->error);

			return;
		}

		switch(((mensaje_t*) mensajeConfirmaCaptura)->tipoMensaje) {
		case CONFIRMA_CAPTURA:
			capturaConfirmada((mensaje9_t*) mensajeConfirmaCaptura, objetivo);

			free(mensajeConfirmaCaptura);
			return;
		case INFORMA_INTERBLOQUEO:
			interbloqueo = 1;

			log_info(logger, "Mapa %s (socket %d): te encuentras involucrado en un interbloqueo con otro/s entrenador/es a causa de la solicitud que realizaste.", nombreCiudad, mapa_s->descriptor);

			free(mensajeConfirmaCaptura);
			continue;
		case SOLICITA_ELECCION_POKEMON:
			elegirPokemon();

			free(mensajeConfirmaCaptura);

			if(mapa_s->errorCode != NO_ERROR)
				return;

			continue;
		case INFORMA_VICTORIA:
			log_info(logger, "Mapa %s (socket %d): has resultado ganador en un Combate Pokémon frente a %s.", nombreCiudad, mapa_s->descriptor, ((mensaje13_t*) mensajeConfirmaCaptura)->nombreAdversario);

			free(((mensaje13_t*) mensajeConfirmaCaptura)->nombreAdversario);
			free(mensajeConfirmaCaptura);
			continue;

		case INFORMA_DERROTA:
			log_info(logger, "Mapa %s (socket %d): has resultado víctima en un Combate Pokémon frente a %s.", nombreCiudad, mapa_s->descriptor, ((mensaje14_t*) mensajeConfirmaCaptura)->nombreAdversario);

			free(((mensaje14_t*) mensajeConfirmaCaptura)->nombreAdversario);
			free(mensajeConfirmaCaptura);
			continue;
		case SOLICITA_DESCONEXION:
			solicitudDesconexion(victima);

			free(mensajeConfirmaCaptura);
			return;
		default:
			log_info(logger, "Se esperaba un mensaje distinto como respuesta del mapa (socket %d).", mapa_s->descriptor);

			mapa_s->errorCode = ERR_MSG_CANNOT_BE_RECEIVED;
			free(mensajeConfirmaCaptura);
			return;
		}
	}
}

void signal_handler() {
 	 struct sigaction sa;

 	 // Print PID
 	 log_info(logger, "PID del proceso entrenador: %d", getpid());

 	 // Setup the sighub handler
 	 sa.sa_handler = &signal_termination_handler;

 	 // Restart the system call
 	 sa.sa_flags = SA_RESTART;

 	 // Block every signal received during the handler execution
 	 sigfillset(&sa.sa_mask);

 	 if (sigaction(SIGUSR1, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGUSR1"); // Should not happen

 	 if (sigaction(SIGTERM, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGTERM"); // Should not happen

 	 if (sigaction(SIGINT, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGINT"); // Should not happen
}

void signal_termination_handler(int signum) {
	switch (signum) {
 	case SIGUSR1:
 		configEntrenador.Vidas++;

 		log_info(logger, "SIGUSR1: Se ha obtenido una vida");
 		log_info(logger, "Vidas restantes: %d", configEntrenador.Vidas);

 		break;
 	case SIGTERM:
 		configEntrenador.Vidas--;

 		log_info(logger, "SIGTERM: Se ha perdido una vida");
 		log_info(logger, "Vidas restantes: %d", configEntrenador.Vidas);

 		break;
 	case SIGINT:
		log_info(logger, "El entrenador ha abandonado el juego.");

		pthread_mutex_lock(&mutexRecursos);
		liberarRecursos();
		exit(EXIT_FAILURE);

 		break;
 	default:
 		log_info(logger, "Código inválido: %d", signum);

 		return;
 	}
}

void obtenerDatosConexion(char* nombreCiudad) {
	t_config* metadata;

	char* rutaMetadataMapa = strdup(puntoMontajeOsada);
	string_append(&rutaMetadataMapa, "/Mapas/");
	string_append(&rutaMetadataMapa, nombreCiudad);
	string_append(&rutaMetadataMapa, "/");
	string_append(&rutaMetadataMapa, METADATA);

	metadata = config_create(rutaMetadataMapa);
	free(rutaMetadataMapa);
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
		}
		else
		{
			ip = NULL;
			puerto = NULL;

			log_error(logger, "El archivo de metadata del mapa tiene un formato inválido.");
		}

		config_destroy(metadata);
	}
	else
	{
		ip = NULL;
		puerto = NULL;

		log_error(logger, "La ruta de archivo de metadata indicada no existe.");
	}
}

void eliminarPokemon(t_metadataPokemon* pokemon) {
	free(pokemon->ciudad);
	free(pokemon->rutaArchivo);
	free(pokemon);
}

void eliminarEntrenador(t_entrenador_config* entrenador) {
	void _eliminarCiudadObjetivo(t_ciudad_objetivos* ciudad) {
		free(ciudad->Nombre);
		string_iterate_lines(ciudad->Objetivos, (void*) free);
		free(ciudad->Objetivos);
		free(ciudad);
	}

	if(entrenador != NULL)
	{
		free(entrenador->Nombre);
		free(entrenador->Simbolo);
		list_destroy_and_destroy_elements(entrenador->CiudadesYObjetivos, (void*) _eliminarCiudadObjetivo);
	}
}

void liberarRecursos() {
	free(puntoMontajeOsada);
	free(rutaDirectorioEntrenador);

	if(mapa_s != NULL)
		eliminarSocket(mapa_s);

	if(ip != NULL)
		free(ip);

	if(puerto != NULL)
		free(puerto);

	if(nombreCiudad != NULL)
		free(nombreCiudad);

	list_destroy_and_destroy_elements(pokemonesAtrapados, (void*) eliminarPokemon);
	eliminarEntrenador(&configEntrenador);
	log_destroy(logger);
}

time_t obtenerFechaActual()
{
	time_t current_time;
	current_time = time(NULL);

	if (current_time == ((time_t)-1))
	{
		log_error(logger, "Error al obtener fecha del sistema.");
	}

	return current_time;
}

double obtenerDiferenciaTiempo(time_t tiempoInicial)
{
	double diferencia;
	diferencia = difftime(obtenerFechaActual(), tiempoInicial);
	return diferencia;
}

void validarVidas() {
	if(configEntrenador.Vidas == 0)
	{
		char respuesta;

		log_info(logger, "¿Desea reiniciar el juego? (Y/N)");

		while(activo) {
			respuesta = getchar();

			if(respuesta == 'Y')
			{
				if (cargarConfiguracion(&configEntrenador) == 1)
				{
					log_info(logger, "La ejecución del proceso Entrenador finaliza de manera errónea.");

					pthread_mutex_lock(&mutexRecursos);
					liberarRecursos();
					exit(EXIT_FAILURE);
				}

				break;
			}
			else if (respuesta == 'N')
				activo = 0;
			else
				log_info(logger, "Ingrese una de las siguientes respuestas: Y (Yes) / N (No)");
		}

		if(!activo)
		{
			log_info(logger, "El entrenador ha abandonado el juego.");

			pthread_mutex_lock(&mutexRecursos);
			liberarRecursos();
			exit(EXIT_FAILURE);
		}

		char* sysCall;

		//Se borra el contenido del directorio de medallas
		sysCall = strdup("rm -rf ");

		string_append(&sysCall, rutaDirectorioEntrenador);
		string_append(&sysCall, "medallas");
		string_append(&sysCall, "/*");

		system(sysCall);

		free(sysCall);

		//Se borra el contenido del Dir de Bill
		sysCall = strdup("rm -rf ");

		string_append(&sysCall, rutaDirectorioEntrenador);
		string_append(&sysCall, "\"Dir de Bill\"");
		string_append(&sysCall, "/*");

		system(sysCall);

		free(sysCall);
	}
}

void capturaConfirmada(mensaje9_t* mensajeConfirmacion, char* objetivo) {
	bool _mayorAMenorNivel(t_metadataPokemon* pokemonMayorNivel, t_metadataPokemon* pokemonMenorNivel) {
		return pokemonMayorNivel->nivel > pokemonMenorNivel->nivel;
	}

	//Se actualiza el contador de tiempo en bloqueo
	configEntrenador.TiempoBloqueado = configEntrenador.TiempoBloqueado + obtenerDiferenciaTiempo(configEntrenador.FechaUltimoBloqueo);

	log_info(logger, "Mapa %s (socket %d): has capturado un pokémon %s de nivel %d.", nombreCiudad, mapa_s->descriptor, objetivo, mensajeConfirmacion->nivel);

	//Se actualiza la lista de pokémones atrapados
	t_metadataPokemon* pokemon;
	pokemon = malloc(sizeof(t_metadataPokemon));

	pokemon->id = objetivo[0];
	pokemon->ciudad = strdup(nombreCiudad);
	pokemon->nivel = mensajeConfirmacion->nivel;
	pokemon->rutaArchivo = strdup(mensajeConfirmacion->nombreArchivoMetadata);

	list_add(pokemonesAtrapados, pokemon);
	list_sort(pokemonesAtrapados, (void*) _mayorAMenorNivel);

	//Se copia el archivo de metadata del pokémon atrapado al directorio del entrenador
	char* sysCall = strdup("cp ");

	string_append(&sysCall, puntoMontajeOsada);
	string_append(&sysCall, mensajeConfirmacion->nombreArchivoMetadata);

	string_append(&sysCall, " ");

	string_append(&sysCall, rutaDirectorioEntrenador);
	string_append(&sysCall, "\"Dir de Bill\"");

	system(sysCall);

	free(sysCall);
	free(mensajeConfirmacion->nombreArchivoMetadata);
}

void solicitudDesconexion(int* victima) {
	bool _esPokemonCiudad(t_metadataPokemon* pokemon) {
		return string_equals_ignore_case(pokemon->ciudad, nombreCiudad);
	}

	void _eliminarPokemonCiudad(t_metadataPokemon* pokemonAEliminar) {
		bool _esPokemon(t_metadataPokemon* pokemon) {
			return pokemonAEliminar->id == pokemon->id &&
					string_equals_ignore_case(pokemonAEliminar->rutaArchivo, pokemon->rutaArchivo) &&
					pokemonAEliminar->nivel == pokemon->nivel &&
					string_equals_ignore_case(pokemonAEliminar->ciudad, pokemon->ciudad);
		}

		//Se borra el archivo de metadata del pokémon del directorio del entrenador
		char* archivo;
		archivo = strdup(strrchr(pokemonAEliminar->rutaArchivo, '/'));

		char* sysCall = strdup("rm -rf ");
		string_append(&sysCall, rutaDirectorioEntrenador);
		string_append(&sysCall, "\"Dir de Bill\"");
		string_append(&sysCall, archivo);

		system(sysCall);

		free(archivo);
		free(sysCall);

		list_remove_by_condition(pokemonesAtrapados, (void*) _esPokemon);
	}

	//Se actualiza el contador de tiempo en bloqueo
	configEntrenador.TiempoBloqueado = configEntrenador.TiempoBloqueado + obtenerDiferenciaTiempo(configEntrenador.FechaUltimoBloqueo);

	log_info(logger, "Mapa %s (socket %d): has resultado víctima en el o los Combates Pokémon.", nombreCiudad, mapa_s->descriptor);

	//Se eliminan los pokémones atrapados dentro de la ciudad actual
	t_list* pokemonesCiudad;

	pokemonesCiudad = list_filter(pokemonesAtrapados, (void*) _esPokemonCiudad);
	list_iterate(pokemonesCiudad, (void*) _eliminarPokemonCiudad);
	list_destroy_and_destroy_elements(pokemonesCiudad, (void*) eliminarPokemon);

	// Enviar mensaje DESCONEXION_ENTRENADOR
	paquete_t paquete;
	mensaje_t mensajeDesconexionEntrenador;

	mensajeDesconexionEntrenador.tipoMensaje = DESCONEXION_ENTRENADOR;
	crearPaquete((void*) &mensajeDesconexionEntrenador, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse.");

		return;
	}

	enviarMensaje(mapa_s, paquete);
	free(paquete.paqueteSerializado);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	pthread_mutex_lock(&mutexRecursos);
	eliminarSocket(mapa_s);

	mapa_s = NULL;
	pthread_mutex_unlock(&mutexRecursos);

	*victima = 1;

	log_info(logger, "Me he desconectado del mapa %s.", nombreCiudad);

	pthread_mutex_lock(&mutexRecursos);
	free(ip);
	free(puerto);
	free(nombreCiudad);

	ip = NULL;
	puerto = NULL;
	nombreCiudad = NULL;
	pthread_mutex_unlock(&mutexRecursos);
}

void elegirPokemon() {
	t_metadataPokemon* pokemon;
	char* nombrePokemon;

	log_info(logger, "Mapa %s (socket %d): debes elegir un pokémon con el cual participar del o los Combates Pokémon.", nombreCiudad, mapa_s->descriptor);

	pokemon = list_get(pokemonesAtrapados, 0);
	nombrePokemon = obtenerNombrePokemon(pokemon);

	// Enviar mensaje INFORMA_POKEMON_ELEGIDO
	paquete_t paquete;
	mensaje12_t mensajeInformaPokemonElegido;

	mensajeInformaPokemonElegido.tipoMensaje = INFORMA_POKEMON_ELEGIDO;
	mensajeInformaPokemonElegido.tamanioNombrePokemon = strlen(nombrePokemon) + 1;
	mensajeInformaPokemonElegido.nombrePokemon = nombrePokemon;
	mensajeInformaPokemonElegido.nivel = pokemon->nivel;
	crearPaquete((void*) &mensajeInformaPokemonElegido, &paquete);
	if(paquete.tamanioPaquete == 0)
	{
		mapa_s->errorCode = ERR_MSG_CANNOT_BE_SENT;
		log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse.");

		return;
	}

	enviarMensaje(mapa_s, paquete);
	free(paquete.paqueteSerializado);
	if(mapa_s->errorCode != NO_ERROR)
	{
		switch(mapa_s->errorCode) {
		case ERR_PEER_DISCONNECTED:
			log_info(logger, "El mapa se encuentra desconectado.");

			break;
		case ERR_MSG_CANNOT_BE_SENT:
			log_info(logger, "No se ha podido enviar un mensaje.");

			break;
		}

		log_info(logger, mapa_s->error);

		return;
	}

	log_info(logger, "He elegido a mi %s de nivel %d para participar del o los Combates Pokémon.", nombrePokemon, pokemon->nivel);

	free(nombrePokemon);
}

char* obtenerNombrePokemon(t_metadataPokemon* pokemon)
{
	char* nombreArchivoMetadataInvertido;
	char* nombrePokemon;

	nombreArchivoMetadataInvertido = string_reverse(strrchr(pokemon->rutaArchivo, '/'));
	nombrePokemon = string_substring_from(nombreArchivoMetadataInvertido, 7);

	free(nombreArchivoMetadataInvertido);
	nombreArchivoMetadataInvertido = string_reverse(nombrePokemon);

	free(nombrePokemon);
	nombrePokemon = string_substring_from(nombreArchivoMetadataInvertido, 1);

	free(nombreArchivoMetadataInvertido);

	return nombrePokemon;
}
