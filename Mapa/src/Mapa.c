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

#define MYPORT 3490 // Puerto al que conectarán los usuarios - "telnet localhost 3490" para empezar a jugar
#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola

void sigchld_handler(int s) {
	while (wait(NULL) > 0)
		;
}

void realizar_movimiento(t_list* items, int x, int y, char personaje,
		char * mapa) {
	MoverPersonaje(items, personaje, x, y);
	nivel_gui_dibujar(items, mapa);
	usleep(200000);
	//usleep(100000); Para pasarlo a nafta
}

ITEM_NIVEL *find_by_id(t_list* lista, char idBuscado) {
	int _is_the_one(void* p) {
		ITEM_NIVEL* castP = p;
		ITEM_NIVEL cajaComparator = *castP;
		return cajaComparator.id == idBuscado;
	}
	return list_find(lista, (void*) _is_the_one);
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
	CrearCaja(newlist, 'C', 20, 2, 5);
	CrearCaja(newlist, 'O', 50, 10, 5);
	CrearCaja(newlist, 'D', 10, 4, 5);
	CrearCaja(newlist, 'E', 70, 15, 5);
	return newlist;
}

int main(void) {

	//START SOCKET
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
	int x = 1;
	int y = 1;

	t_list* items = cargarPokenest(); //Carga de Pokenest del mapa
	t_list* objetivos = cargarObjetivos(); //Carga de Pokemons a buscar por el personaje

	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&rows, &cols);
	CrearPersonaje(items, '$', x, y); //Carga de personaje TODO: Hacer un TAD
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

		//COMIENZA LA BUSQUEDA POKEMON!
		int cant = list_size(objetivos);

		while (cant > 0) {

			int movioX = 1; //Comienza moviendo en eje y
			int movioY = 0;

			//Obtengo la pokenest correspondiente a mi pokemon
			char* pokemon = list_get(objetivos, 0);
			ITEM_NIVEL pokenest = *find_by_id(items, *pokemon);
			int x1 = pokenest.posx;
			int y1 = pokenest.posy;
			char idCaja = pokenest.id;

			//Movimientos hasta la pokenest TODO: Abstraer
			while ((x != x1) || (y != y1)) {
				movioY = 0;
				if ((movioX == 1) || (x == x1)) {
					if (y > y1) {
						y--;
						movioY = 1;
						realizar_movimiento(items, x, y, '$', "CodeTogheter");
					}
					if (y < y1) {
						y++;
						movioY = 1;
						realizar_movimiento(items, x, y, '$', "CodeTogheter");
					}
				}

				movioX = 0;
				if ((movioY == 1) || (y == y1)) {
					if (x > x1) {
						x--;
						movioX = 1;
						realizar_movimiento(items, x, y, '$', "CodeTogheter");
					}
					if (x < x1) {
						x++;
						movioX = 1;
						realizar_movimiento(items, x, y, '$', "CodeTogheter");
					}
				}

				//Si llego a la pokenest le resto un pokemon
				if ((x == x1) && (y == y1)) {
					restarRecurso(items, idCaja);
					BorrarItem(objetivos, idCaja); //Quito mi objetivo
					realizar_movimiento(items, x, y, '$', "CodeTogheter");
				}
			}
			cant = list_size(objetivos);
		}
	}
	nivel_gui_terminar();
	return EXIT_SUCCESS;
}
