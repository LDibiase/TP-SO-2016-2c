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
#include "PokeDexCliente.h"
#include <stddef.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
//#include "protocoloPokedexClienteServidor.h"



#define LOG_FILE_PATH "PokeDexCliente.log"
#define TAMANIO_MAXIMO_MENSAJE 50

/* Variables */
t_log* logger;
socket_t* pokedex;

static int fuse_getattr(const char *path, struct stat *stbuf) {

	paquete_t paqueteLectura;
	mensaje1_t mensajeQuieroGetAttr;

	mensajeQuieroGetAttr.tipoMensaje = GETATTR;
	mensajeQuieroGetAttr.path = path;
	mensajeQuieroGetAttr.tamanioPath = strlen(mensajeQuieroGetAttr.path) + 1;

	crearPaquete((void*) &mensajeQuieroGetAttr, &paqueteLectura);
	if(paqueteLectura.tamanioPaquete == 0) {
		pokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, pokedex->error);
		log_info(logger, "Conexión mediante socket %d finalizada", pokedex->descriptor);
		exit(EXIT_FAILURE);
	}

	enviarMensaje(pokedex, paqueteLectura);

	// Recibir mensaje RESPUESTA
	mensaje3_t mensajeGETATTR_RESPONSE;
	mensajeGETATTR_RESPONSE.tipoMensaje = GETATTR_RESPONSE;

	recibirMensaje(pokedex, &mensajeGETATTR_RESPONSE);
	if(pokedex->error != NULL)
		{
		log_info(logger, pokedex->error);
		eliminarSocket(pokedex);
	}
	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje
	//log_info(logger, "TIPO ARCHIVO %d  TAMAÑO ARCHIVO %d", mensajeGETATTR_RESPONSE.tipoArchivo, mensajeGETATTR_RESPONSE.tamanioArchivo);

	memset(stbuf, 0, sizeof(struct stat));


	if (mensajeGETATTR_RESPONSE.tipoArchivo == 2) {
		stbuf->st_mode = S_IFDIR | 0777;
	    stbuf->st_nlink = 2;
	    return 0;
	}

	if (mensajeGETATTR_RESPONSE.tipoArchivo == 1)  {
	    stbuf->st_mode = S_IFREG | 0777;
	    stbuf->st_nlink = 1;
	    stbuf->st_size = mensajeGETATTR_RESPONSE.tamanioArchivo;
	    return 0;
	}

	return -ENOENT;
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	(void) offset;
	(void) fi;

	// Enviar mensaje READDIR
	paquete_t paqueteLectura;
	mensaje1_t mensajeQuieroLeer;

	mensajeQuieroLeer.tipoMensaje = READDIR;
	mensajeQuieroLeer.path = path;
	mensajeQuieroLeer.tamanioPath = strlen(mensajeQuieroLeer.path) + 1;

	crearPaquete((void*) &mensajeQuieroLeer, &paqueteLectura);
	if(paqueteLectura.tamanioPaquete == 0) {
		pokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, pokedex->error);
		log_info(logger, "Conexión mediante socket %d finalizada", pokedex->descriptor);
		exit(EXIT_FAILURE);
	}

	enviarMensaje(pokedex, paqueteLectura);

	// Recibir mensaje RESPUESTA
	mensaje2_t mensajeREADDIR_RESPONSE;
	mensajeREADDIR_RESPONSE.tipoMensaje = READDIR_RESPONSE;

	recibirMensaje(pokedex, &mensajeREADDIR_RESPONSE);
	if(pokedex->error != NULL)
		{
		log_info(logger, pokedex->error);
		eliminarSocket(pokedex);
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	char** array = string_split(mensajeREADDIR_RESPONSE.mensaje, "/");
	int i = 0;
	while (array[i]) {
		char* fname = array[i];
		filler(buf, fname, NULL, 0);
		i++;
	}

	//filler(buf, DEFAULT_FILE_NAME, NULL, 0);

	return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi) {
	//if (strcmp(path, DEFAULT_FILE_PATH) != 0)
	//	return -ENOENT;

	//TODO: Verificar que el archivo exista
	return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Enviar mensaje READ
	paquete_t paqueteLectura;
	mensaje4_t mensajeQuieroREAD;

	mensajeQuieroREAD.tipoMensaje = READ;
	mensajeQuieroREAD.path = path;
	mensajeQuieroREAD.tamanioPath = strlen(mensajeQuieroREAD.path) + 1;
	mensajeQuieroREAD.tamanioBuffer = size;
	mensajeQuieroREAD.offset = offset;



	crearPaquete((void*) &mensajeQuieroREAD, &paqueteLectura);
	log_info(logger, "MENSAJE READ PATH: %s BYTES: %d OFFSET: %d", mensajeQuieroREAD.path, mensajeQuieroREAD.tamanioBuffer, mensajeQuieroREAD.offset);
	if(paqueteLectura.tamanioPaquete == 0) {
		pokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, pokedex->error);
		log_info(logger, "Conexión mediante socket %d finalizada", pokedex->descriptor);
		exit(EXIT_FAILURE);
	}

	enviarMensaje(pokedex, paqueteLectura);

	// Recibir mensaje RESPUESTA
		mensaje5_t mensajeREAD_RESPONSE;
		mensajeREAD_RESPONSE.tipoMensaje = READ_RESPONSE;

		recibirMensaje(pokedex, &mensajeREAD_RESPONSE);
		log_info(logger, "MENSAJE READ_RESPONSE TAMAÑO BUFFER %d , BUFFER: %c", mensajeREAD_RESPONSE.tamanioBuffer, mensajeREAD_RESPONSE.buffer);

		//TODO: verificar si el archivo existe
			//size = mensajeREAD_RESPONSE.buffer;
		    memcpy(buf, mensajeREAD_RESPONSE.buffer, mensajeREAD_RESPONSE.tamanioBuffer);
		    return mensajeREAD_RESPONSE.tamanioBuffer;

		  //return -ENOENT;
}

static int fuse_mkdir(const char *path, mode_t mode)
{
    int res;

    // Enviar mensaje MKDIR
   paquete_t paqueteCrearDirectorio;
   mensaje6_t mensajeQuieroMKDIR;

   mensajeQuieroMKDIR.tipoMensaje = MKDIR;
   mensajeQuieroMKDIR.path = path;
   mensajeQuieroMKDIR.tamanioPath = strlen(mensajeQuieroMKDIR.path) + 1;
   mensajeQuieroMKDIR.modo = mode;


	crearPaquete((void*) &mensajeQuieroMKDIR, &paqueteCrearDirectorio);
	log_info(logger, "MENSAJE MKDIR PATH: %s MODO: %d", mensajeQuieroMKDIR.path, mensajeQuieroMKDIR.modo);
	if(paqueteCrearDirectorio.tamanioPaquete == 0) {
		pokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, pokedex->error);
		log_info(logger, "Conexión mediante socket %d finalizada", pokedex->descriptor);
		exit(EXIT_FAILURE);
	}

	enviarMensaje(pokedex, paqueteCrearDirectorio);

	// Recibir mensaje RESPUESTA
	mensaje7_t mensajeMKDIR_RESPONSE;
	mensajeMKDIR_RESPONSE.tipoMensaje = MKDIR_RESPONSE;

	recibirMensaje(pokedex, &mensajeMKDIR_RESPONSE);

    res = mensajeMKDIR_RESPONSE.res;
    if(res == -1)
        return -errno;

    return 0;
}


static int fuse_rmdir(const char *path)
{
	  int res;

	    // Enviar mensaje MKDIR
	   paquete_t paqueteCrearDirectorio;
	   mensaje1_t mensajeQuieroRMDIR;

	   mensajeQuieroRMDIR.tipoMensaje = RMDIR;
	   mensajeQuieroRMDIR.path = path;
	   mensajeQuieroRMDIR.tamanioPath = strlen(mensajeQuieroRMDIR.path) + 1;


		crearPaquete((void*) &mensajeQuieroRMDIR, &paqueteCrearDirectorio);
		log_info(logger, "MENSAJE RMDIR PATH: %s", mensajeQuieroRMDIR.path);
		if(paqueteCrearDirectorio.tamanioPaquete == 0) {
			pokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, pokedex->error);
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex->descriptor);
			exit(EXIT_FAILURE);
		}

		enviarMensaje(pokedex, paqueteCrearDirectorio);

		// Recibir mensaje RESPUESTA
		mensaje7_t mensajeRMDIR_RESPONSE;
		mensajeRMDIR_RESPONSE.tipoMensaje = RMDIR_RESPONSE;

		recibirMensaje(pokedex, &mensajeRMDIR_RESPONSE);

	    res = mensajeRMDIR_RESPONSE.res;
	    if(res == -1)
	        return -errno;

	    return 0;
}

static int fuse_truncate(const char *path, off_t size)
{
    return 0;
}

static int fuse_unlink(const char *path)
{
	int res;

	// Enviar mensaje UNLINK
	paquete_t paqueteBorrarArchivo;
	mensaje1_t mensajeQuieroUNLINK;

	mensajeQuieroUNLINK.tipoMensaje = UNLINK;
	mensajeQuieroUNLINK.path = path;
	mensajeQuieroUNLINK.tamanioPath = strlen(mensajeQuieroUNLINK.path) + 1;


	crearPaquete((void*) &mensajeQuieroUNLINK, &paqueteBorrarArchivo);
	log_info(logger, "MENSAJE UNLINK PATH: %s", mensajeQuieroUNLINK.path);
	if(paqueteBorrarArchivo.tamanioPaquete == 0) {
		pokedex->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
		log_info(logger, pokedex->error);
		log_info(logger, "Conexión mediante socket %d finalizada", pokedex->descriptor);
		exit(EXIT_FAILURE);
	}

	enviarMensaje(pokedex, paqueteBorrarArchivo);

	// Recibir mensaje RESPUESTA
	mensaje7_t mensajeUNLINK_RESPONSE;
	mensajeUNLINK_RESPONSE.tipoMensaje = UNLINK_RESPONSE;

	recibirMensaje(pokedex, &mensajeUNLINK_RESPONSE);

	res = mensajeUNLINK_RESPONSE.res;
	if(res == -1)
		return -errno;

	return 0;
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
		.mkdir = fuse_mkdir,
		.rmdir = fuse_rmdir,
		.truncate = fuse_truncate,
		.unlink = fuse_unlink,
		//.write = fuse_write,
		//.create = fuse_create,

		//NO SON NECESARIAS
		//.flush = fuse_flush,
		//.release = fuse_release,
};

int main(int argc, char *argv[]) {

	/* Creación del log */
	logger = log_create(LOG_FILE_PATH, "POKEDEX_CLIENTE", true, LOG_LEVEL_INFO);
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

	pokedex = conectarAPokedexServidor("127.0.0.1", "8080");

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar todo
	// en varios threads
	fuse_main(args.argc, args.argv, &fuse_oper, NULL);

	while(1) {

	}

	eliminarSocket(pokedex);
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

	// Enviar mensaje CONEXION_POKEDEX_SERVIDOR
		paquete_t paquete;
		mensajeDePokedex mensajePokedex;

		mensajePokedex.tipoMensaje = CONEXION_POKEDEX_CLIENTE;
		mensajePokedex.ruta = DEFAULT_FILE_PATH;
		mensajePokedex.nombre= "Test";

		crearPaquete((void*) &mensajePokedex, &paquete);

		if(paquete.tamanioPaquete == 0) {
			pokedex_servidor->error = strdup("No se ha podido alocar memoria para el mensaje a enviarse");
			log_info(logger, pokedex_servidor->error);
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex_servidor->descriptor);
			return pokedex_servidor;
		}

		enviarMensaje(pokedex_servidor, paquete);

		if(pokedex_servidor->error != NULL) {
			log_info(logger, pokedex_servidor->error);
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex_servidor->descriptor);
			return pokedex_servidor;
		}

		free(paquete.paqueteSerializado);

		// Recibir mensaje ACEPTA_CONEXION
		mensaje_t mensajeAceptaConexion;

		mensajeAceptaConexion.tipoMensaje = ACEPTA_CONEXION;
		recibirMensaje(pokedex_servidor, &mensajeAceptaConexion);

		if(pokedex_servidor->error != NULL)
		{
			log_info(logger, pokedex_servidor->error);
			log_info(logger, "Conexión mediante socket %d finalizada", pokedex_servidor->descriptor);
			return pokedex_servidor;
		}

		switch(mensajeAceptaConexion.tipoMensaje) {
		case RECHAZA_CONEXION:
			log_info(logger, "Conexion a pokedex servidor rechazada");
			break;
		case ACEPTA_CONEXION:
			log_info(logger, "Conexion a pokedex servidor aceptada");
			break;
		}

		return pokedex_servidor;
}
