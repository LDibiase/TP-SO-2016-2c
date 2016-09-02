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

int main(void) {
	puts("Proceso Mapa"); /* prints Proceso Mapa */
    t_list* items = list_create();

    //Inicializacion
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

	//Getch, cuando termina de recorrer se puede salir con Q
	while ( 1 ) {

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
