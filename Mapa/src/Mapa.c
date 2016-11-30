/*
 ============================================================================
 Name        : Mapa.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Mapa
 ============================================================================
 */

#include <stdlib.h>
#include <tad_items.h>
#include <curses.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/string.h>
#include <commons/log.h>
#include "Mapa.h"
#include "nivel.h"
#include <pkmn/battle.h>
#include <pkmn/factory.h>

#define BACKLOG 10 					// Cuántas conexiones pendientes se mantienen en cola
#define TAMANIO_MAXIMO_MENSAJE 50	// Tamaño máximo de un mensaje

/* Variables globales */
socket_t* mi_socket_s;

t_log* logger;			  		// Archivo de log
t_mapa_config configMapa; 		// Datos de configuración
t_list* entrenadores;	  		// Entrenadores conectados al mapa
t_list* items; 			  		// Ítemes existentes en el mapa (entrenadores y PokéNests)
t_list* recursosTotales;  		// Recursos existentes en el mapa (Pokémones)
t_list* recursosDisponibles;	// Recursos disponibles en el mapa (Pokémones)
t_list* recursosAsignados;		// Recursos asignados (Pokémones)
t_list* recursosSolicitados;	// Recursos solicitados (Pokémones)
int activo;						// Flag de actividad del mapa
int configuracionActualizada;   // Flag de actualización de la configuración
char* puntoMontajeOsada;        // Punto de montaje del FS
char* rutaDirectorioMapa;		// Ruta del directorio del mapa
char* nombreMapa;				// Nombre del mapa

//COLAS DE PLANIFICACIÓN
t_queue* colaReady; 			// Cola de entrenadores listos
t_queue* colaBlocked;			// Cola de entrenadores bloqueados

//SEMÁFORO PARA SINCRONIZAR EL ARCHIVO DE LOG
pthread_mutex_t mutexLog;

//SEMÁFOROS PARA SINCRONIZAR LA LISTA Y LAS COLAS
pthread_mutex_t mutexEntrenadores;
pthread_mutex_t mutexReady;
pthread_mutex_t mutexBlocked;

int main(int argc, char** argv) {
	// Variables para la creación del hilo para el manejo de señales
	pthread_t hiloSignalHandler;
	pthread_attr_t atributosHiloSignalHandler;

	//DANDOLE FORMA A LOS PARAMETROS RECIBIDOS
	puntoMontajeOsada = strdup(argv[1]);
	rutaDirectorioMapa = strdup(argv[1]);
	string_append(&rutaDirectorioMapa, "/Mapas/");
	string_append(&rutaDirectorioMapa, argv[2]);
	string_append(&rutaDirectorioMapa, "/");

	nombreMapa = argv[2];

	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHiloEnEscucha;

	// Variables para la creación del hilo verificador de deadlock
	pthread_t hiloDeadlock;
	pthread_attr_t atributosHiloDeadlock;

	// Variables para la diagramación del mapa
	int rows, cols;

	//INICIALIZACIÓN DE LOS SEMÁFOROS
	pthread_mutex_init(&mutexLog, NULL);
	pthread_mutex_init(&mutexEntrenadores, NULL);
	pthread_mutex_init(&mutexReady, NULL);
	pthread_mutex_init(&mutexBlocked, NULL);

	// Creación de las listas de recursos
	recursosTotales = list_create();
	recursosDisponibles = list_create();
	recursosAsignados = list_create();
	recursosSolicitados = list_create();

	// Creación de la lista de entrenadores conectados y las colas de planificación
	entrenadores = list_create();
	colaReady = queue_create();
	colaBlocked = queue_create();

	//CREACIÓN DEL ARCHIVO DE LOG
	logger = log_create(LOG_FILE_PATH, "MAPA", false, LOG_LEVEL_INFO);

	//CONFIGURACIÓN DEL MAPA
	pthread_mutex_lock(&mutexLog);
	log_info(logger, "Cargando archivo de configuración");

	if (cargarConfiguracion(&configMapa) == 1)
	{
		log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
		liberarMemoriaAlocada();
		return EXIT_FAILURE;
	}

	log_info(logger, "Algoritmo de planificación: %s", configMapa.Algoritmo);
	log_info(logger, "Batalla: %d (0: Off / 1: On)", configMapa.Batalla);
	log_info(logger, "IP: %s", configMapa.IP);
	log_info(logger, "Puerto: %s", configMapa.Puerto);
	log_info(logger, "Quantum: %d", configMapa.Quantum);
	log_info(logger, "Retardo: %d", configMapa.Retardo);
	log_info(logger, "Tiempo de chequeo de deadlocks: %d", configMapa.TiempoChequeoDeadlock);
	pthread_mutex_unlock(&mutexLog);

	//INICIALIZACIÓN DEL MAPA
	items = cargarPokenests(); //Carga de las Pokénest del mapa
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&rows, &cols);
	nivel_gui_dibujar(items, nombreMapa);

	// Se setea en 1 (on) el flag de actividad
	activo = 1;

	//CREACIÓN DEL HILO PARA EL MANEJO DE SEÑALES
	pthread_attr_init(&atributosHiloSignalHandler);
	pthread_create(&hiloSignalHandler, &atributosHiloSignalHandler, (void*) signal_handler, NULL);
	pthread_attr_destroy(&atributosHiloSignalHandler);

	//CREACIÓN DEL HILO VERIFICADOR DE DEADLOCK
	pthread_attr_init(&atributosHiloDeadlock);
	pthread_create(&hiloDeadlock, &atributosHiloDeadlock, (void*) chequearDeadlock, NULL);
	pthread_attr_destroy(&atributosHiloDeadlock);

	//CREACIÓN DEL HILO EN ESCUCHA
	pthread_attr_init(&atributosHiloEnEscucha);
	pthread_create(&hiloEnEscucha, &atributosHiloEnEscucha, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHiloEnEscucha);

	while(activo) {

		if(list_size(colaReady->elements) > 0)
		{
			t_entrenador* entrenadorAEjecutar = NULL;

			if(string_equals_ignore_case(configMapa.Algoritmo, "SRDF"))
			{
				bool _noConoceUbicacion(t_entrenador* entrenador) {
					return entrenador->idPokenestActual == 0;
				}

				pthread_mutex_lock(&mutexReady);
				entrenadorAEjecutar = list_remove_by_condition(colaReady->elements, (void*) _noConoceUbicacion);
				pthread_mutex_unlock(&mutexReady);
			}

			if(entrenadorAEjecutar == NULL)
			{
				pthread_mutex_lock(&mutexReady);
				entrenadorAEjecutar = queue_pop(colaReady);
				pthread_mutex_unlock(&mutexReady);
			}

			//MENSAJES A UTILIZAR
			mensaje5_t mensajeBrindaUbicacion;
			mensaje7_t mensajeConfirmaDesplazamiento;

			//SE ATIENDEN LAS SOLICITUDES DEL PRIMER ENTRENADOR EN LA COLA DE LISTOS
			int solicitoCaptura = 0;
			int solicitoUbicacion = 0;
			entrenadorAEjecutar->utEjecutadas = 0;

			if(!activo)
				continue;

			while(entrenadorAEjecutar != NULL && activo && !solicitoCaptura && !solicitoUbicacion &&
                ((string_equals_ignore_case(configMapa.Algoritmo, "RR") && entrenadorAEjecutar->utEjecutadas < configMapa.Quantum) ||
                  string_equals_ignore_case(configMapa.Algoritmo, "SRDF"))) {
				void* mensajeSolicitud = malloc(TAMANIO_MAXIMO_MENSAJE);
				((mensaje_t*) mensajeSolicitud)->tipoMensaje = INDEFINIDO;

				recibirMensaje(entrenadorAEjecutar->socket, mensajeSolicitud);
				if(entrenadorAEjecutar->socket->errorCode != NO_ERROR)
				{
					free(mensajeSolicitud);

					switch(entrenadorAEjecutar->socket->errorCode) {
					case ERR_PEER_DISCONNECTED:
						pthread_mutex_lock(&mutexLog);
						log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
						log_info(logger, entrenadorAEjecutar->socket->error);
						pthread_mutex_unlock(&mutexLog);

						eliminarEntrenadorMapa(entrenadorAEjecutar);
						BorrarItem(items, entrenadorAEjecutar->id);
						eliminarEntrenador(entrenadorAEjecutar);
						entrenadorAEjecutar = NULL;

						informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
						informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);

						desbloquearJugadores();

						continue;
					case ERR_MSG_CANNOT_BE_RECEIVED:
						activo = 0;
						eliminarSocket(mi_socket_s);

						pthread_mutex_lock(&mutexLog);
						log_info(logger, "No se ha podido recibir un mensaje");
						log_info(logger, entrenadorAEjecutar->socket->error);
						log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
						pthread_mutex_unlock(&mutexLog);

						eliminarEntrenador(entrenadorAEjecutar);
						liberarMemoriaAlocada();
						nivel_gui_terminar();

						return EXIT_FAILURE;
					}
				}

				if(!activo)
				{
					eliminarEntrenador(entrenadorAEjecutar);
					free(mensajeSolicitud);
					continue;
				}

				//HAGO UN SWITCH PARA DETERMINAR QUÉ ACCIÓN DESEA REALIZAR EL ENTRENADOR
				t_ubicacion pokeNestSolicitada;
				switch(((mensaje_t*) mensajeSolicitud)->tipoMensaje) {
				case SOLICITA_UBICACION:
					log_info(logger, "Socket %d: solicito ubicación de la PokéNest %c", entrenadorAEjecutar->socket->descriptor, ((mensaje4_t*) mensajeSolicitud)->idPokeNest);

					//BUSCO LA POKÉNEST SOLICITADA
					pokeNestSolicitada = buscarPokenest(items, ((mensaje4_t*) mensajeSolicitud)->idPokeNest);
					entrenadorAEjecutar->idPokenestActual = ((mensaje4_t*) mensajeSolicitud)->idPokeNest;

					//LE ENVÍO AL ENTRENADOR LA UBICACIÓN
					mensajeBrindaUbicacion.tipoMensaje = BRINDA_UBICACION;
					mensajeBrindaUbicacion.ubicacionX = pokeNestSolicitada.x;
					mensajeBrindaUbicacion.ubicacionY = pokeNestSolicitada.y;

					paquete_t paquetePokenest;
					crearPaquete((void*) &mensajeBrindaUbicacion, &paquetePokenest);
					if(paquetePokenest.tamanioPaquete == 0)
					{
						activo = 0;
						eliminarSocket(mi_socket_s);

						free(mensajeSolicitud);
						pthread_mutex_lock(&mutexLog);
						log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
						log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
						pthread_mutex_unlock(&mutexLog);

						eliminarEntrenador(entrenadorAEjecutar);
						liberarMemoriaAlocada();
						nivel_gui_terminar();

						return EXIT_FAILURE;
					}

					enviarMensaje(entrenadorAEjecutar->socket, paquetePokenest);

					free(paquetePokenest.paqueteSerializado);

					if(entrenadorAEjecutar->socket->errorCode != NO_ERROR)
					{
						free(mensajeSolicitud);

						switch(entrenadorAEjecutar->socket->errorCode) {
						case ERR_PEER_DISCONNECTED:
							pthread_mutex_lock(&mutexLog);
							log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
							log_info(logger, entrenadorAEjecutar->socket->error);
							pthread_mutex_unlock(&mutexLog);

							eliminarEntrenadorMapa(entrenadorAEjecutar);
							BorrarItem(items, entrenadorAEjecutar->id);
							eliminarEntrenador(entrenadorAEjecutar);
							entrenadorAEjecutar = NULL;

							informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
							informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);

							desbloquearJugadores();

							continue;
						case ERR_MSG_CANNOT_BE_SENT:
							activo = 0;
							eliminarSocket(mi_socket_s);

							pthread_mutex_lock(&mutexLog);
							log_info(logger, "No se ha podido enviar un mensaje");
							log_info(logger, entrenadorAEjecutar->socket->error);
							log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
							pthread_mutex_unlock(&mutexLog);

							eliminarEntrenador(entrenadorAEjecutar);
							liberarMemoriaAlocada();
							nivel_gui_terminar();

							return EXIT_FAILURE;
						}
					}

					log_info(logger, "Se envía ubicación de la PokéNest %c al entrenador", ((mensaje4_t*) mensajeSolicitud)->idPokeNest);
					free(mensajeSolicitud);

					if(string_equals_ignore_case(configMapa.Algoritmo, "SRDF"))
						solicitoUbicacion = 1;
					else if(string_equals_ignore_case(configMapa.Algoritmo, "RR"))
						entrenadorAEjecutar->utEjecutadas++;

					break;
				case SOLICITA_DESPLAZAMIENTO:
					//MODIFICO LA UBICACIÓN DEL ENTRENADOR DE ACUERDO A LA DIRECCIÓN DE DESPLAZAMIENTO SOLICITADA
					switch(((mensaje6_t*) mensajeSolicitud)->direccion) {
					case ARRIBA:
						log_info(logger, "Socket %d: solicito desplazamiento hacia arriba", entrenadorAEjecutar->socket->descriptor);
						entrenadorAEjecutar->ubicacion.y--;
						break;
					case ABAJO:
						log_info(logger, "Socket %d: solicito desplazamiento hacia abajo", entrenadorAEjecutar->socket->descriptor);
						entrenadorAEjecutar->ubicacion.y++;
						break;
					case IZQUIERDA:
						log_info(logger, "Socket %d: solicito desplazamiento hacia la izquierda", entrenadorAEjecutar->socket->descriptor);
						entrenadorAEjecutar->ubicacion.x--;
						break;
					case DERECHA:
						log_info(logger, "Socket %d: solicito desplazamiento hacia la derecha", entrenadorAEjecutar->socket->descriptor);
						entrenadorAEjecutar->ubicacion.x++;
						break;
					}

					realizar_movimiento(items, *entrenadorAEjecutar, nombreMapa);

					//LE ENVÍO AL ENTRENADOR SU NUEVA UBICACIÓN
					mensajeConfirmaDesplazamiento.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;
					mensajeConfirmaDesplazamiento.ubicacionX = entrenadorAEjecutar->ubicacion.x;
					mensajeConfirmaDesplazamiento.ubicacionY = entrenadorAEjecutar->ubicacion.y;

					paquete_t paqueteDesplazamiento;
					crearPaquete((void*) &mensajeConfirmaDesplazamiento, &paqueteDesplazamiento);
					if(paqueteDesplazamiento.tamanioPaquete == 0)
					{
						activo = 0;
						eliminarSocket(mi_socket_s);

						free(mensajeSolicitud);
						pthread_mutex_lock(&mutexLog);
						log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
						log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
						pthread_mutex_unlock(&mutexLog);

						eliminarEntrenador(entrenadorAEjecutar);
						liberarMemoriaAlocada();
						nivel_gui_terminar();

						return EXIT_FAILURE;
					}

					enviarMensaje(entrenadorAEjecutar->socket, paqueteDesplazamiento);

					free(paqueteDesplazamiento.paqueteSerializado);

					if(entrenadorAEjecutar->socket->errorCode != NO_ERROR)
					{
						free(mensajeSolicitud);

						switch(entrenadorAEjecutar->socket->errorCode) {
						case ERR_PEER_DISCONNECTED:
							pthread_mutex_lock(&mutexLog);
							log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
							log_info(logger, entrenadorAEjecutar->socket->error);
							pthread_mutex_unlock(&mutexLog);

							eliminarEntrenadorMapa(entrenadorAEjecutar);
							BorrarItem(items, entrenadorAEjecutar->id);
							eliminarEntrenador(entrenadorAEjecutar);
							entrenadorAEjecutar = NULL;

							informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
							informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);

							desbloquearJugadores();

							continue;
						case ERR_MSG_CANNOT_BE_SENT:
							activo = 0;
							eliminarSocket(mi_socket_s);

							pthread_mutex_lock(&mutexLog);
							log_info(logger, "No se ha podido enviar un mensaje");
							log_info(logger, entrenadorAEjecutar->socket->error);
							log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
							pthread_mutex_unlock(&mutexLog);

							eliminarEntrenador(entrenadorAEjecutar);
							liberarMemoriaAlocada();
							nivel_gui_terminar();

							return EXIT_FAILURE;
						}
					}

					log_info(logger, "Se le informa al entrenador su nueva posición: (%d,%d)", entrenadorAEjecutar->ubicacion.x, entrenadorAEjecutar->ubicacion.y);
					free(mensajeSolicitud);

					if(string_equals_ignore_case(configMapa.Algoritmo, "SRDF"))
						entrenadorAEjecutar->utEjecutadas++;

					break;
				case SOLICITA_CAPTURA:
					log_info(logger, "Socket %d: solicito capturar Pokémon", entrenadorAEjecutar->socket->descriptor);

					solicitoCaptura = 1;

					actualizarMatriz(recursosSolicitados, entrenadorAEjecutar, 1);
					insertarAlFinal(entrenadorAEjecutar, colaBlocked, &mutexBlocked);
					informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);
					informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);

					free(mensajeSolicitud);

					break;
				}
			}

			if(!activo)
			{
				if(entrenadorAEjecutar != NULL && !solicitoCaptura)
					eliminarEntrenador(entrenadorAEjecutar);
				continue;
			}

			if(configuracionActualizada)
			{
				//CARGO NUEVAMENTE LA CONFIGURACIÓN DEL MAPA
				log_info(logger, "Cargando archivo de configuración");

				if (cargarConfiguracion(&configMapa) == 1)
				{
					if(entrenadorAEjecutar != NULL)
						eliminarEntrenador(entrenadorAEjecutar);

					eliminarSocket(mi_socket_s);
					liberarMemoriaAlocada();
					nivel_gui_terminar();

					return EXIT_FAILURE;
				}

				configuracionActualizada = 0;
			}

			if(!activo)
			{
				if(entrenadorAEjecutar != NULL && !solicitoCaptura)
					eliminarEntrenador(entrenadorAEjecutar);
				continue;
			}

			if(entrenadorAEjecutar != NULL)
			{
				if(solicitoCaptura)
				{
					bool _esEntrenador(t_entrenador* entrenador) {
						return entrenador->id == entrenadorAEjecutar->id;
					}

					pthread_mutex_lock(&mutexBlocked);
					entrenadorAEjecutar = list_remove_by_condition(colaBlocked->elements, (void*) _esEntrenador);
					pthread_mutex_unlock(&mutexBlocked);

					capturarPokemon(entrenadorAEjecutar);

					if(!list_any_satisfy(colaReady->elements, (void*) _esEntrenador))
					{
						pthread_mutex_lock(&mutexBlocked);
						queue_push(colaBlocked, entrenadorAEjecutar);
						pthread_mutex_unlock(&mutexBlocked);
					}
				}
				else
					//VUELVO A ENCOLAR AL ENTRENADOR
					reencolarEntrenador(entrenadorAEjecutar);
			}
		}
	}

	eliminarSocket(mi_socket_s);

	log_info(logger, "La ejecución del proceso Mapa ha finalizado correctamente");

	liberarMemoriaAlocada();
	nivel_gui_terminar();

	return EXIT_SUCCESS;
}

//FUNCIONES PLANIFICADOR
void encolarEntrenador(t_entrenador* entrenador) {
	//SE CREA EL PERSONAJE PARA LA INTERFAZ GRÁFICA
	CrearPersonaje(items, entrenador->id, entrenador->ubicacion.x, entrenador->ubicacion.y);

	log_info(logger, "Se grafica al entrenador %s (%c) en el mapa", entrenador->nombre, entrenador->id);

	//SI EL ALGORITMO ES ROUND ROBIN, LO AGREGO AL FINAL DE LA COLA DE READY
	if(string_equals_ignore_case(configMapa.Algoritmo, "RR"))
		insertarAlFinal(entrenador, colaReady, &mutexReady);
	//SI ES SRDF, INSERTO ORDENADO DE MENOR A MAYOR, DE ACUERDO A CUÁNTO LE FALTE EJECUTAR AL ENTRENADOR
	else
	{
		calcularFaltante(*entrenador);
		insertarOrdenado(entrenador, colaReady, &mutexReady);
	}

	log_info(logger, "Se encola al entrenador");

	informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
	informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);
}

void reencolarEntrenador(t_entrenador* entrenador) {
	//SI EL ALGORITMO ES ROUND ROBIN, LO AGREGO AL FINAL DE LA COLA DE READY
	if(string_equals_ignore_case(configMapa.Algoritmo, "RR"))
		insertarAlFinal(entrenador, colaReady, &mutexReady);
	//SI ES SRDF, INSERTO ORDENADO DE MENOR A MAYOR, DE ACUERDO A CUÁNTO LE FALTE EJECUTAR AL ENTRENADOR
	else
	{
		calcularFaltante(*entrenador);
		insertarOrdenado(entrenador, colaReady, &mutexReady);
	}

	log_info(logger, "Se encola al entrenador");

	informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
	informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);
}

void calcularFaltante(t_entrenador entrenador) {
	t_ubicacion pokenest = buscarPokenest(items, entrenador.idPokenestActual);

	entrenador.faltaEjecutar = abs(pokenest.x - entrenador.ubicacion.x) + abs(pokenest.y - entrenador.ubicacion.y);
}

//FUNCIONES PARA COLAS DE PLANIFICACIÓN
void insertarOrdenado(t_entrenador* entrenador, t_queue* cola, pthread_mutex_t* mutex) {
	bool _auxComparador(t_entrenador* entrenador1, t_entrenador* entrenador2) {
		return entrenador1->faltaEjecutar < entrenador2->faltaEjecutar;
	}

	//SI LA COLA ESTÁ VACÍA, INSERTO AL ENTRENADOR SIN ORDENAR
	if(queue_size(cola) == 0)
	{
		pthread_mutex_lock(mutex);
		queue_push(cola, entrenador);
		pthread_mutex_unlock(mutex);
	}
	else
	{
		pthread_mutex_lock(mutex);
		queue_push(cola, entrenador);
		list_sort(cola->elements, (void*) _auxComparador);
		pthread_mutex_unlock(mutex);
	}
}

void insertarAlFinal(t_entrenador* entrenador, t_queue* cola, pthread_mutex_t* mutex) {
	pthread_mutex_lock(mutex);
	queue_push(cola, entrenador);
	pthread_mutex_unlock(mutex);
}

//FUNCIONES PARA ENTRENADORES
void realizar_movimiento(t_list* items, t_entrenador personaje, char* mapa) {
	MoverPersonaje(items, personaje.id, personaje.ubicacion.x, personaje.ubicacion.y);
	nivel_gui_dibujar(items, mapa);
	usleep(configMapa.Retardo * 1000);
}

ITEM_NIVEL* find_by_id(t_list* items, char idToFind) {
	bool _isTheOne(ITEM_NIVEL* item) {
		return item->id == idToFind;
	}

	return list_find(items, (void*) _isTheOne);
}

t_ubicacion buscarPokenest(t_list* lista, char pokemon) {
	t_ubicacion ubicacion;

	ITEM_NIVEL* pokenest = find_by_id(lista, pokemon);

	if(pokenest != NULL)
	{
		ubicacion.x = pokenest->posx;
		ubicacion.y = pokenest->posy;
	}
	else
	{
		ubicacion.x = 0;
		ubicacion.y = 0;
	}

	return ubicacion;
}

t_list* cargarPokenests() {
	t_mapa_pokenest pokenestLeida;
	t_mapa_pokenest* recursoTotales;
	t_mapa_pokenest* recursoDisponibles;

	t_list* newlist = list_create();
	char* rutaDirectorioPokenests;

	struct dirent* dent;

	rutaDirectorioPokenests = strdup(rutaDirectorioMapa);
	string_append(&rutaDirectorioPokenests, "PokeNests/");

	DIR* srcdir = opendir(rutaDirectorioPokenests);
	if (srcdir == NULL)
		perror("opendir");

	while((dent = readdir(srcdir)) != NULL) {
		struct stat st;

		if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;
		if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0)
		{
			perror(dent->d_name);
			continue;
		}

		if (S_ISDIR(st.st_mode)){
			char* str;
			int cantidadDeRecursos = 0;

	        recursoTotales = malloc(sizeof(pokenestLeida));
	    	recursoDisponibles = malloc(sizeof(pokenestLeida));

			recursoTotales->metadatasPokemones = list_create();
			recursoDisponibles->metadatasPokemones = list_create();

			str = strdup(rutaDirectorioPokenests);
			string_append(&str, dent->d_name);

			//OBTENGO METADATAS DE PÓKEMONES EXISTENTES EN LA POKÉNEST
			cantidadDeRecursos = obtenerCantidadRecursos(dent->d_name, str, recursoTotales->metadatasPokemones);

			string_append(&str, "/metadata");

	        pokenestLeida = leerPokenest(str);

			//CHEQUEO QUE LA POSICIÓN OBTENIDA NO SEA 0. SI LO ES, LE SUMO UNO. (ACLARADO POR LOS AYUDANTES)
			if(pokenestLeida.ubicacion.x == 0)
			{
				pokenestLeida.ubicacion.x++;
			}
			
			if(pokenestLeida.ubicacion.y == 0)
			{
				pokenestLeida.ubicacion.y++;
			}

	        CrearCaja(newlist, pokenestLeida.id, pokenestLeida.ubicacion.x, pokenestLeida.ubicacion.y, cantidadDeRecursos);

	        recursoTotales->cantidad = cantidadDeRecursos;
	        recursoTotales->id = pokenestLeida.id;
	        recursoTotales->tipo = NULL;

	        recursoDisponibles->cantidad = cantidadDeRecursos;
	        recursoDisponibles->id = pokenestLeida.id;
	        recursoDisponibles->tipo = NULL;

	        list_add(recursosTotales, recursoTotales);
	        list_add(recursosDisponibles, recursoDisponibles);

	        log_info(logger, "Se cargó la PokéNest: %c", pokenestLeida.id);
	    	free(str);
	    	free(pokenestLeida.tipo);
		}
	}

	free(rutaDirectorioPokenests);
	closedir(srcdir);
	return newlist;
}

int obtenerCantidadRecursos(char* nombrePokemon, char* rutaPokenest, t_list* metadatasPokemones) {
	int cantidad = 0;
	t_config* config;

	char* nombreArchivoPokemonAux = strdup(rutaPokenest);
	string_append(&nombreArchivoPokemonAux, "/");
	string_append(&nombreArchivoPokemonAux, nombrePokemon);

	char* numeroPokemon = strdup("001");
	char* nombreArchivoPokemon = strdup(nombreArchivoPokemonAux);
	string_append(&nombreArchivoPokemon, numeroPokemon);
	string_append(&nombreArchivoPokemon, ".dat");

	while(1) {
		config = config_create(nombreArchivoPokemon);

		if(config != NULL)
		{
			if(config_has_property(config, "Nivel"))
			{
				bool _mayorAMenorNivel(t_metadataPokemon* pokemonMayorNivel, t_metadataPokemon* pokemonMenorNivel) {
					return pokemonMayorNivel->nivel > pokemonMenorNivel->nivel;
				}

				t_metadataPokemon* metadataPokemon;
				metadataPokemon = malloc(sizeof(t_metadataPokemon));

				metadataPokemon->nivel = config_get_int_value(config, "Nivel");
				metadataPokemon->rutaArchivo = string_substring_from(nombreArchivoPokemon, strlen(puntoMontajeOsada));

				list_add(metadatasPokemones, metadataPokemon);
				list_sort(metadatasPokemones, (void*) _mayorAMenorNivel);

				cantidad++;

				free(numeroPokemon);
				free(nombreArchivoPokemon);

				char* cantidadToString = string_itoa(cantidad);

				if(cantidad <= 9)
					numeroPokemon = strdup("00");

				if(cantidad > 9)
					numeroPokemon = strdup("0");

				if(cantidad > 99)
					numeroPokemon = string_new();

				string_append(&numeroPokemon, cantidadToString);
				nombreArchivoPokemon = strdup(nombreArchivoPokemonAux);
				string_append(&nombreArchivoPokemon, numeroPokemon);
				string_append(&nombreArchivoPokemon, ".dat");

				config_destroy(config);
				free(cantidadToString);
			}
			else
			{
				config_destroy(config);
				break;
			}
		}
		else
		{
			cantidad--;
			break;
		}
	}

	list_remove(metadatasPokemones, 0); // TODO Arreglar

	free(numeroPokemon);
	free(nombreArchivoPokemon);
	free(nombreArchivoPokemonAux);

	return cantidad;
}

t_mapa_pokenest leerPokenest(char* metadata) {
	t_config* config;
	t_mapa_pokenest structPokenest;

	config = config_create(metadata);

	if(config != NULL)
	{
		if(config_has_property(config, "Tipo")
				&& config_has_property(config, "Posicion")
				&& config_has_property(config, "Identificador"))
		{
			structPokenest.id = *(config_get_string_value(config, "Identificador"));
			structPokenest.tipo = strdup(config_get_string_value(config, "Tipo"));
			char* posXY = strdup(config_get_string_value(config, "Posicion"));
			char** array = string_split(posXY, ";");
			structPokenest.ubicacion.x = atoi(array[0]);
			structPokenest.ubicacion.y = atoi(array[1]);
			config_destroy(config);
		}
		else
		{
			log_error(logger, "El archivo de metadata de la PokéNest tiene un formato inválido");
			config_destroy(config);
		}
	}
	else
		log_error(logger, "La ruta de archivo de metadata indicada no existe");

	return structPokenest;
}

int cargarConfiguracion(t_mapa_config* structConfig) {
	t_config* config;
	char* rutaMetadataMapa;

	rutaMetadataMapa = strdup(rutaDirectorioMapa);
	string_append(&rutaMetadataMapa, METADATA);

	config = config_create(rutaMetadataMapa);

	if(config != NULL)
	{
		free(rutaMetadataMapa);

		if(config_has_property(config, "TiempoChequeoDeadlock")
				&& config_has_property(config, "Batalla")
				&& config_has_property(config, "algoritmo")
				&& config_has_property(config, "quantum")
				&& config_has_property(config, "retardo")
				&& config_has_property(config, "IP")
				&& config_has_property(config, "Puerto"))
		{
			structConfig->TiempoChequeoDeadlock = config_get_int_value(config, "TiempoChequeoDeadlock");
			structConfig->Batalla = config_get_int_value(config, "Batalla");
			structConfig->Algoritmo = strdup(config_get_string_value(config, "algoritmo"));
			structConfig->Quantum = config_get_int_value(config, "quantum");
			structConfig->Retardo = config_get_int_value(config, "retardo");
			structConfig->IP = strdup(config_get_string_value(config, "IP"));
			structConfig->Puerto = strdup(config_get_string_value(config, "Puerto"));

			log_info(logger, "El archivo de configuración se cargó correctamente");
			config_destroy(config);
			return 0;
		}
		else
		{
			log_error(logger, "El archivo de metadata del pokémon tiene un formato inválido");
			config_destroy(config);
			return 1;
		}
	}
	else
	{
		log_error(logger, "La ruta de archivo de configuración indicada no existe");
		free(rutaMetadataMapa);
		return 1;
	}
}

void aceptarConexiones() {
	int returnValue;

	mi_socket_s = crearServidor(configMapa.IP, configMapa.Puerto);
	if(mi_socket_s->errorCode == ERR_SERVER_CREATION)
	{
		activo = 0;

		pthread_mutex_lock(&mutexLog);
		log_info(logger, "La creación del servidor ha fallado");
		log_info(logger, mi_socket_s->error);
		log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
		pthread_mutex_unlock(&mutexLog);

		liberarMemoriaAlocada();
		nivel_gui_terminar();

		abort();
	}

	returnValue = escucharConexiones(*mi_socket_s, BACKLOG);
	if(returnValue != 0)
	{
		activo = 0;
		eliminarSocket(mi_socket_s);

		pthread_mutex_lock(&mutexLog);
		log_info(logger, "No se ha podido iniciar la escucha de conexiones");
		log_info(logger, strerror(returnValue));
		log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
		pthread_mutex_unlock(&mutexLog);

		liberarMemoriaAlocada();
		nivel_gui_terminar();

		abort();
	}

	while(activo) {
		socket_t* cli_socket_s;
		t_entrenador* entrenador;

		entrenador = malloc(sizeof(t_entrenador));

		log_info(logger, "Escuchando conexiones");

		cli_socket_s = aceptarConexion(*mi_socket_s);
		if(cli_socket_s->errorCode != NO_ERROR)
		{
			switch(cli_socket_s->errorCode) {
			case ERR_CLIENT_CANNOT_CONNECT:
				pthread_mutex_lock(&mutexLog);
				log_info(logger, "Un cliente ha intentado establecer conexión con el servidor sin éxito");
				log_info(logger, cli_socket_s->error);
				pthread_mutex_unlock(&mutexLog);

				free(entrenador);
				free(cli_socket_s->error);

				continue;
			case ERR_SERVER_DISCONNECTED:
				activo = 0;
				eliminarSocket(mi_socket_s);

				pthread_mutex_lock(&mutexLog);
				log_info(logger, "El socket del servidor se encuentra desconectado");
				log_info(logger, cli_socket_s->error);
				log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
				pthread_mutex_unlock(&mutexLog);

				free(entrenador);
				free(cli_socket_s->error);

				liberarMemoriaAlocada();
				nivel_gui_terminar();

				abort();
			}
		}

		///////////////////////
		////// HANDSHAKE //////
		///////////////////////

		// Recibir mensaje CONEXION_ENTRENADOR
		mensaje1_t mensajeConexionEntrenador;

		mensajeConexionEntrenador.tipoMensaje = CONEXION_ENTRENADOR;
		recibirMensaje(cli_socket_s, &mensajeConexionEntrenador);
		if(cli_socket_s->errorCode != NO_ERROR)
		{
			switch(cli_socket_s->errorCode) {
			case ERR_PEER_DISCONNECTED:
				pthread_mutex_lock(&mutexLog);
				log_info(logger, "El socket %d se encuentra desconectado", cli_socket_s->descriptor);
				log_info(logger, cli_socket_s->error);
				pthread_mutex_unlock(&mutexLog);

				free(entrenador);
				free(cli_socket_s->error);

				continue;
			case ERR_MSG_CANNOT_BE_RECEIVED:
				activo = 0;
				eliminarSocket(mi_socket_s);

				pthread_mutex_lock(&mutexLog);
				log_info(logger, "No se ha podido recibir un mensaje");
				log_info(logger, cli_socket_s->error);
				log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
				pthread_mutex_unlock(&mutexLog);

				free(entrenador);
				free(cli_socket_s->error);

				liberarMemoriaAlocada();
				nivel_gui_terminar();

				abort();
			}
		}

		if(mensajeConexionEntrenador.tipoMensaje != CONEXION_ENTRENADOR)
		{
			free(entrenador);

			// Enviar mensaje RECHAZA_CONEXION
			paquete_t paquete;
			mensaje_t mensajeRechazaConexion;

			mensajeRechazaConexion.tipoMensaje = RECHAZA_CONEXION;

			crearPaquete((void*) &mensajeRechazaConexion, &paquete);
			if(paquete.tamanioPaquete == 0)
			{
				activo = 0;
				eliminarSocket(mi_socket_s);

				pthread_mutex_lock(&mutexLog);
				log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
				pthread_mutex_unlock(&mutexLog);

				free(cli_socket_s->error);

				liberarMemoriaAlocada();
				nivel_gui_terminar();

				abort();
			}

			enviarMensaje(cli_socket_s, paquete);

			free(paquete.paqueteSerializado);

			if(cli_socket_s->errorCode != NO_ERROR)
			{
				switch(cli_socket_s->errorCode) {
				case ERR_PEER_DISCONNECTED:
					pthread_mutex_lock(&mutexLog);
					log_info(logger, "El socket %d se encuentra desconectado", cli_socket_s->descriptor);
					log_info(logger, cli_socket_s->error);
					pthread_mutex_unlock(&mutexLog);

					free(cli_socket_s->error);

					continue;
				case ERR_MSG_CANNOT_BE_SENT:
					activo = 0;
					eliminarSocket(mi_socket_s);

					pthread_mutex_lock(&mutexLog);
					log_info(logger, "No se ha podido enviar un mensaje");
					log_info(logger, cli_socket_s->error);
					log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
					pthread_mutex_unlock(&mutexLog);

					free(cli_socket_s->error);

					liberarMemoriaAlocada();
					nivel_gui_terminar();

					abort();
				}
			}

			log_info(logger, "Conexión mediante socket %d rechazada", cli_socket_s->descriptor);

			continue;
		}

		log_info(logger, "Socket %d: mi nombre es %s (%c) y soy un entrenador Pokémon", cli_socket_s->descriptor, mensajeConexionEntrenador.nombreEntrenador, mensajeConexionEntrenador.simboloEntrenador);

		entrenador->faltaEjecutar = 0;
		entrenador->id = mensajeConexionEntrenador.simboloEntrenador;
		entrenador->idPokenestActual = 0;
		entrenador->nombre = mensajeConexionEntrenador.nombreEntrenador;
		entrenador->socket = cli_socket_s;
		entrenador->ubicacion.x = 1;
		entrenador->ubicacion.y = 1;
		entrenador->utEjecutadas = 0;
		entrenador->fechaIngreso = obtenerFechaIngreso();

		// Enviar mensaje ACEPTA_CONEXION
		paquete_t paquete;
		mensaje_t mensajeAceptaConexion;

		mensajeAceptaConexion.tipoMensaje = ACEPTA_CONEXION;

		crearPaquete((void*) &mensajeAceptaConexion, &paquete);
		if(paquete.tamanioPaquete == 0)
		{
			activo = 0;
			eliminarSocket(mi_socket_s);

			pthread_mutex_lock(&mutexLog);
			log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
			pthread_mutex_unlock(&mutexLog);

			free(entrenador);
			free(cli_socket_s->error);

			liberarMemoriaAlocada();
			nivel_gui_terminar();

			abort();
		}

		enviarMensaje(entrenador->socket, paquete);

		free(paquete.paqueteSerializado);

		if(entrenador->socket->errorCode != NO_ERROR)
		{
			switch(cli_socket_s->errorCode) {
			case ERR_PEER_DISCONNECTED:
				pthread_mutex_lock(&mutexLog);
				log_info(logger, "El socket %d se encuentra desconectado", cli_socket_s->descriptor);
				log_info(logger, cli_socket_s->error);
				pthread_mutex_unlock(&mutexLog);

				free(entrenador);
				free(cli_socket_s->error);

				continue;
			case ERR_MSG_CANNOT_BE_SENT:
				activo = 0;
				eliminarSocket(mi_socket_s);

				pthread_mutex_lock(&mutexLog);
				log_info(logger, "No se ha podido enviar un mensaje");
				log_info(logger, cli_socket_s->error);
				log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
				pthread_mutex_unlock(&mutexLog);

				free(entrenador);
				free(cli_socket_s->error);

				liberarMemoriaAlocada();
				nivel_gui_terminar();

				abort();
			}
		}

		log_info(logger, "Se aceptó una conexión (socket %d)", entrenador->socket->descriptor);

		t_entrenador* entrenadorPlanificado;
		entrenadorPlanificado = malloc(sizeof(t_entrenador));

		entrenadorPlanificado->faltaEjecutar = entrenador->faltaEjecutar;
		entrenadorPlanificado->id = entrenador->id;
		entrenadorPlanificado->idPokenestActual = entrenador->idPokenestActual;
		entrenadorPlanificado->nombre = strdup(entrenador->nombre);
		entrenadorPlanificado->socket = malloc(sizeof(socket_t));
		*(entrenadorPlanificado->socket) = *(entrenador->socket);
		entrenadorPlanificado->ubicacion.x = entrenador->ubicacion.x;
		entrenadorPlanificado->ubicacion.y = entrenador->ubicacion.y;
		entrenadorPlanificado->utEjecutadas = entrenador->utEjecutadas;

		//Se agrega al entrenador a la lista de entrenadores conectados
		pthread_mutex_lock(&mutexEntrenadores);
		list_add(entrenadores, entrenador);
		pthread_mutex_unlock(&mutexEntrenadores);

		informarEstadoCola("Entrenadores conectados", entrenadores, &mutexEntrenadores);

		//SE PLANIFICA AL NUEVO ENTRENADOR
		encolarEntrenador(entrenadorPlanificado);

		//SE ADAPTAN Y ACTUALIZAN LAS MATRICES DE RECURSOS
		t_recursosEntrenador* recursosAsignadosEntrenador;
		t_recursosEntrenador* recursosSolicitadosEntrenador;

		recursosAsignadosEntrenador = malloc(sizeof(t_recursosEntrenador));
		recursosSolicitadosEntrenador = malloc(sizeof(t_recursosEntrenador));

		recursosAsignadosEntrenador->id = entrenadorPlanificado->id;
		recursosSolicitadosEntrenador->id = entrenadorPlanificado->id;

		recursosAsignadosEntrenador->recursos = list_create();
		recursosSolicitadosEntrenador->recursos = list_create();

		void _agregarRecursosEntrenador(t_mapa_pokenest* recurso)
		{
			t_mapa_pokenest* recursoAsignado;
			t_mapa_pokenest* recursoSolicitado;

			recursoAsignado = malloc(sizeof(t_mapa_pokenest));
			recursoSolicitado = malloc(sizeof(t_mapa_pokenest));

			recursoAsignado->tipo = NULL;
			recursoAsignado->id = recurso->id;
			recursoAsignado->cantidad = 0;
			recursoAsignado->metadatasPokemones = list_create();

			recursoSolicitado->tipo = NULL;
			recursoSolicitado->id = recurso->id;
			recursoSolicitado->cantidad = 0;
			recursoSolicitado->metadatasPokemones = list_create();

			list_add(recursosAsignadosEntrenador->recursos, recursoAsignado);
			list_add(recursosSolicitadosEntrenador->recursos, recursoSolicitado);
		}

		list_iterate(recursosTotales, (void*) _agregarRecursosEntrenador);

		list_add(recursosAsignados, (void*) recursosAsignadosEntrenador);
		list_add(recursosSolicitados, (void*) recursosSolicitadosEntrenador);

		log_info(logger, "Se planificó al entrenador %s (%c)", entrenadorPlanificado->nombre, entrenadorPlanificado->id);
	}
}

void eliminarEntrenador(t_entrenador* entrenador) {
	free(entrenador->nombre);
	eliminarSocket(entrenador->socket);
	free(entrenador);
}

bool algoritmoDeteccion() {
	//LISTA AUXILIAR DE ENTRENADORES
	t_list* entrenadoresAux;
	entrenadoresAux = list_create();
	list_add_all(entrenadoresAux, colaBlocked->elements);

	//LISTA AUXILIAR SOLICITUDES
	t_list* solicitudesAux;
	solicitudesAux = list_create();
	list_add_all(solicitudesAux, recursosSolicitados);

	//LISTA AUXILIAR DE DISPONIBLES
	t_list* disponiblesAux;
	disponiblesAux = list_create();
	list_add_all(disponiblesAux, recursosDisponibles);

	//VERIFICO QUE CADA ENTRENADOR TENGA ALGO ASIGNADO
	void _verificarAsignaciones(t_entrenador* entrenador)
	{
		bool _esEntrenadorBuscado(t_entrenador* entrenadorABuscar) {
			return entrenadorABuscar->id == entrenador->id;
		}

		t_entrenador* entrenadorConRecursos = list_find(recursosAsignados, (void*) _esEntrenadorBuscado);

		//DESCARTO DE LA LISTA DE ENTRENADORES Y DE SOLICITUDES AQUELLOS ENTRENADORES QUE NO RETIENEN RECURSOS
		if(entrenadorConRecursos == NULL)
		{
			list_remove_by_condition(entrenadoresAux, (void*) _esEntrenadorBuscado);
			list_remove_by_condition(solicitudesAux, (void*) _esEntrenadorBuscado);
		}
	}

	//VERIFICO QUE LAS SOLICITUDES SEAN POSIBLES
	void _verificarSolicitudes(t_entrenador* entrenador)
	{
		//ME FIJO QUE LA SOLICITUD SE PUEDA LLEVAR A CABO
		bool _solicitudPosible(t_recursosEntrenador* entrenadorRecursos)
		{
			bool _cantidadSuficiente (t_mapa_pokenest* recurso)
			{
				bool _buscado(t_mapa_pokenest* recursoABuscar)
				{
					return recursoABuscar->id == recurso->id;
				}

				t_mapa_pokenest* recursoBuscado = list_find(entrenadorRecursos->recursos, (void*) _buscado);

				return recurso->cantidad >= recursoBuscado->cantidad;
			}

			list_all_satisfy(disponiblesAux, (void*) _cantidadSuficiente);
			return true;
		}

		//MIENTRAS HAYA ALGO EN LA LISTA DE SOLICITUDES
		while(list_size(solicitudesAux) > 0)
		{
			//AGARRO EL PRIMER ENTRENADOR CON SU SOLICITUD
			t_recursosEntrenador* entr = list_find(solicitudesAux, (void*) _solicitudPosible);
			if(entr == NULL)
				break;
			else
			{
				//SI SE PUDO CUMPLIR CON LA SOLICITUD, DEVUELVO LOS RECURSOS Y SIGO CHEQUEANDO LOS OTROS ENTRENADORES
				void _sumarRecurso(t_mapa_pokenest* recurso)
				{
					bool _buscado(t_mapa_pokenest* recursoABuscar)
					{
						return recursoABuscar->id == recurso->id;
					}

					t_mapa_pokenest* recursoAux = list_find(entr->recursos, (void*) _buscado);

					recurso->cantidad = recurso->cantidad + recursoAux->cantidad;
				}

				list_iterate(disponiblesAux, (void*) _sumarRecurso);

				bool _isTheOne(t_recursosEntrenador* entrenadorABuscar)
				{
					return entrenadorABuscar->id == entr->id;
				}

				//REMUEVO A DICHO ENTRENADOR DE LA LISTA, YA QUE NO SE ENCUENTRA EN INTERBLOQUEO
				list_remove_by_condition(solicitudesAux, (void*) _isTheOne);
				list_remove_by_condition(entrenadoresAux, (void*) _isTheOne);
			}
		}
	}

	//VERIFICO LOS ENTRENADORES QUE TIENEN ASIGNADO ALGO.
	list_iterate(entrenadoresAux, (void*) _verificarAsignaciones);

	//CON LOS ENTRENADORES QUE QUEDARON, CHEQUEO QUE SE PUEDAN CUMPLIR SUS PETICIONES
	list_iterate(entrenadoresAux, (void*) _verificarSolicitudes);

	//SI LUEGO DE ESTOS CHEQUEOS, HAY ENTRENADORES EN LA LISTA, QUIERE DECIR QUE ESTÁN INTERBLOQUEADOS
	return list_size(entrenadoresAux) >= 2;
}

void eliminarRecursosEntrenador(t_recursosEntrenador* recursosEntrenador) {
	void _eliminarRecursos(t_mapa_pokenest* recurso) {
		void _eliminarMetadata(t_metadataPokemon* metadata) {
			free(metadata->rutaArchivo);
			free(metadata);
		}

		if(recurso->tipo != NULL)
			free(recurso->tipo);

		list_destroy_and_destroy_elements(recurso->metadatasPokemones, (void*) _eliminarMetadata);
		free(recurso);
	}

	list_destroy_and_destroy_elements(recursosEntrenador->recursos, (void*) _eliminarRecursos);
	free(recursosEntrenador);
}

void liberarMemoriaAlocada() {
	list_destroy_and_destroy_elements(recursosTotales, (void*) free);
	list_destroy_and_destroy_elements(recursosDisponibles, (void*) free);
	list_destroy_and_destroy_elements(recursosAsignados, (void*) eliminarRecursosEntrenador);
	list_destroy_and_destroy_elements(recursosSolicitados, (void*) eliminarRecursosEntrenador);
	list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
	pthread_mutex_lock(&mutexReady);
	queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
	pthread_mutex_unlock(&mutexReady);
	pthread_mutex_lock(&mutexBlocked);
	queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
	pthread_mutex_unlock(&mutexBlocked);
	pthread_mutex_lock(&mutexLog);
	log_destroy(logger);
	pthread_mutex_unlock(&mutexLog);
	pthread_mutex_destroy(&mutexEntrenadores);
	pthread_mutex_destroy(&mutexReady);
	pthread_mutex_destroy(&mutexBlocked);
	pthread_mutex_destroy(&mutexLog);
}

void eliminarEntrenadorMapa(t_entrenador* entrenadorAEliminar) {
	bool _esEntrenadorBuscado(t_entrenador* entrenador) {
		return entrenador->id == entrenadorAEliminar->id;
	}

	pthread_mutex_lock(&mutexEntrenadores);
	list_remove_and_destroy_by_condition(entrenadores, (void*) _esEntrenadorBuscado, (void*) eliminarEntrenador);
	pthread_mutex_unlock(&mutexEntrenadores);

	informarEstadoCola("Entrenadores conectados", entrenadores, &mutexEntrenadores);

	liberarRecursosEntrenador(entrenadorAEliminar);
}

void signal_handler() {
 	 struct sigaction sa;

 	 // Print PID
 	 log_info(logger, "PID del proceso Mapa: %d", getpid());

 	 // Setup the sighub handler
 	 sa.sa_handler = &signal_termination_handler;

 	 // Restart the system call
 	 sa.sa_flags = SA_RESTART;

 	 // Block every signal received during the handler execution
 	 sigfillset(&sa.sa_mask);

 	 if (sigaction(SIGUSR2, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGUSR2"); // Should not happen

 	 if (sigaction(SIGTERM, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGTERM"); // Should not happen

 	 if (sigaction(SIGINT, &sa, NULL) == -1)
 	    log_info(logger, "Error: no se puede manejar la señal SIGINT"); // Should not happen
}

void signal_termination_handler(int signum) {
 	switch (signum) {
 	case SIGUSR2:
 		configuracionActualizada = 1;

 	   	break;
 	case SIGTERM:
	      activo = 0;

	      break;
 	case SIGINT:
 		  activo = 0;

 		  break;
 	 default:
 	    log_info(logger, "Código inválido: %d", signum);

 	    return;
 	}
}

void chequearDeadlock() {
	//CORRO EL ALGORITMO SEGUN EL TIEMPO QUE ESTÁ SETEADO EN LA CONFIGURACIÓN
	sleep(configMapa.TiempoChequeoDeadlock);

	if(algoritmoDeteccion())
	{
		t_pkmn_factory* pokemon_factory = create_pkmn_factory();
		t_list* entrenadoresConPokemonesAPelear;
		entrenadoresConPokemonesAPelear = list_create();

		//LISTA AUXILIAR DE ENTRENADORES
		t_list* entrenadoresBloqueadosAux;
		entrenadoresBloqueadosAux = list_create();
		list_add_all(entrenadoresBloqueadosAux, colaBlocked->elements);

		//LOS ORDENO POR FECHA DE INGRESO
		bool _comparadorFechas(t_entrenador* entrenador1, t_entrenador* entrenador2)
		{
			double seconds = difftime(entrenador1->fechaIngreso, entrenador2->fechaIngreso);
			return seconds < 0;
		}

		list_sort(entrenadoresBloqueadosAux, (void*) _comparadorFechas);

		//CREO UN POKEMON POR CADA ENTRENADOR BLOQUEADO
		int i;
		for(i=0; i < list_size(entrenadoresBloqueadosAux); i++)
		{
			//OBTENGO POKMEON DE MAYOR NIVEL DEL ENTRENADOR PARA PELEAR
			t_pokemonEntrenador* pokemonConEntrenador;
			pokemonConEntrenador = malloc(sizeof(t_pokemonEntrenador));

			pthread_mutex_lock(&mutexBlocked);
			t_entrenador* entrenadorAux = list_get(entrenadoresBloqueadosAux, i);
			pthread_mutex_unlock(&mutexBlocked);
			*pokemonConEntrenador = obtenerPokemonMayorNivel(entrenadorAux);

			//CREO EL POKEMON DE LA "CLASE" DE LA BIBLIOTECA
			t_pokemon* pokemon = create_pokemon(pokemon_factory, pokemonConEntrenador->nombre, pokemonConEntrenador->nivel);
			pokemonConEntrenador->pokemon = pokemon;
			
			list_add(entrenadoresConPokemonesAPelear, pokemonConEntrenador);
		}
		
		//YA TENGO TODOS LOS POKEMON DE CADA ENTRENADOR, AHORA A PELEAR
		t_pokemonEntrenador* entrenadorAEliminar;
		entrenadorAEliminar = malloc(sizeof(t_pokemonEntrenador));
		entrenadorAEliminar = obtenerEntrenadorAEliminar(entrenadoresConPokemonesAPelear);

		//ARMO EL MENSAJE PARA MANDAR A LIBERAR RECURSOS
		mensaje_t mensajeLiberaRecursos;
		mensajeLiberaRecursos.tipoMensaje = INFORMA_MUERTE;

		paquete_t paqueteEliminarRecursos;
		crearPaquete((void*) &mensajeLiberaRecursos, &paqueteEliminarRecursos);

		if(paqueteEliminarRecursos.tamanioPaquete == 0)
		{
			activo = 0;
			eliminarSocket(mi_socket_s);

			pthread_mutex_lock(&mutexLog);
			log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
			pthread_mutex_unlock(&mutexLog);

			liberarMemoriaAlocada();
			nivel_gui_terminar();
			abort();
		}

		bool _esElEntrenador(t_entrenador* entrenador)
		{
			return entrenador->id == entrenadorAEliminar->id;
		}

		t_entrenador* entrenadorAux;
		entrenadorAux = list_find(entrenadoresBloqueadosAux, (void*) _esElEntrenador);

		enviarMensaje(entrenadorAux->socket, paqueteEliminarRecursos);
		free(paqueteEliminarRecursos.paqueteSerializado);

		if(entrenadorAux->socket->errorCode != NO_ERROR)
		{
			switch(entrenadorAux->socket->errorCode) {
			case ERR_PEER_DISCONNECTED:
				pthread_mutex_lock(&mutexLog);
				log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAux->socket->descriptor);
				log_info(logger, entrenadorAux->socket->error);
				pthread_mutex_unlock(&mutexLog);

				eliminarEntrenadorMapa(entrenadorAux);
				BorrarItem(items, entrenadorAux->id);
				eliminarEntrenador(entrenadorAux);
				entrenadorAux = NULL;

				informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
				informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);

				desbloquearJugadores();

				break;
			case ERR_MSG_CANNOT_BE_SENT:
				activo = 0;
				eliminarSocket(mi_socket_s);

				pthread_mutex_lock(&mutexLog);
				log_info(logger, "No se ha podido enviar un mensaje");
				log_info(logger, entrenadorAux->socket->error);
				log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
				pthread_mutex_unlock(&mutexLog);

				eliminarEntrenador(entrenadorAux);
				liberarMemoriaAlocada();
				nivel_gui_terminar();

				break;
			}
		}
	}
}

t_pokemonEntrenador obtenerPokemonMayorNivel(t_entrenador* entrenador) {
	t_pokemonEntrenador entrenadorYPokemon;

	//ME GUARDO EL ID DEL ENTRENADOR DEL POKEMON, PARA SABER CUAL ES EL PERDEDOR
	entrenadorYPokemon.idEntrenador = entrenador->id;

	return entrenadorYPokemon;
}

t_pokemonEntrenador* obtenerEntrenadorAEliminar(t_list* entrenadoresConPokemonesAPelear) {
	t_pokemonEntrenador* entrenadorPerdedor;
	bool noHayEntrenadorAEliminar = true;

	while(noHayEntrenadorAEliminar)
	{
		if(list_size(entrenadoresConPokemonesAPelear) != 0)
		{
			if(list_size(entrenadoresConPokemonesAPelear) >= 2)
			{
				t_pokemonEntrenador* entrenador1;
				t_pokemonEntrenador* entrenador2;

				entrenador1 = (t_pokemonEntrenador*)list_remove(entrenadoresConPokemonesAPelear, 0);
				entrenador2 = (t_pokemonEntrenador*)list_remove(entrenadoresConPokemonesAPelear, 0);

				//HACER PELEAR A LOS ENTRENADORES
				t_pokemon * loser = pkmn_battle(entrenador1->pokemon, entrenador2->pokemon);

				if(entrenador1->pokemon == loser)
				{
					entrenadorPerdedor = entrenador1;
				}
				else
				{
					entrenadorPerdedor = entrenador2;
				}
			}
			else
			{
				t_pokemonEntrenador* entrenadorRestante;
				entrenadorRestante = (t_pokemonEntrenador*)list_remove(entrenadoresConPokemonesAPelear, 0);

				//HACER PELEAR AL YA PERDEDOR, CON ESTE ÚLTIMO ENTRENADOR Y ASÍ SABER QUIÉN ES EL PERDEDOR
				t_pokemon* loser = pkmn_battle(entrenadorPerdedor->pokemon, entrenadorRestante->pokemon);

				if(entrenadorRestante->pokemon == loser)
				{
					entrenadorPerdedor = entrenadorRestante;
				}

				noHayEntrenadorAEliminar = false;
			}
		}
		else
			break;
	}
	
	return entrenadorPerdedor;
}

void liberarRecursosEntrenador(t_entrenador* entrenador) {
	bool _recursosEntrenador(t_recursosEntrenador* recursos) {
		return recursos->id == entrenador->id;
	}

	void _actualizarDisponibilidad(t_mapa_pokenest* recurso) {
		bool _recursoBuscado(t_mapa_pokenest* recursoBuscado) {
			return recursoBuscado->id == recurso->id;
		}

		t_mapa_pokenest* recursoAActualizar;
		recursoAActualizar = list_find(recursosDisponibles, (void*) _recursoBuscado);

		if(recursoAActualizar != NULL)
			recursoAActualizar->cantidad = recursoAActualizar->cantidad + recurso->cantidad;
	}

	t_recursosEntrenador* recursosEntrenador;

	recursosEntrenador = list_remove_by_condition(recursosAsignados, (void*) _recursosEntrenador);
	list_iterate(recursosEntrenador->recursos, (void*) _actualizarDisponibilidad);
	eliminarRecursosEntrenador(recursosEntrenador);

	list_remove_and_destroy_by_condition(recursosSolicitados, (void*) _recursosEntrenador, (void*) eliminarRecursosEntrenador);
}

void desbloquearJugadores() {
	int i;

	for(i = 0; i < list_size(colaBlocked->elements); i++) {
		t_entrenador* entrenadorBloqueado;

		bool _esEntrenador(t_entrenador* entrenador) {
			return entrenador->id == entrenadorBloqueado->id;
		}

		pthread_mutex_lock(&mutexBlocked);
		entrenadorBloqueado = list_remove(colaBlocked->elements, i);
		pthread_mutex_unlock(&mutexBlocked);

		capturarPokemon(entrenadorBloqueado);

		if(!list_any_satisfy(colaReady->elements, (void*) _esEntrenador))
		{
			pthread_mutex_lock(&mutexBlocked);
			queue_push(colaBlocked, entrenadorBloqueado);
			pthread_mutex_unlock(&mutexBlocked);
		}
		else
			i--;
	}
}

void capturarPokemon(t_entrenador* entrenador) {
	bool _recursoBuscado(t_mapa_pokenest* recursoBuscado) {
		return recursoBuscado->id == entrenador->idPokenestActual;
	}

	t_mapa_pokenest* recurso;
	recurso = list_find(recursosDisponibles, (void*) _recursoBuscado);

	if(recurso->cantidad >= 1)
	{
		recurso->cantidad--;
		restarRecurso(items, entrenador->idPokenestActual);

		actualizarMatriz(recursosSolicitados, entrenador, 0);
		actualizarMatriz(recursosAsignados, entrenador, 1);

		recurso = list_find(recursosTotales, (void*) _recursoBuscado);

		t_metadataPokemon* metadata;
		metadata = list_get(recurso->metadatasPokemones, 0);

		mensaje9_t mensajeConfirmaCaptura;

		mensajeConfirmaCaptura.tipoMensaje = CONFIRMA_CAPTURA;
		mensajeConfirmaCaptura.tamanioNombreArchivoMetadata = strlen(metadata->rutaArchivo) + 1;
		mensajeConfirmaCaptura.nombreArchivoMetadata = strdup(metadata->rutaArchivo);

		paquete_t paqueteCaptura;
		crearPaquete((void*) &mensajeConfirmaCaptura, &paqueteCaptura);
		if(paqueteCaptura.tamanioPaquete == 0)
		{
			activo = 0;
			eliminarSocket(mi_socket_s);

			pthread_mutex_lock(&mutexLog);
			log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
			pthread_mutex_unlock(&mutexLog);

			eliminarEntrenador(entrenador);
			liberarMemoriaAlocada();
			nivel_gui_terminar();

			abort();
		}

		enviarMensaje(entrenador->socket, paqueteCaptura);

		free(paqueteCaptura.paqueteSerializado);

		if(entrenador->socket->errorCode != NO_ERROR)
		{
			switch(entrenador->socket->errorCode) {
			case ERR_PEER_DISCONNECTED:
				pthread_mutex_lock(&mutexLog);
				log_info(logger, "Conexión mediante socket %d finalizada", entrenador->socket->descriptor);
				log_info(logger, entrenador->socket->error);
				pthread_mutex_unlock(&mutexLog);

				eliminarEntrenadorMapa(entrenador);
				BorrarItem(items, entrenador->id);
				eliminarEntrenador(entrenador);
				entrenador = NULL;

				informarEstadoCola("Cola Ready", colaReady->elements, &mutexReady);
				informarEstadoCola("Cola Blocked", colaBlocked->elements, &mutexBlocked);

				desbloquearJugadores();

				return;
			case ERR_MSG_CANNOT_BE_SENT:
				activo = 0;
				eliminarSocket(mi_socket_s);

				pthread_mutex_lock(&mutexLog);
				log_info(logger, "No se ha podido enviar un mensaje");
				log_info(logger, entrenador->socket->error);
				log_info(logger, "La ejecución del proceso Mapa finaliza de manera errónea");
				pthread_mutex_unlock(&mutexLog);

				eliminarEntrenador(entrenador);
				liberarMemoriaAlocada();
				nivel_gui_terminar();

				abort();
			}
		}

		log_info(logger, "Se le confirma al entrenador la captura del Pokémon solicitado (%c)", entrenador->idPokenestActual);

		//VUELVO A ENCOLAR AL ENTRENADOR
		reencolarEntrenador(entrenador);
	}
}

void actualizarMatriz(t_list* matriz, t_entrenador* entrenador, int aumentar) {
	bool _recursosEntrenador(t_recursosEntrenador* recursos) {
		return recursos->id == entrenador->id;
	}

	bool _recursoBuscado(t_mapa_pokenest* recurso) {
		return recurso->id == entrenador->idPokenestActual;
	}

	t_recursosEntrenador* recursosEntrenador;
	t_mapa_pokenest* recursoAActualizar;

	recursosEntrenador = list_find(matriz, (void*) _recursosEntrenador);
	recursoAActualizar = list_find(recursosEntrenador->recursos, (void*) _recursoBuscado);

	if(aumentar)
		recursoAActualizar->cantidad++;
	else
		recursoAActualizar->cantidad--;
}

void informarEstadoCola(char* nombreCola, t_list* cola, pthread_mutex_t* mutex) {
	char* estadoCola;

	void _informarEstado(t_entrenador* entrenador) {
		if(string_is_empty(estadoCola))
			string_append(&estadoCola, entrenador->nombre);
		else
		{
			string_append(&estadoCola, " - ");
			string_append(&estadoCola, entrenador->nombre);
		}
	}

	estadoCola = string_new();

	if(list_size(cola) > 0)
	{
		pthread_mutex_lock(mutex);
		list_iterate(cola, (void*) _informarEstado);
		pthread_mutex_unlock(mutex);

		log_info(logger, "%s: %s", nombreCola, estadoCola);
	}
	else
		log_info(logger, "%s: se encuentra vacía", nombreCola);
}

time_t obtenerFechaIngreso()
{
	time_t current_time;
	current_time = time(NULL);

	if (current_time == ((time_t)-1))
	{
		log_error(logger, "Error al obtener fecha del sistema");
		exit(EXIT_FAILURE);
	}

	return current_time;
}
