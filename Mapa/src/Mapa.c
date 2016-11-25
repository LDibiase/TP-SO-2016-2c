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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
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

#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola
#define TAMANIO_MAXIMO_MENSAJE 50 // Tamaño máximo de un mensaje

/* Variables globales */
t_log* logger; // Archivo de log
t_mapa_config configMapa; // Datos de configuración
t_list* entrenadores; // Entrenadores conectados al mapa
t_list* items; // Items existentes en el mapa (entrenadores y PokéNests)
t_list* recursosTotales;
t_list* recursosDisponibles;
t_list* recursosAsignados;
t_list* recursosSolicitados;

//COLAS DE PLANIFICACIÓN
t_queue* colaReady;
t_queue* colaBlocked;

//SEMÁFOROS PARA SINCRONIZAR LA LISTA Y LAS COLAS
pthread_mutex_t mutexEntrenadores;
pthread_mutex_t mutexReady;
pthread_mutex_t mutexBlocked;

//SEMÁFORO PARA SINCRONIZAR EL ARCHIVO DE LOG
pthread_mutex_t mutexLog;

int main(void) {
	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;

	// Variables para la creación del hilo verificador de deadlock
	pthread_t hiloDeadlock;
	pthread_attr_t atributosHiloDeadlock;

	// Flag de actividad
	int activo;

	//INICIALIZACIÓN DE LOS SEMÁFOROS
	pthread_mutex_init(&mutexEntrenadores, NULL);
	pthread_mutex_init(&mutexReady, NULL);
	pthread_mutex_init(&mutexBlocked, NULL);
	pthread_mutex_init(&mutexLog, NULL);

	// Variables para la diagramación del mapa
	int rows, cols;

	//CREACIÓN DEL ARCHIVO DE LOG
	logger = log_create(LOG_FILE_PATH, "MAPA", false, LOG_LEVEL_INFO);

	//CONFIGURACIÓN DEL MAPA
	pthread_mutex_lock(&mutexLog);
	log_info(logger, "Cargando archivo de configuración");

	if (cargarConfiguracion(&configMapa) == 1)
	{
		log_info(logger, "La ejecución del proceso finaliza de manera errónea");
		log_destroy(logger);
		pthread_mutex_destroy(&mutexEntrenadores);
		pthread_mutex_destroy(&mutexReady);
		pthread_mutex_destroy(&mutexBlocked);
		pthread_mutex_destroy(&mutexLog);
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

	// Creación de la lista de entrenadores conectados y las colas de planificación
	recursosTotales = list_create();
	recursosDisponibles = list_create();
	recursosAsignados = list_create();
	recursosSolicitados = list_create();
	entrenadores = list_create();
	colaReady = queue_create();
	colaBlocked = queue_create();

	//INICIALIZACIÓN DEL MAPA
	items = cargarPokenests(); //Carga de las Pokénest del mapa
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&rows, &cols);
	nivel_gui_dibujar(items, "CodeTogether");

	//CREACIÓN DEL HILO EN ESCUCHA
	pthread_attr_init(&atributosHilo);
	pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);

	//CREACIÓN DEL HILO VERIFICADOR DE DEADLOCK
	pthread_attr_init(&atributosHiloDeadlock);
	pthread_create(&hiloDeadlock, &atributosHiloDeadlock, (void*) chequearDeadlock, NULL);
	pthread_attr_destroy(&atributosHiloDeadlock);

	//MENSAJES A UTILIZAR
	mensaje5_t mensajeBrindaUbicacion;
	mensaje7_t mensajeConfirmaDesplazamiento;
	mensaje_t mensajeConfirmaCaptura;

	// Se setea en 1 (on) el flag de actividad
	activo = 1;

	while(activo) {

		if(list_size(colaReady->elements) > 0)
		{
			pthread_mutex_lock(&mutexReady);
			t_entrenador* entrenadorAEjecutar = queue_pop(colaReady);
			pthread_mutex_unlock(&mutexReady);

			//SE ATIENDEN LAS SOLICITUDES DEL PRIMER ENTRENADOR EN LA COLA DE LISTOS
			int solicitoCaptura = 0;
			entrenadorAEjecutar->utEjecutadas = 0;

			while((string_equals_ignore_case(configMapa.Algoritmo, "RR") && entrenadorAEjecutar->utEjecutadas < configMapa.Quantum && !solicitoCaptura) ||
					(string_equals_ignore_case(configMapa.Algoritmo, "SRDF") && !solicitoCaptura))
			{
				void* mensajeRespuesta = malloc(TAMANIO_MAXIMO_MENSAJE);
				((mensaje_t*) mensajeRespuesta)->tipoMensaje = INDEFINIDO;

				recibirMensaje(entrenadorAEjecutar->socket, mensajeRespuesta);
				if(entrenadorAEjecutar->socket->error != NULL)
				{
					free(mensajeRespuesta);
					queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
					queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
					list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
					pthread_mutex_lock(&mutexLog);
					log_info(logger, entrenadorAEjecutar->socket->error);
					log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
					log_info(logger, "La ejecución del proceso finaliza de manera errónea");
					pthread_mutex_unlock(&mutexLog);
					log_destroy(logger);
					pthread_mutex_destroy(&mutexEntrenadores);
					pthread_mutex_destroy(&mutexReady);
					pthread_mutex_destroy(&mutexBlocked);
					pthread_mutex_destroy(&mutexLog);
					return EXIT_FAILURE;
				}

				//HAGO UN SWITCH PARA DETERMINAR QUÉ ACCIÓN DESEA REALIZAR EL ENTRENADOR
				t_ubicacion pokeNestSolicitada;
				switch(((mensaje_t*) mensajeRespuesta)->tipoMensaje) {
				case SOLICITA_UBICACION:
					log_info(logger, "Socket %d: solicito ubicación de la PokéNest", entrenadorAEjecutar->socket->descriptor, ((mensaje4_t*) mensajeRespuesta)->idPokeNest);

					pokeNestSolicitada = buscarPokenest(items, ((mensaje4_t*) mensajeRespuesta)->idPokeNest);
					entrenadorAEjecutar->idPokenestActual = ((mensaje4_t*) mensajeRespuesta)->idPokeNest;

					mensajeBrindaUbicacion.tipoMensaje = BRINDA_UBICACION;
					mensajeBrindaUbicacion.ubicacionX = pokeNestSolicitada.x;
					mensajeBrindaUbicacion.ubicacionY = pokeNestSolicitada.y;

					paquete_t paquetePokenest;
					crearPaquete((void*) &mensajeBrindaUbicacion, &paquetePokenest);
					if(paquetePokenest.tamanioPaquete == 0)
					{
						free(mensajeRespuesta);
						queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
						queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
						list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
						pthread_mutex_lock(&mutexLog);
						log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
						log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
						log_info(logger, "La ejecución del proceso finaliza de manera errónea");
						pthread_mutex_unlock(&mutexLog);
						log_destroy(logger);
						pthread_mutex_destroy(&mutexEntrenadores);
						pthread_mutex_destroy(&mutexReady);
						pthread_mutex_destroy(&mutexBlocked);
						pthread_mutex_destroy(&mutexLog);
						return EXIT_FAILURE;
					}

					enviarMensaje(entrenadorAEjecutar->socket, paquetePokenest);
					if(entrenadorAEjecutar->socket->error != NULL)
					{
						free(paquetePokenest.paqueteSerializado);
						free(mensajeRespuesta);
						log_info(logger, entrenadorAEjecutar->socket->error);
						log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
						//TODO VERIFICAR SI ES CORRECTO BORRAR EL SOCKET
						eliminarSocket(entrenadorAEjecutar->socket);
						exit(-1);
					}

					free(paquetePokenest.paqueteSerializado);
					log_info(logger, "Se envía ubicación de la PokéNest %c al entrenador", ((mensaje4_t*) mensajeRespuesta)->idPokeNest);
					free(mensajeRespuesta);

					//CARGO NUEVAMENTE LA CONFIGURACIÓN DEL MAPA POR SI FUE MODIFICADA EXTERNAMENTE
					//TODO IMPLEMENTAR SIGNAL PARA RECARGAR CONFIGURACIÓN
					log_info(logger, "Cargando archivo de configuración");
					if (cargarConfiguracion(&configMapa) == 1)
						return EXIT_FAILURE;

					entrenadorAEjecutar->utEjecutadas++;

					break;
				case SOLICITA_DESPLAZAMIENTO:
					mensajeConfirmaDesplazamiento.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;

					//MODIFICO LA UBICACIÓN DEL ENTRENADOR DE ACUERDO A LA DIRECCIÓN DE DESPLAZAMIENTO SOLICITADA
					switch(((mensaje6_t*) mensajeRespuesta)->direccion)
					{
					case ARRIBA:
						log_info(logger, "Socket %d: solicito desplazamiento hacia arriba", entrenadorAEjecutar->socket->descriptor);
						(entrenadorAEjecutar->ubicacion.y)--;
						break;
					case ABAJO:
						log_info(logger, "Socket %d: solicito desplazamiento hacia abajo", entrenadorAEjecutar->socket->descriptor);
						(entrenadorAEjecutar->ubicacion.y)++;
						break;
					case IZQUIERDA:
						log_info(logger, "Socket %d: solicito desplazamiento hacia la izquierda", entrenadorAEjecutar->socket->descriptor);
						(entrenadorAEjecutar->ubicacion.x)--;
						break;
					case DERECHA:
						log_info(logger, "Socket %d: solicito desplazamiento hacia la derecha", entrenadorAEjecutar->socket->descriptor);
						(entrenadorAEjecutar->ubicacion.x)++;
						break;
					}

					realizar_movimiento(items, *entrenadorAEjecutar, "CodeTogether");

					//LE ENVÍO AL ENTRENADOR SU NUEVA UBICACIÓN
					mensajeConfirmaDesplazamiento.ubicacionX = entrenadorAEjecutar->ubicacion.x;
					mensajeConfirmaDesplazamiento.ubicacionY = entrenadorAEjecutar->ubicacion.y;

					paquete_t paqueteDesplazamiento;
					crearPaquete((void*) &mensajeConfirmaDesplazamiento, &paqueteDesplazamiento);
					if(paqueteDesplazamiento.tamanioPaquete == 0)
					{
						free(mensajeRespuesta);
						log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
						log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
						//TODO VERIFICAR SI ES CORRECTO BORRAR EL SOCKET
						eliminarSocket(entrenadorAEjecutar->socket);
						exit(-1);
					}

					enviarMensaje(entrenadorAEjecutar->socket, paqueteDesplazamiento);
					if(entrenadorAEjecutar->socket->error != NULL)
					{
						free(paqueteDesplazamiento.paqueteSerializado);
						free(mensajeRespuesta);
						queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
						queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
						list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
						pthread_mutex_lock(&mutexLog);
						log_info(logger, entrenadorAEjecutar->socket->error);
						log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
						log_info(logger, "La ejecución del proceso finaliza de manera errónea");
						pthread_mutex_unlock(&mutexLog);
						log_destroy(logger);
						pthread_mutex_destroy(&mutexEntrenadores);
						pthread_mutex_destroy(&mutexReady);
						pthread_mutex_destroy(&mutexBlocked);
						pthread_mutex_destroy(&mutexLog);
						return EXIT_FAILURE;
					}

					free(paqueteDesplazamiento.paqueteSerializado);
					free(mensajeRespuesta);
					log_info(logger, "Se le informa al entrenador su nueva posición: (%d,%d)", entrenadorAEjecutar->ubicacion.x, entrenadorAEjecutar->ubicacion.y);

					//CARGO NUEVAMENTE LA CONFIGURACIÓN DEL MAPA POR SI FUE MODIFICADA EXTERNAMENTE
					//TODO IMPLEMENTAR SIGNAL PARA RECARGAR CONFIGURACIÓN
					log_info(logger, "Cargando archivo de configuración");
					if (cargarConfiguracion(&configMapa) == 1)
						return EXIT_FAILURE;

					entrenadorAEjecutar->utEjecutadas++;

					break;
					case SOLICITA_CAPTURA:
						log_info(logger, "Socket %d: solicito capturar Pokémon", entrenadorAEjecutar->socket->descriptor);

						solicitoCaptura = 1;

						mensajeConfirmaCaptura.tipoMensaje = CONFIRMA_CAPTURA;

						paquete_t paqueteCaptura;
						crearPaquete((void*) &mensajeConfirmaCaptura, &paqueteCaptura);
						if(paqueteCaptura.tamanioPaquete == 0)
						{
							free(mensajeRespuesta);
							queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
							queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
							list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
							pthread_mutex_lock(&mutexLog);
							log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
							log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
							log_info(logger, "La ejecución del proceso finaliza de manera errónea");
							pthread_mutex_unlock(&mutexLog);
							log_destroy(logger);
							pthread_mutex_destroy(&mutexEntrenadores);
							pthread_mutex_destroy(&mutexReady);
							pthread_mutex_destroy(&mutexBlocked);
							pthread_mutex_destroy(&mutexLog);
							return EXIT_FAILURE;
						}

						//TODO LÓGICA DE ASIGNACIÓN DE POKÉMON AL ENTRENADOR
						restarRecurso(items, entrenadorAEjecutar->idPokenestActual);

						enviarMensaje(entrenadorAEjecutar->socket, paqueteCaptura);
						if(entrenadorAEjecutar->socket->error != NULL)
						{
							free(paqueteCaptura.paqueteSerializado);
							free(mensajeRespuesta);
							log_info(logger, entrenadorAEjecutar->socket->error);
							log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
							//TODO VERIFICAR SI ES CORRECTO BORRAR EL SOCKET
							eliminarSocket(entrenadorAEjecutar->socket);
							exit(-1);
						}

						free(paqueteCaptura.paqueteSerializado);
						free(mensajeRespuesta);
						log_info(logger, "Se le confirma al entrenador la captura del Pokémon solicitado (%c)", entrenadorAEjecutar->idPokenestActual);

						//CARGO NUEVAMENTE LA CONFIGURACIÓN DEL MAPA POR SI FUE MODIFICADA EXTERNAMENTE
						//TODO IMPLEMENTAR SIGNAL PARA RECARGAR CONFIGURACIÓN
						log_info(logger, "Cargando archivo de configuración");
						if (cargarConfiguracion(&configMapa) == 1)
							return EXIT_FAILURE;

						entrenadorAEjecutar->utEjecutadas++;

						//VUELVO A ENCOLAR AL ENTRENADOR
						reencolarEntrenador(entrenadorAEjecutar);
				}
			}
		}
	}

	nivel_gui_terminar();
	// TODO Cerrar la conexión del servidor
	queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
	queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
	list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
	log_destroy(logger);
	pthread_mutex_destroy(&mutexEntrenadores);
	pthread_mutex_destroy(&mutexReady);
	pthread_mutex_destroy(&mutexBlocked);
	pthread_mutex_destroy(&mutexLog);
	return EXIT_SUCCESS;
}

/*
log_info(logger, "Socket %d: he completado todos mis objetivos", entrenadorAEjecutar->socket->descriptor);

//TODO LÓGICA DE LIBERACIÓN DE RECURSOS
BorrarItem(items, entrenadorAEjecutar->id);

free(mensajeRespuesta);
log_info(logger, "El entrenador %s (%c) ha completado todos sus objetivos dentro del mapa", entrenadorAEjecutar->nombre, entrenadorAEjecutar->id);

//CARGO NUEVAMENTE LA CONFIGURACIÓN DEL MAPA POR SI FUE MODIFICADA EXTERNAMENTE
log_info(logger, "Cargando archivo de configuración");
if (cargarConfiguracion(&configMapa) == 1)
	return EXIT_FAILURE;
 */

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
}

void calcularFaltante(t_entrenador entrenador)
{
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
	usleep(configMapa.Retardo);
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

	struct dirent* dent;

	DIR* srcdir = opendir("PokeNests");
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
			char* str = string_new();

			string_append(&str, "PokeNests/");
			string_append(&str, dent->d_name);
			string_append(&str, "/metadata");

	        pokenestLeida = leerPokenest(str);
	        CrearCaja(newlist, pokenestLeida.id, pokenestLeida.ubicacion.x, pokenestLeida.ubicacion.y, 10);
	    	recursoTotales = malloc(sizeof(pokenestLeida));
	    	recursoDisponibles = malloc(sizeof(pokenestLeida));
	        *recursoTotales = pokenestLeida;
	        *recursoDisponibles = pokenestLeida;
	        list_add(recursosTotales, recursoTotales);
	        list_add(recursosDisponibles, recursoDisponibles);

	        log_info(logger, "Se cargó la PokéNest: %c", pokenestLeida.id);
	    	free(str);
		}
	}

	closedir(srcdir);

	return newlist;
}

t_mapa_pokenest leerPokenest(char* metadata)
{
	t_config* config;
	t_mapa_pokenest structPokenest;

	config = config_create(metadata);

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
		log_error(logger, "La PokéNest tiene un formato inválido");
		config_destroy(config);
	}
	return structPokenest;
}

int cargarConfiguracion(t_mapa_config* structConfig)
{
	t_config* config;
	config = config_create(CONFIG_FILE_PATH);

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
		log_error(logger, "El archivo de configuración tiene un formato inválido");
		config_destroy(config);
		return 1;
	}
}

void aceptarConexiones() {
	struct socket* mi_socket_s;

	int returnValue;
	int conectado;

	mi_socket_s = crearServidor(configMapa.IP, configMapa.Puerto);
	if(mi_socket_s->descriptor == 0)
	{
		queue_destroy_and_destroy_elements(colaBlocked, (void*) eliminarEntrenador);
		queue_destroy_and_destroy_elements(colaReady, (void*) eliminarEntrenador);
		list_destroy_and_destroy_elements(entrenadores, (void*) eliminarEntrenador);
		pthread_mutex_lock(&mutexLog);
		log_info(logger, "Conexión fallida");
		log_info(logger, mi_socket_s->error);
		log_info(logger, "La ejecución del proceso finaliza de manera errónea");
		pthread_mutex_unlock(&mutexLog);
		log_destroy(logger);
		pthread_mutex_destroy(&mutexEntrenadores);
		pthread_mutex_destroy(&mutexReady);
		pthread_mutex_destroy(&mutexBlocked);
		pthread_mutex_destroy(&mutexLog);
		abort();
	}

	returnValue = escucharConexiones(*mi_socket_s, BACKLOG);
	if(returnValue != 0)
	{
		log_info(logger, strerror(returnValue));
		eliminarSocket(mi_socket_s);
		abort();
	}

	conectado = 1;

	while(conectado) {
		struct socket* cli_socket_s;
		t_entrenador* entrenador;

		entrenador = malloc(sizeof(t_entrenador));

		log_info(logger, "Escuchando conexiones");

		cli_socket_s = aceptarConexion(*mi_socket_s);
		if(cli_socket_s->descriptor == 0)
		{
			log_info(logger, "Se rechaza conexión");
			log_info(logger, cli_socket_s->error);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			abort();
		}

		///////////////////////
		////// HANDSHAKE //////
		///////////////////////

		// Recibir mensaje CONEXION_ENTRENADOR
		mensaje1_t mensajeConexionEntrenador;

		mensajeConexionEntrenador.tipoMensaje = CONEXION_ENTRENADOR;
		recibirMensaje(cli_socket_s, &mensajeConexionEntrenador);
		if(cli_socket_s->error != NULL)
		{
			log_info(logger, cli_socket_s->error);
			eliminarSocket(cli_socket_s);
		}

		if(mensajeConexionEntrenador.tipoMensaje != CONEXION_ENTRENADOR)
		{
			// Enviar mensaje RECHAZA_CONEXION
			paquete_t paquete;
			mensaje_t mensajeRechazaConexion;

			mensajeRechazaConexion.tipoMensaje = RECHAZA_CONEXION;
			crearPaquete((void*) &mensajeRechazaConexion, &paquete);
			if(paquete.tamanioPaquete == 0)
			{
				log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
				eliminarSocket(cli_socket_s);
				conectado = 0;
				eliminarSocket(mi_socket_s);
				abort();
			}

			enviarMensaje(cli_socket_s, paquete);
			if(cli_socket_s->error != NULL)
			{
				log_info(logger, cli_socket_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
				eliminarSocket(cli_socket_s);
				conectado = 0;
				eliminarSocket(mi_socket_s);
				abort();
			}

			free(paquete.paqueteSerializado);

			log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
			eliminarSocket(cli_socket_s);
		}

		log_info(logger, "Socket %d: mi nombre es %s (%c) y soy un entrenador Pokémon", cli_socket_s->descriptor, mensajeConexionEntrenador.nombreEntrenador, mensajeConexionEntrenador.simboloEntrenador);

		entrenador->id = mensajeConexionEntrenador.simboloEntrenador;
		entrenador->nombre = mensajeConexionEntrenador.nombreEntrenador;
		entrenador->socket = cli_socket_s;
		entrenador->ubicacion.x = 1;
		entrenador->ubicacion.y = 1;

		// Enviar mensaje ACEPTA_CONEXION
		paquete_t paquete;
		mensaje_t mensajeAceptaConexion;

		mensajeAceptaConexion.tipoMensaje = ACEPTA_CONEXION;
		crearPaquete((void*) &mensajeAceptaConexion, &paquete);
		if(paquete.tamanioPaquete == 0)
		{
			log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, "Conexión mediante socket %d finalizada", entrenador->socket->descriptor);
			eliminarSocket(entrenador->socket);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			abort();
		}

		enviarMensaje(entrenador->socket, paquete);
		if(entrenador->socket->error != NULL)
		{
			log_info(logger, entrenador->socket->error);
			log_info(logger, "Conexión mediante socket %d finalizada", entrenador->socket->descriptor);
			eliminarSocket(entrenador->socket);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			abort();
		}

		free(paquete.paqueteSerializado);

		//Se agrega al entrenador a la lista de entrenadores conectados
		list_add(entrenadores, entrenador);

		log_info(logger, "Se aceptó una conexión (socket %d)", entrenador->socket->descriptor);

		//SE PLANIFICA AL NUEVO ENTRENADOR
		t_entrenador* entrenadorPlanificado;

		entrenadorPlanificado = malloc(sizeof(t_entrenador));
		*entrenadorPlanificado = *entrenador;

		encolarEntrenador(entrenadorPlanificado);

		t_recursosEntrenador* recursosAsignadosEntrenador;
		t_recursosEntrenador* recursosSolicitadosEntrenador;

		recursosAsignadosEntrenador = malloc(sizeof(t_recursosEntrenador));
		recursosSolicitadosEntrenador = malloc(sizeof(t_recursosEntrenador));

		recursosAsignadosEntrenador->recursos = list_create();
		recursosSolicitadosEntrenador->recursos = list_create();

		void _agregarRecursosEntrenador(t_mapa_pokenest* recurso)
		{
			t_mapa_pokenest* recursoAsignado;
			t_mapa_pokenest* recursoSolicitado;

			recursoAsignado = malloc(sizeof(t_mapa_pokenest));
			recursoSolicitado = malloc(sizeof(t_mapa_pokenest));

			recursoAsignado->id = recurso->id;
			recursoAsignado->cantidad = 10; //*recurso->cantidad;

			recursoSolicitado->id = recurso->id;
			recursoSolicitado->cantidad = 10; //*recurso->cantidad;

			list_add(recursosAsignadosEntrenador->recursos, recursoAsignado);
			list_add(recursosSolicitadosEntrenador->recursos, recursoSolicitado);
		}

		list_iterate(recursosTotales, (void*) _agregarRecursosEntrenador);

		list_add(recursosAsignados, (void*) recursosAsignadosEntrenador);
		list_add(recursosSolicitados, (void*) recursosSolicitadosEntrenador);

		/*pokenestLeida = leerPokenest(str);
		CrearCaja(newlist, pokenestLeida.id, pokenestLeida.ubicacion.x, pokenestLeida.ubicacion.y, 10);
		recursoTotales = malloc(sizeof(pokenestLeida));
		recursoDisponibles = malloc(sizeof(pokenestLeida));
		*recursoTotales = pokenestLeida;
		*recursoDisponibles = pokenestLeida;
		list_add(recursosTotales, recursoTotales);
		list_add(recursosDisponibles, recursoDisponibles);*/


		log_info(logger, "Se planificó al entrenador %s (%c)", entrenadorPlanificado->nombre, entrenadorPlanificado->id);
	}
}

void eliminarEntrenador(t_entrenador* entrenador) {
	free(entrenador->nombre);
	eliminarSocket(entrenador->socket);
	free(entrenador);
}

bool algoritmoDeteccion()
{
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
		bool _isTheOne(t_entrenador* entrenadorABuscar) {
			return entrenadorABuscar->id == entrenador->id;
		}

		t_entrenador* entrenadorConRecursos = list_find(recursosAsignados, (void*) _isTheOne);

		//LOS QUE NO TIENEN NADA, LOS DESCARTO DE LA LISTA DE ENTRENADORES Y DE SOLICITUDES.
		if(entrenadorConRecursos == NULL)
		{
			list_remove_by_condition(entrenadoresAux, (void*) _isTheOne);
			list_remove_by_condition(solicitudesAux, (void*) _isTheOne);
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

void chequearDeadlock()
{
	if(algoritmoDeteccion())
	{
		t_pkmn_factory* pokemon_factory = create_pkmn_factory();
		t_list* entrenadoresConPokemonesAPelear;
		entrenadoresConPokemonesAPelear = list_create();

		//CREO UN POKEMON POR CADA ENTRENADOR BLOQUEADO
		int i;
		for(i=0; i < list_size(colaBlocked->elements); i++)
		{
			//OBTENGO POKMEON DE MAYOR NIVEL DEL ENTRENADOR PARA PELEAR
			t_pokemonEntrenador* pokemonConEntrenador;
			pokemonConEntrenador = malloc(sizeof(t_pokemonEntrenador));

			t_entrenador* entrenadorAux = list_get(colaBlocked->elements, i);
			*pokemonConEntrenador = obtenerPokemonMayorNivel(entrenadorAux);

			//CREO EL POKEMON DE LA "CLASE" DE LA BIBLIOTECA
			t_pokemon* pokemon = create_pokemon(pokemon_factory, pokemonConEntrenador->nombre, pokemonConEntrenador->nivel);
			pokemonConEntrenador->pokemon = pokemon;
			
			list_add(entrenadoresConPokemonesAPelear, pokemonConEntrenador);
		}
		
		//YA TENGO TODOS LOS POKEMON DE CADA ENTRENADOR, AHORA A PELEAR
		t_pokemonEntrenador* entrenadorAEliminar;
		entrenadorAEliminar = malloc(sizeof(t_pokemonEntrenador));
		*entrenadorAEliminar = obtenerEntrenadorAEliminar(entrenadoresConPokemonesAPelear);

		//LLAMAR FUNCION PARA LIBERAR RECURSOS
	}
}

t_pokemonEntrenador obtenerEntrenadorAEliminar(t_list* entrenadoresConPokemonesAPelear)
{
	t_pokemonEntrenador entrenadorYPokemon;
	
	return entrenadorYPokemon;
}

t_pokemonEntrenador obtenerPokemonMayorNivel(t_entrenador* entrenador)
{
	t_pokemonEntrenador entrenadorYPokemon;

	//ME GUARDO EL ID DEL ENTRENADOR DEL POKEMON, PARA SABER CUAL ES EL PERDEDOR
	entrenadorYPokemon.idEntrenador = entrenador->id;

	return entrenadorYPokemon;
}
