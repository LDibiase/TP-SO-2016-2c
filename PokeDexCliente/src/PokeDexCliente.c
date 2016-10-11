/*
 ============================================================================
 Name        : PokeDexCliente.c
 Author      : CodeTogether
 Version     : 1.0
 Description : Proceso Cliente del PokéDex
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
//#include <Utility_Library/socket.h>
#include "socket.h" // BORRAR
#include "PokeDexCliente.h"
#include <stddef.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define LOG_FILE_PATH "PokeDexCliente.log"

/* Variables */
t_log* logger;

static int fuse_getattr(const char *path, struct stat *stbuf) {

	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje

	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0777;
	    stbuf->st_nlink = 2;
	    return 0;
	}

	if (strcmp(path, DEFAULT_FILE_PATH) == 0) {
	    stbuf->st_mode = S_IFREG | 0777;
	    stbuf->st_nlink = 1;
	    stbuf->st_size = strlen(DEFAULT_FILE_CONTENT);
	    return 0;
	}

	return -ENOENT;
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	filler(buf, DEFAULT_FILE_NAME, NULL, 0);

	return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi) {
	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
		return -ENOENT;

	return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	if (strcmp(path, DEFAULT_FILE_PATH) == 0) {
	    size_t len = strlen(DEFAULT_FILE_CONTENT);
	    if (offset >= len) {
	      return 0;
	    }

	    if (offset + size > len) {
	    	memcpy(buf, DEFAULT_FILE_CONTENT + offset, len - offset);
	    	return len - offset;
	    }

	  		memcpy(buf, DEFAULT_FILE_CONTENT + offset, size);
	  		return size;
	 }

	 return -ENOENT;
}

static struct fuse_opt fuse_options[] = {
		// Este es un parametro definido por nosotros
		CUSTOM_FUSE_OPT_KEY("--welcome-msg %s", welcome_msg, 0),

		// Estos son parametros por defecto que ya tiene FUSE
		//FUSE_OPT_KEY("-V", KEY_VERSION),
		//FUSE_OPT_KEY("--version", KEY_VERSION),
		//FUSE_OPT_KEY("-h", KEY_HELP),
		//FUSE_OPT_KEY("--help", KEY_HELP),
		FUSE_OPT_END,
};

static struct fuse_operations fuse_oper = {
		.getattr = fuse_getattr,
		.readdir = fuse_readdir,
		.open = fuse_open,
		.read = fuse_read,
};

int main(int argc, char *argv[]) {
	struct socket* serv_socket_s;

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

		// Limpio la estructura que va a contener los parametros
		memset(&runtime_options, 0, sizeof(struct t_runtime_options));

		// Esta funcion de FUSE lee los parametros recibidos y los intepreta
		if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
			/** error parsing options */
			perror("Invalid arguments!");
			return EXIT_FAILURE;
		}

		// Si se paso el parametro --welcome-msg
		// el campo welcome_msg deberia tener el
		// valor pasado
		if( runtime_options.welcome_msg != NULL ){
			printf("%s\n", runtime_options.welcome_msg);
		}

		// Esta es la funcion principal de FUSE, es la que se encarga
		// de realizar el montaje, comuniscarse con el kernel, delegar todo
		// en varios threads
		return fuse_main(args.argc, args.argv, &fuse_oper, NULL);

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "ENTRENADOR", true, LOG_LEVEL_INFO);

	serv_socket_s = conectarAServidor("127.0.0.1", "8080");
	//if(serv_socket_s->descriptor == 0)
	//{
		//log_info(logger, "Conexión fallida");
		//log_info(logger, serv_socket_s->error);
		//return EXIT_FAILURE;
	//}

	log_info(logger, "Conexión exitosa");

	while(1);

	eliminarSocket(serv_socket_s);
	log_destroy(logger);
	return EXIT_SUCCESS;
}

socket_t* conectarAPokedexServidor(char* ip, char* puerto) {
	socket_t* pokedex_servidor;

	pokedex_servidor = conectarAServidor(ip, puerto);
	if(pokedex_servidor->descriptor == 0)
	{
		log_info(logger, "Conexión fallida");
		log_info(logger, pokedex_servidor->error);
		return pokedex_servidor;
	}

	log_info(logger, "Conexión exitosa");
}
