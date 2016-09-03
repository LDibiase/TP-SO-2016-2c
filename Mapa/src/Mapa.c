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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define MYPORT 3490 // Puerto al que conectarán los usuarios
#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola

void sigchld_handler(int s)	{
	while(wait(NULL) > 0);
}

int main(void) {

	//START SOCKET
	int sockfd, new_fd; // Escuchar sobre sock_fd, nuevas conexiones sobre new_fd
		struct sockaddr_in my_addr; // información sobre mi dirección
		struct sockaddr_in their_addr; // información sobre la dirección del cliente
		int sin_size;
		struct sigaction sa;
		int yes=1;

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			perror("socket");
			exit(1);
		}

		if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		my_addr.sin_family = AF_INET; // Ordenación de bytes de la máquina
		my_addr.sin_port = htons(MYPORT); // short, Ordenación de bytes de la red
		my_addr.sin_addr.s_addr = INADDR_ANY; // Rellenar con mi dirección IP
		memset(&(my_addr.sin_zero), '\0', 8); // Poner a cero el resto de la estructura

		if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))	== -1) {
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




    //Inicializacion
	t_list* items = list_create();
	int rows, cols;
	int x = 1;
	int y = 1;
	nivel_gui_inicializar();
    nivel_gui_get_area_nivel(&rows, &cols);
	CrearPersonaje(items, '$', x, y);

	//Carga de items
	CrearCaja(items, 'C', 20, 2, 1);
	CrearCaja(items, 'O', 50, 10, 1);
	CrearCaja(items, 'D', 10, 4, 1);
	CrearCaja(items, 'E', 70, 15, 1);

	nivel_gui_dibujar(items, "CodeTogheter");

	//MAIN LOOP, cuando termina de recorrer se puede salir con Q
	while ( 1 ) {

		sin_size = sizeof(struct sockaddr_in);

		if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr,&sin_size)) == -1) {
			perror("accept");
		continue;
		}

		printf("Conexion entrante de %s\n",	inet_ntoa(their_addr.sin_addr));

		if (!fork()) { // Este es el proceso hijo
			close(sockfd); // El hijo no necesita este descriptor
		if (send(new_fd, "GAME START!!\n", 14, 0) == -1)
			perror("send");
			close(new_fd);
			exit(0);
		}
		close(new_fd); // El proceso padre no lo necesita


		ITEM_NIVEL * pcaja;
		ITEM_NIVEL caja;


		//Recorre la lista de items
		int cant = list_size(items);
		while (cant > 1 ) {

			int movioX = 1; //Comienza moviendo en eje y
			int movioY = 0;

			//Obtengo la caja
			pcaja = list_get(items, 1);
			caja = *pcaja;
			int x1 = caja.posx;
			int y1 = caja.posy;
			char idCaja = caja.id;

			//Movimientos hasta la caja
			while ((x!=x1) || (y!=y1)) {
				movioY = 0;
				if ((movioX == 1) || (x==x1)) {
					if (y>y1) {
						y--;
						movioY = 1;
					}
					if (y<y1) {
						y++;
						movioY = 1;
					}
				}

				movioX = 0;
				if ((movioY == 1) || (y==y1))  {
					if (x>x1) {
						x--;
						movioX = 1;
					}
					if (x<x1) {
						x++;
						movioX = 1;
					}
				}

				usleep(100000);

				//Si llego la borro
				if ((x == x1) && (y == y1)) {
					BorrarItem(items, idCaja);
				}
				MoverPersonaje(items, '$', x, y);
				nivel_gui_dibujar(items, "CodeTogheter");
				cant = list_size(items);
			}

		}

		int key = getch();
		switch( key ) {
			case 'Q':
			case 'q':
				nivel_gui_terminar();
				exit(0);
			break;

		 }
	}

	MoverPersonaje(items, '$', x, y);
	nivel_gui_dibujar(items, "CodeTogheter");

	BorrarItem(items, '$');
	BorrarItem(items, 'C');
	BorrarItem(items, 'O');
	BorrarItem(items, 'D');
	BorrarItem(items, 'E');

	nivel_gui_terminar();
	return EXIT_SUCCESS;
}
