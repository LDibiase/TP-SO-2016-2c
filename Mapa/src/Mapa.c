/*
 ============================================================================
 Name        : Mapa.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Mapa
 ============================================================================
 */

#include <stdlib.h>
#include <unistd.h>
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
#include "socket.h" // BORRAR
#include "Mapa.h"
#include "nivel.h"

#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola
#define TAMANIO_MAXIMO_MENSAJE 50 // Tamaño máximo de un mensaje

/* Variables globales */
t_log* logger;
t_mapa_config configMapa;
t_list* entrenadores;
t_list* items;

//COLAS PLANIFICADOR
t_queue* colaReady;
t_queue* colaBloqueados;

//SEMÁFORO PARA SINCRONIZAR LAS COLAS
pthread_mutex_t mutex;

int main(void) {
	// Variables para la creación del hilo en escucha
	pthread_t hiloEnEscucha;
	pthread_attr_t atributosHilo;
	int activo;

	//SE INICIALIZA SEMAFORO
	pthread_mutex_init(&mutex, NULL);

	// Variables para la diagramación del mapa
	int rows, cols;

	// Creación de las colas de planificación
	entrenadores = list_create();
	colaReady = queue_create();
	colaBloqueados = queue_create();


	//CREACIÓN DEL ARCHIVO DE LOG
	logger = log_create(LOG_FILE_PATH, "MAPA", false, LOG_LEVEL_INFO);


	//CONFIGURACIÓN DEL MAPA
	log_info(logger, "Cargando archivo de configuración");
	if (cargarConfiguracion(&configMapa) == 1)
		return EXIT_FAILURE;

	log_info(logger, "El algoritmo de planificación es: %s \n", configMapa.Algoritmo);
	log_info(logger, "Batalla: %d \n", configMapa.Batalla);
	log_info(logger, "El IP es: %s \n", configMapa.IP);
	log_info(logger, "El puerto es: %s \n", configMapa.Puerto);
	log_info(logger, "El quantum es: %d \n", configMapa.Quantum);
	log_info(logger, "El retardo es: %d \n", configMapa.Retardo);
	log_info(logger, "El tiempo de chequeo es: %d \n", configMapa.TiempoChequeoDeadlock);


	//INICIALIZACIÓN DEL MAPA
	items = cargarPokenest(); //Carga de las Pokénest del mapa
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&rows, &cols);
	nivel_gui_dibujar(items, "CodeTogether");


	// CREACIÓN DEL HILO EN ESCUCHA
	pthread_attr_init(&atributosHilo);
	pthread_create(&hiloEnEscucha, &atributosHilo, (void*) aceptarConexiones, NULL);
	pthread_attr_destroy(&atributosHilo);


	//MENSAJES A UTILIZAR
	mensaje6_t mensajeBrindaUbicacion;
	mensaje8_t mensajeConfirmaDesplazamiento;
	mensaje_t mensajeSolicitaCaptura;

	activo = 1;

	while(activo) {

		if(list_size(entrenadores) > 0)
		{
			pthread_mutex_lock(&mutex);
			t_entrenador* entrenadorAEjecutar = queue_pop(colaReady);
			pthread_mutex_unlock(&mutex);

			//LE AVISO AL ENTRENADOR QUE SE LE CONCEDIÓ UN TURNO
			mensaje_t mensajeTurno;
			paquete_t paquete;

			mensajeTurno.tipoMensaje = TURNO;
			crearPaquete((void*) &mensajeTurno, &paquete);

			if(paquete.tamanioPaquete == 0)
			{
				log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
				log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
				//TODO Cerrar todos los sockets y salir
				eliminarSocket(entrenadorAEjecutar->socket);
				return EXIT_FAILURE;
			}

			enviarMensaje(entrenadorAEjecutar->socket, paquete);

			if(entrenadorAEjecutar->socket->error != NULL)
			{
				free(paquete.paqueteSerializado);
				log_info(logger, entrenadorAEjecutar->socket->error);
				log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
				//TODO Cerrar todos los sockets y salir
				eliminarSocket(entrenadorAEjecutar->socket);
				return EXIT_FAILURE;
			}

			free(paquete.paqueteSerializado);
			log_info(logger, "Se concede un turno al entrenador %s (%c)", entrenadorAEjecutar->nombre, entrenadorAEjecutar->id);

			//FALSO POLIMORFISMO
			void* mensajeRespuesta = malloc(TAMANIO_MAXIMO_MENSAJE);
			((mensaje_t*) mensajeRespuesta)->tipoMensaje = INDEFINIDO;

			recibirMensaje(entrenadorAEjecutar->socket, mensajeRespuesta);

			if(entrenadorAEjecutar->socket->error != NULL)
			{
				free(mensajeRespuesta);
				log_info(logger, entrenadorAEjecutar->socket->error);
				log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
				//TODO Cerrar todos los sockets y salir
				eliminarSocket(entrenadorAEjecutar->socket);
				return EXIT_FAILURE;
			}

			//HAGO UN SWITCH PARA DETERMINAR QUÉ ACCIÓN QUIERE REALIZAR EL ENTRENADOR
			t_ubicacion pokeNestSolicitada;
			switch(((mensaje_t*) mensajeRespuesta)->tipoMensaje) {
			case SOLICITA_UBICACION:
				log_info(logger, "Socket %d: solicito ubicación de la PokéNest", entrenadorAEjecutar->socket->descriptor, ((mensaje5_t*) mensajeRespuesta)->idPokeNest);

				pokeNestSolicitada = buscarPokenest(items, ((mensaje5_t*) mensajeRespuesta)->idPokeNest);
				entrenadorAEjecutar->idPokenestActual = ((mensaje5_t*) mensajeRespuesta)->idPokeNest;

				mensajeBrindaUbicacion.tipoMensaje = BRINDA_UBICACION;
				mensajeBrindaUbicacion.ubicacionX = pokeNestSolicitada.x;
				mensajeBrindaUbicacion.ubicacionY = pokeNestSolicitada.y;

				paquete_t paquetePokenest;
				crearPaquete((void*) &mensajeBrindaUbicacion, &paquetePokenest);

				if(paquetePokenest.tamanioPaquete == 0)
				{
					free(mensajeRespuesta);
					log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
					//TODO VERIFICAR SI ES CORRECTO BORRAR EL SOCKET
					eliminarSocket(entrenadorAEjecutar->socket);
					exit(-1);
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
				log_info(logger, "Se envía ubicación de la PokéNest %c al entrenador", ((mensaje5_t*) mensajeRespuesta)->idPokeNest);
				free(mensajeRespuesta);

				//VUELVO A ENCOLAR AL ENTRENADOR
				reencolarEntrenador(entrenadorAEjecutar);

				break;
			case SOLICITA_DESPLAZAMIENTO:
				mensajeConfirmaDesplazamiento.tipoMensaje = CONFIRMA_DESPLAZAMIENTO;

				//MODIFICO LA UBICACIÓN DEL ENTRENADOR DE ACUERDO A LA DIRECCIÓN DE DESPLAZAMIENTO SOLICITADA
				switch(((mensaje7_t*) mensajeRespuesta)->direccion)
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
					log_info(logger, entrenadorAEjecutar->socket->error);
					log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
					//TODO VERIFICAR SI ES CORRECTO BORRAR EL SOCKET
					eliminarSocket(entrenadorAEjecutar->socket);
					exit(-1);
				}

				free(paqueteDesplazamiento.paqueteSerializado);
				free(mensajeRespuesta);
				log_info(logger, "Se le informa al entrenador su nueva posición: (%d,%d)", entrenadorAEjecutar->ubicacion.x, entrenadorAEjecutar->ubicacion.y);

				//VUELVO A ENCOLAR AL ENTRENADOR
				reencolarEntrenador(entrenadorAEjecutar);

				break;
			case SOLICITA_CAPTURA:
				log_info(logger, "Socket %d: solicito capturar Pokémon", entrenadorAEjecutar->socket->descriptor);

				mensajeSolicitaCaptura.tipoMensaje = CONFIRMA_CAPTURA;

				paquete_t paqueteCaptura;
				crearPaquete((void*) &mensajeSolicitaCaptura, &paqueteCaptura);

				if(paqueteCaptura.tamanioPaquete == 0)
				{
					free(mensajeRespuesta);
					log_info(logger, "No se ha podido alocar memoria para el mensaje a enviarse");
					log_info(logger, "Conexión mediante socket %d finalizada", entrenadorAEjecutar->socket->descriptor);
					//TODO VERIFICAR SI ES CORRECTO BORRAR EL SOCKET
					eliminarSocket(entrenadorAEjecutar->socket);
					exit(-1);
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

				//VUELVO A ENCOLAR AL ENTRENADOR
				reencolarEntrenador(entrenadorAEjecutar);

				break;
			case OBJETIVOS_COMPLETADOS:
				log_info(logger, "Socket %d: he completado todos mis objetivos", entrenadorAEjecutar->socket->descriptor);

				//TODO LÓGICA DE LIBERACIÓN DE RECURSOS
				BorrarItem(items, entrenadorAEjecutar->id);

				free(mensajeRespuesta);
				log_info(logger, "El entrenador %s (%c) ha completado todos sus objetivos dentro del mapa", entrenadorAEjecutar->nombre, entrenadorAEjecutar->id);

				break;
			}

			//while(1);
/*			//INGRESO DEL ENTRENADOR
			entrenadorAEjecutar->ubicacion.x = 1;
			entrenadorAEjecutar->ubicacion.y = 1;
			CrearPersonaje(items, entrenadorAEjecutar->id, entrenadorAEjecutar->ubicacion.x, entrenadorAEjecutar->ubicacion.y); //Carga de entrenador
			t_list* objetivos = cargarObjetivos("C,O,D,E"); //Carga de Pokémons a buscar
			realizar_movimiento(items, *entrenadorAEjecutar, "CodeTogether");

			//COMIENZA LA BÚSQUEDA POKÉMON!
			int cant = list_size(objetivos);
			while (cant > 0) {
				//Obtengo la ubicación de la Pokénest correspondiente a mi Pokémon
				char* pokemon = list_get(objetivos, 0);
				char pokemonID = *pokemon;
				t_ubicacion pokenest = buscarPokenest(items, pokemonID);

				//Movimientos hasta la Pokénest
				while (((entrenadorAEjecutar->ubicacion.x != pokenest.x) || (entrenadorAEjecutar->ubicacion.y != pokenest.y))) { //&& pokenest.cantidad > 0) {
					entrenadorAEjecutar->ubicacion = calcularMovimiento(entrenadorAEjecutar->ubicacion, pokenest);
					realizar_movimiento(items, *entrenadorAEjecutar, "CodeTogether");

					//Si llego a la Pokénest, capturo un Pokémon
					if ((entrenadorAEjecutar->ubicacion.x == pokenest.x) && (entrenadorAEjecutar->ubicacion.y == pokenest.y))
					{
						restarRecurso(items, pokemonID);  //Resto un Pokémon de la Pokénest
						BorrarItem(objetivos, pokemonID); //Quito mi objetivo
						realizar_movimiento(items, *entrenadorAEjecutar, "CodeTogether");
					}
				}

				cant = list_size(objetivos);
			}*/

			//activo = 0;
		}
	}

	nivel_gui_terminar();
	// TODO Cerrar la conexión del servidor
	queue_destroy(colaBloqueados);
	queue_destroy(colaReady);
	list_destroy(entrenadores);
	pthread_mutex_destroy(&mutex);
	log_destroy(logger);
	return EXIT_SUCCESS;
}

//FUNCIONES PLANIFICADOR
void encolarEntrenador(t_entrenador* entrenador) {
	//SE CREA EL PERSONAJE PARA LA INTERFAZ GRÁFICA
	CrearPersonaje(items, entrenador->id, entrenador->ubicacion.x, entrenador->ubicacion.y);

	log_info(logger, "Se grafica al entrenador %s (%c) en el mapa", entrenador->nombre, entrenador->id);

	if(string_equals_ignore_case(configMapa.Algoritmo, "RR"))
	{
		//SI EL ALGORITMO ES ROUND ROBIN, LO AGREGO AL FINAL DE LA COLA DE READY
		insertarAlFinal(entrenador, colaReady);
	}
	else
	{
		//SI ES SRDF, INSERTO ORDENADO DE MENOR A MAYOR, DE ACUERDO A CUANTO LE FALTE EJECUTAR AL ENTRENADOR
		calcularFaltante(*entrenador);
		insertarOrdenado(entrenador, colaReady);
	}

	log_info(logger, "Se encola al entrenador");
}

void reencolarEntrenador(t_entrenador* entrenador) {
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

	log_info(logger, "Se encola al entrenador");
}

void calcularFaltante(t_entrenador entrenador)
{
/*	if(!list_is_empty(entrenador.objetivos))
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
	}*/
}

//FUNCIONES PARA COLAS PLANIFICADOR
void insertarOrdenado(t_entrenador* entrenador, t_queue* lista)
{
	/*//SEMAFORO PARA SINCRONIZAR LAS COLAS
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);*/

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

		bool _auxComparador(t_entrenador *entrenador1, t_entrenador *entrenador2)
		{
			return entrenador->faltaEjecutar < entrenador2->faltaEjecutar;
		}

		pthread_mutex_lock(&mutex);
		list_sort(lista->elements, (void*)_auxComparador);
		pthread_mutex_unlock(&mutex);
	}

	//pthread_mutex_destroy(&mutex);
}

void insertarAlFinal(t_entrenador* entrenador, t_queue* lista)
{
	pthread_mutex_lock(&mutex);
	queue_push(lista, entrenador);
	pthread_mutex_unlock(&mutex);

	pthread_mutex_destroy(&mutex);
}

void realizar_movimiento(t_list* items, t_entrenador personaje, char* mapa) {
	MoverPersonaje(items, personaje.id, personaje.ubicacion.x, personaje.ubicacion.y);
	nivel_gui_dibujar(items, mapa);
	usleep(configMapa.Retardo);
}

//FUNCIÓN DE ENTRENADOR
char ejeAnterior = 'x'; //VAR de entrenador
t_ubicacion calcularMovimiento(t_ubicacion posActual, t_ubicacion posFinal) {
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

ITEM_NIVEL* find_by_id(t_list* lista, char idBuscado) {
	int _is_the_one(void* p) {
		ITEM_NIVEL* castP = p;
		ITEM_NIVEL cajaComparator = *castP;
		return cajaComparator.id == idBuscado;
	}

	return list_find(lista, (void*) _is_the_one);
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
	t_mapa_pokenest pokenestLeida;
	t_list* newlist = list_create();

	 int dir_count = 0;
	    struct dirent* dent;
	    DIR* srcdir = opendir("PokeNests");
	    if (srcdir == NULL)
	    {
	        perror("opendir");
	    }
	    while((dent = readdir(srcdir)) != NULL)
	    {
	        struct stat st;
	        if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
	            continue;
	        if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0)
	        {
	            perror(dent->d_name);
	            continue;
	        }
	        if (S_ISDIR(st.st_mode)){
	        	dir_count++;
	        	char *str = string_new();
	        	string_append(&str, "PokeNests/");
	        	string_append(&str, dent->d_name);
	        	string_append(&str, "/metadata");
	        	pokenestLeida = leerPokenest(str);
	        	CrearCaja(newlist, pokenestLeida.id[0], pokenestLeida.ubicacion.x, pokenestLeida.ubicacion.y, 10); //Todos con 10
	        	log_info(logger, "Se cargo la pokenest: %c", pokenestLeida.id[0]);
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
		structPokenest.id = strdup(config_get_string_value(config, "Identificador"));
		structPokenest.tipo = strdup(config_get_string_value(config, "Tipo"));
		char* posXY = strdup(config_get_string_value(config, "Posicion"));
		char** array = string_split(posXY, ";");
		structPokenest.ubicacion.x = atoi(array[0]);
		structPokenest.ubicacion.y = atoi(array[1]);
		config_destroy(config);
	}
	else
	{
		log_error(logger, "La pokenest tiene un formato inválido");
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
			exit(error);
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

		free(paquete.paqueteSerializado);

		//Se agrega al entrenador a la lista de entrenadores conectados
		list_add(entrenadores, entrenador);

		//SE PLANIFICA AL NUEVO ENTRENADOR
		t_entrenador* entrenadorPlanificado;

		entrenadorPlanificado = malloc(sizeof(t_entrenador));
		*entrenadorPlanificado = *entrenador;

		encolarEntrenador(entrenadorPlanificado);

		log_info(logger, "Se aceptó una conexión. Socket° %d.\n", entrenador->socket->descriptor);
		log_info(logger, "Se planificó al entrenador %s (%c)", entrenadorPlanificado->nombre, entrenadorPlanificado->id);
	}
}
