/*
 ============================================================================
 Name        : Mapa.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Mapa
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <unistd.h>
#include <tad_items.h>
#include <curses.h>
#include "Mapa.h"
#include "nivel.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/log.h>

#define MYPORT 3490 // Puerto al que conectarán los entrenadores - "telnet localhost 3490" para empezar a jugar
#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola

/* Variables */
t_log* logger;
t_mapa_config configMapa;



void sigchld_handler(int s) {
	while (wait(NULL) > 0);
}

void realizar_movimiento(t_list* items, t_mapa_pj personaje, char * mapa) {
	MoverPersonaje(items, personaje.id, personaje.pos.x, personaje.pos.y);
	nivel_gui_dibujar(items, mapa);
	usleep(200000);
	//usleep(100000); //Para pasarlo a nafta
}

//FUNCION DE ENTRENADOR
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
	return ubicacion;
}


t_list* cargarObjetivos() {
	t_list* newlist = list_create();
	list_add(newlist, "C");
	list_add(newlist, "O");
	list_add(newlist, "D");
	list_add(newlist, "E");
	list_add(newlist, "O");
	list_add(newlist, "D");
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
		structConfig->Algoritmo = config_get_string_value(config, "algoritmo");
		structConfig->Quantum = config_get_int_value(config, "quantum");
		structConfig->Retardo = config_get_int_value(config, "retardo");
		structConfig->IP = config_get_string_value(config, "IP");
		structConfig->Puerto = config_get_string_value(config, "Puerto");

		log_info(logger, "El archivo de configuración se cargo correctamente");
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

int main(void) {

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "MAPA", true, LOG_LEVEL_INFO);

	/*Cargar Configuración*/
	log_info(logger, "Cargando archivo configuración");
	//int res = cargarConfiguracion(&configMapa);



	//INICIO SOCKET
	int sockfd, new_fd; // Escuchar sobre sock_fd, nuevas conexiones sobre new_fd
	struct sockaddr_in my_addr; // información sobre mi dirección
	struct sockaddr_in their_addr; // información sobre la dirección del cliente
	int sin_size;
	struct sigaction sa;
	int yes = 1;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	my_addr.sin_family = AF_INET; // Ordenación de bytes de la máquina
	my_addr.sin_port = htons(MYPORT); // short, Ordenación de bytes de la red
	my_addr.sin_addr.s_addr = INADDR_ANY; // Rellenar con mi dirección IP
	memset(&(my_addr.sin_zero), '\0', 8); // Poner a cero el resto de la estructura

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr))
			== -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // Eliminar procesos muertos
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	//FIN SOCKET


	//INICIALIZACION DEL MAPA
	int rows, cols;
	t_list* items = cargarPokenest(); //Carga de las Pokenest del mapa
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&rows, &cols);
	nivel_gui_dibujar(items, "CodeTogheter");


	//MAIN LOOP
	while (1) {
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}

		printf("Conexion entrante de %s\n", inet_ntoa(their_addr.sin_addr));

		if (!fork()) { // Este es el proceso hijo
			close(sockfd); // El hijo no necesita este descriptor
			if (send(new_fd, "GAME START!!\n", 14, 0) == -1)
				perror("send");
			close(new_fd);
			exit(0);
		}
		close(new_fd); // El proceso padre no lo necesita


		//INGRESO DEL ENTRENADOR
		t_mapa_pj personaje;
			personaje.id = '$';
			personaje.pos.x = 1;
			personaje.pos.y = 1;
		CrearPersonaje(items, personaje.id, personaje.pos.x, personaje.pos.y); //Carga de entrenador
		t_list* objetivos = cargarObjetivos(); //Carga de Pokemons a buscar
		realizar_movimiento(items, personaje, "CodeTogheter");


		//COMIENZA LA BUSQUEDA POKEMON!
		int cant = list_size(objetivos);
		while (cant > 0) {

			//Obtengo la ubicacion de la pokenest correspondiente a mi pokemon
			char* pokemon = list_get(objetivos, 0);
			char pokemonID = *pokemon;
			t_mapa_pos pokenest = buscarPokenest(items, pokemonID);

			//Movimientos hasta la pokenest
			while ((personaje.pos.x != pokenest.x) || (personaje.pos.y != pokenest.y)) {

				personaje.pos = calcularMovimiento(personaje.pos, pokenest);
				realizar_movimiento(items, personaje, "CodeTogheter");

				//Si llego a la pokenest capturo un pokemon
				if ((personaje.pos.x == pokenest.x) && (personaje.pos.y == pokenest.y)) {
					restarRecurso(items, pokemonID);  //Resto un pokemon de la pokenest
					BorrarItem(objetivos, pokemonID); //Quito mi objetivo
					realizar_movimiento(items, personaje, "CodeTogheter");
				}
			}
			cant = list_size(objetivos);
		}
	}
	nivel_gui_terminar();
	log_destroy(logger);
	return EXIT_SUCCESS;
}
