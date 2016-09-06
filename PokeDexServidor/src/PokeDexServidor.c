/*
 ============================================================================
 Name        : PokeDexServidor.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Servidor PokéDex
 ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "PokeDexServidor.h"
#include "osada.h"

int main(void) {
	FILE *fileFS;
	osada_header cabeceraFS;

	puts("Proceso Servidor PokéDex\n"); /* prints Proceso Servidor PokéDex */

	// Abre el archivo de File System
	if ((fileFS=fopen("/home/utnso/workspace/tp-2016-2c-CodeTogether/Osada.bin","r"))==NULL)
	{
		puts("Error al abrir el archivo de FS.fs\n");
		return EXIT_FAILURE;
	}

	// Lee la cabecera del archivo
	fread(&cabeceraFS, sizeof(cabeceraFS), 1 , fileFS);

	puts(cabeceraFS.magic_number);
	printf("%d\n", cabeceraFS.version);
	printf("%d\n", cabeceraFS.fs_blocks);
	printf("%d\n", cabeceraFS.bitmap_blocks);
	printf("%d\n", cabeceraFS.allocations_table_offset);
	printf("%d\n", cabeceraFS.data_blocks);
	puts(cabeceraFS.padding);

	fclose(fileFS);

	// Valida que sea un FS Odada
	if (strncmp(cabeceraFS.magic_number, "OsadaFS", 7) == 0)
		puts("Es un FS Osada\n");
	else
		puts("NO es un FS Osada\n");

	return EXIT_SUCCESS;
}
