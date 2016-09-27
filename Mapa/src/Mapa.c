/*
 ============================================================================
 Name        : Mapa.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Mapa
 ============================================================================
 */

#include <stdlib.h>
#include <commons/collections/list.h>
#include <unistd.h>
#include <tad_items.h>
#include <curses.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/string.h>
#include <commons/log.h>
#include "socket.h" // BORRAR
#include "protocoloMapaEntrenador.h" // BORRAR
#include "Mapa.h"
#include "nivel.h"

#define BACKLOG 10	// Cuántas conexiones pendientes se mantienen en cola

/* Variables */
t_log* logger;
t_mapa_config configMapa;
t_list* entrenadores;
t_list* items;

//COLAS PLANIFICADOR
t_queue* colaReady;
t_queue* colaBloqueados;

int main(void) {
	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;
	int activo;

	// Variables para la diagramación del mapa
	int rows, cols;

	// Inicialización de la lista de entrenadores conectados
	entrenadores = list_create();


	//CREACIÓN DEL ARCHIVO DE LOG
	logger = log_create(LOG_FILE_PATH, "MAPA", false, LOG_LEVEL_INFO);


	//CONFIGURACIÓN DEL MAPA
	log_info(logger, "Cargando archivo de configuración");
	cargarConfiguracion(&configMapa);

	//VAMOS A VER SI FUNCIONA
	log_info(logger, "El algoritmo es: %s \n", configMapa.Algoritmo);
	log_info(logger, "Batalla: %d \n", configMapa.Batalla);
	log_info(logger, "El IP es: %s \n", configMapa.IP);
	log_info(logger, "El puerto es: %s \n", configMapa.Puerto);
	log_info(logger, "El quantum es: %d \n", configMapa.Quantum);
	log_info(logger, "El retardo es: %d \n", configMapa.Retardo);
	log_info(logger, "El tiempo de chequeo es: %d \n", configMapa.TiempoChequeoDeadlock);


	// CREACIÓN DEL HILO EN ESCUCHA
	pthread_attr_init(&atributosHilo);
	pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);


	//INICIALIZACIÓN DEL MAPA
	items = cargarPokenest(); //Carga de las Pokénest del mapa
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&rows, &cols);
	nivel_gui_dibujar(items, "CodeTogether");

	activo = 1;

	while(activo) {
		void _jugar(t_mapa_pj* entrenador) {
			//INGRESO DEL ENTRENADOR
				entrenador->pos.x = 1;
				entrenador->pos.y = 1;
			CrearPersonaje(items, entrenador->id, entrenador->pos.x, entrenador->pos.y); //Carga de entrenador
			t_list* objetivos = cargarObjetivos("C,O,D,E"); //Carga de Pokémons a buscar
			realizar_movimiento(items, *entrenador, "CodeTogether");

			//COMIENZA LA BÚSQUEDA POKÉMON!
			int cant = list_size(objetivos);
			while (cant > 0) {
				//Obtengo la ubicación de la Pokénest correspondiente a mi Pokémon
				char* pokemon = list_get(objetivos, 0);
				char pokemonID = *pokemon;
				t_mapa_pos pokenest = buscarPokenest(items, pokemonID);

				//Movimientos hasta la Pokénest
				while (((entrenador->pos.x != pokenest.x) || (entrenador->pos.y != pokenest.y)) && pokenest.cantidad > 0) {

					entrenador->pos = calcularMovimiento(entrenador->pos, pokenest);
					realizar_movimiento(items, *entrenador, "CodeTogether");

					//Si llego a la Pokénest, capturo un Pokémon
					if ((entrenador->pos.x == pokenest.x) && (entrenador->pos.y == pokenest.y)) {
						restarRecurso(items, pokemonID);  //Resto un Pokémon de la Pokénest
						BorrarItem(objetivos, pokemonID); //Quito mi objetivo
						realizar_movimiento(items, *entrenador, "CodeTogether");
					}
				}

				cant = list_size(objetivos);
			}
		}

		if(list_size(entrenadores) > 0) {
			list_iterate(entrenadores, (void*) _jugar);
		}

		activo = 0;
	}

	nivel_gui_terminar();
	// TODO Cerrar la conexión del servidor
	log_destroy(logger);
	return EXIT_SUCCESS;
}

//FUNCIONES PLANIFICADOR
void encolarNuevoEntrenador(t_mapa_pj* entrenador)
{
	//SI EL ALGORITMO ES ROUND ROBIN, LO AGREGO AL FINAL DE LA COLA DE READY
	if(string_equals_ignore_case(configMapa.Algoritmo, "RR"))
	{
		insertarAlFinal(entrenador, colaReady);
	}
	//SI ES SRDF, INSERTO ORDENADO DE MENOR A MAYOR, DE ACUERDO A CUANTO LE FALTE EJECUTAR AL ENTRENADOR
	else
	{
		calcularFaltante(*entrenador);
		insertarOrdenado(entrenador, colaReady);
	}
}

void calcularFaltante(t_mapa_pj entrenador)
{
	if(!list_is_empty(entrenador.objetivos))
	{
		char* objetivoActual = list_get(entrenador.objetivos, 0);
		char objetivoID = *objetivoActual;
		t_mapa_pos pokenest = buscarPokenest(items, objetivoID);

		int cantidad = 0;

		int distX = abs(pokenest.x - entrenador.pos.x);
		int distY = abs(pokenest.y - entrenador.pos.y);
		cantidad = distX + distY;

		entrenador.faltaEjecutar = cantidad;
	}
	else
	{
		entrenador.faltaEjecutar = 0;
	}
}

//FUNCIONES PARA COLAS PLANIFICADOR
void insertarOrdenado(t_mapa_pj* entrenador, t_queue* lista)
{
	//SEMAFORO PARA SINCRONIZAR LAS COLAS
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	//SI LA COLA ESTA VACIA, INSERTO EL ENTRENADOR SIN ORDENAR NADA
	if(queue_size(lista) == 0)
	{
		pthread_mutex_lock(&mutex);
		queue_push(lista, entrenador);
		pthread_mutex_unlock(&mutex);
	}
	else
	{
		pthread_mutex_lock(&mutex);
		queue_push(lista, entrenador);
		pthread_mutex_unlock(&mutex);

		bool _auxComparador(t_mapa_pj *entrenador1, t_mapa_pj *entrenador2)
		{
			return entrenador->faltaEjecutar < entrenador2->faltaEjecutar;
		}

		pthread_mutex_lock(&mutex);
		list_sort(lista->elements, (void*)_auxComparador);
		pthread_mutex_unlock(&mutex);
	}

	pthread_mutex_destroy(&mutex);
}

void insertarAlFinal(t_mapa_pj* entrenador, t_queue* lista)
{
	//SEMAFORO PARA SINCRONIZAR LAS COLAS
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&mutex);
	queue_push(lista, entrenador);
	pthread_mutex_unlock(&mutex);

	pthread_mutex_destroy(&mutex);
}

void realizar_movimiento(t_list* items, t_mapa_pj personaje, char * mapa) {
	MoverPersonaje(items, personaje.id, personaje.pos.x, personaje.pos.y);
	nivel_gui_dibujar(items, mapa);
	usleep(configMapa.Retardo);
}

//FUNCIÓN DE ENTRENADOR
char ejeAnterior = 'x'; //VAR de entrenador
t_mapa_pos calcularMovimiento(t_mapa_pos posActual, t_mapa_pos posFinal) {
			int cantMovimientos = 0;
			if ((ejeAnterior == 'x') || (posActual.x == posFinal.x)) {
				if (posActual.y > posFinal.y) {
					posActual.y--;
					ejeAnterior = 'y';
					cantMovimientos = 1;
				}
				if (posActual.y < posFinal.y) {
					posActual.y++;
					ejeAnterior = 'y';
					cantMovimientos = 1;
				}
			}
			if (cantMovimientos == 0) {
				if ((ejeAnterior == 'y') || (posActual.y == posFinal.y)) {
					if (posActual.x > posFinal.x) {
						posActual.x--;
						ejeAnterior = 'x';
					}
					if (posActual.x < posFinal.x) {
						posActual.x++;
						ejeAnterior = 'x';
					}
				}
			}
	return posActual;
}

ITEM_NIVEL *find_by_id(t_list* lista, char idBuscado) {
	int _is_the_one(void* p) {
		ITEM_NIVEL* castP = p;
		ITEM_NIVEL cajaComparator = *castP;
		return cajaComparator.id == idBuscado;
	}
	return list_find(lista, (void*) _is_the_one);
}

t_mapa_pos buscarPokenest(t_list* lista, char pokemon) {
	ITEM_NIVEL pokenest = *find_by_id(lista, pokemon);
	t_mapa_pos ubicacion;
	ubicacion.x= pokenest.posx;
	ubicacion.y = pokenest.posy;
	ubicacion.cantidad = pokenest.quantity;
	return ubicacion;
}

t_list* cargarObjetivos(char* objetivosString) {
	t_list* newlist = list_create();
	char** objetivos;

	void _agregarObjetivo(char* objetivo) {
		list_add(newlist, objetivo);
	}

	objetivos = string_split(objetivosString, ",");
	string_iterate_lines(objetivos, _agregarObjetivo);

	return newlist;
}

t_list* cargarPokenest() {
	t_list* newlist = list_create();
	CrearCaja(newlist, 'C', 20, 2, 10); //Charmander
	CrearCaja(newlist, 'O', 50, 10, 10); //Oddish
	CrearCaja(newlist, 'D', 10, 4, 10); //Doduo
	CrearCaja(newlist, 'E', 70, 15, 10); //Eevee
	return newlist;
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

	int error = -1;

	mi_socket_s = crearServidor(configMapa.IP, configMapa.Puerto);
	if(mi_socket_s->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, mi_socket_s->error);
		exit(error);
	}

	returnValue = escucharConexiones(*mi_socket_s, BACKLOG);
	if(returnValue != 0)
	{
		log_info(logger, strerror(returnValue));
		eliminarSocket(mi_socket_s);
		exit(error);
	}

	conectado = 1;

	while(conectado) {
		struct socket* cli_socket_s;
		t_mapa_pj* entrenador;

		entrenador = malloc(sizeof(t_mapa_pj));

		log_info(logger, "Escuchando conexiones");

		cli_socket_s = aceptarConexion(*mi_socket_s);
		if(cli_socket_s->descriptor == 0)
		{
			log_info(logger, "Se rechaza conexión");
			log_info(logger, cli_socket_s->error);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			exit(error);
		}

		// HANDSHAKE

		// Recibir mensaje CONEXION_ENTRENADOR
		mensaje1_t mensaje1;

		mensaje1.tipoMensaje = CONEXION_ENTRENADOR;
		recibirMensaje(cli_socket_s, &mensaje1);
		if(cli_socket_s->error != NULL)
		{
			log_info(logger, cli_socket_s->error);
			eliminarSocket(cli_socket_s);
		}

		if(mensaje1.tipoMensaje != CONEXION_ENTRENADOR)
		{
			// Enviar mensaje RECHAZA_CONEXION
			paquete_t paquete;
			mensaje_t mensaje3;

			mensaje3.tipoMensaje = RECHAZA_CONEXION;
			crearPaquete((void*) &mensaje3, &paquete);
			if(paquete.tamanioPaquete == 0)
			{
				log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
				eliminarSocket(cli_socket_s);
				conectado = 0;
				eliminarSocket(mi_socket_s);
				exit(error);
			}

			enviarMensaje(cli_socket_s, paquete);
			if(cli_socket_s->error != NULL)
			{
				log_info(logger, cli_socket_s->error);
				log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
				eliminarSocket(cli_socket_s);
				conectado = 0;
				eliminarSocket(mi_socket_s);
				exit(error);
			}

			log_info(logger, "Conexión mediante socket %d finalizada", cli_socket_s->descriptor);
			eliminarSocket(cli_socket_s);
		}

		log_info(logger, "Socket %d: mi nombre es %s (%c) y soy un entrenador Pokémon", cli_socket_s->descriptor, mensaje1.nombreEntrenador, mensaje1.simboloEntrenador);

		entrenador->id = mensaje1.simboloEntrenador;
		entrenador->nombre = mensaje1.nombreEntrenador;
		entrenador->socket = cli_socket_s;

		// Enviar mensaje ACEPTA_CONEXION
		paquete_t paquete;
		mensaje_t mensaje2;

		mensaje2.tipoMensaje = ACEPTA_CONEXION;
		crearPaquete((void*) &mensaje2, &paquete);
		if(paquete.tamanioPaquete == 0)
		{
			log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, "Conexión mediante socket %d finalizada", entrenador->socket->descriptor);
			eliminarSocket(entrenador->socket);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			exit(error);
		}

		enviarMensaje(entrenador->socket, paquete);
		if(entrenador->socket->error != NULL)
		{
			log_info(logger, entrenador->socket->error);
			log_info(logger, "Conexión mediante socket %d finalizada", entrenador->socket->descriptor);
			eliminarSocket(entrenador->socket);
			conectado = 0;
			eliminarSocket(mi_socket_s);
			exit(error);
		}

		list_add(entrenadores, entrenador);
		log_info(logger, "Se aceptó una conexión. Socket° %d.\n", entrenador->socket->descriptor);
	}
}
