
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#ifndef NARGS
#define NARGS 4
#endif

/*
 * Ejecuta 'comando' pasándole los 'cantidad' argumentos de 'argumentos'
 * (cantidad <= NARGS), esperando su finalización antes de retornar.
 */
static void
ejecutar_lote(char *comando, char *argumentos[], int cantidad)
{
	char *argv_hijo[NARGS + 2];
	pid_t pid_hijo;
	int estado;

	pid_hijo = fork();
	if (pid_hijo == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid_hijo == 0) {
		argv_hijo[0] = comando;
		for (int i = 0; i < cantidad; i++)
			argv_hijo[i + 1] = argumentos[i];
		argv_hijo[cantidad + 1] = NULL;

		execvp(comando, argv_hijo);

		/* Solo se llega aquí si execvp falló. */
		perror("execvp");
		_exit(EXIT_FAILURE);
	}

	if (waitpid(pid_hijo, &estado, 0) == -1) {
		perror("waitpid");
		exit(EXIT_FAILURE);
	}
}

/* Libera las 'cantidad' líneas guardadas en 'argumentos'. */
static void
liberar_lote(char *argumentos[], int cantidad)
{
	for (int i = 0; i < cantidad; i++)
		free(argumentos[i]);
}

/* Valida los argumentos del programa y devuelve el comando a ejecutar. */
static char *
obtener_comando(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Uso: %s <comando>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	return argv[1];
}

/*
 * Lee la entrada estándar línea a línea, agrupando de a lo sumo NARGS
 * argumentos por lote, y ejecuta 'comando' con cada lote completo (y con
 * el resto final, si queda alguno).
 */
static void
procesar_entrada(char *comando)
{
	char *argumentos[NARGS];
	int cantidad = 0;
	char *linea = NULL;
	size_t capacidad = 0;
	ssize_t longitud;

	while ((longitud = getline(&linea, &capacidad, stdin)) != -1) {
		if (longitud > 0 && linea[longitud - 1] == '\n')
			linea[longitud - 1] = '\0';

		argumentos[cantidad] = strdup(linea);
		if (argumentos[cantidad] == NULL) {
			perror("strdup");
			free(linea);
			liberar_lote(argumentos, cantidad);
			exit(EXIT_FAILURE);
		}
		cantidad++;

		if (cantidad == NARGS) {
			ejecutar_lote(comando, argumentos, cantidad);
			liberar_lote(argumentos, cantidad);
			cantidad = 0;
		}
	}

	free(linea);

	if (cantidad > 0) {
		ejecutar_lote(comando, argumentos, cantidad);
		liberar_lote(argumentos, cantidad);
	}
}

int
main(int argc, char *argv[])
{
	char *comando = obtener_comando(argc, argv);

	procesar_entrada(comando);

	return EXIT_SUCCESS;
}