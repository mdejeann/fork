/*30 de Agosto Matias Dejean
 * primes.c - Criba de Eratóstenes concurrente (Sieve of Eratosthenes)
 *
 * Cada primo encontrado se modela como un proceso ("filtro"). Cada filtro:
 *   1) Lee el primer número que le llega por su pipe izquierdo: ese número
 *      es primo, se imprime y define el filtro (primo).
 *   2) Crea su "hermano" derecho (fork + pipe) para reenviarle los números
 *      que no sean múltiplos de ese primo.
 *   3) Sigue leyendo del pipe izquierdo: por cada número leído, si no es
 *      múltiplo del primo, lo escribe en el pipe derecho.
 *   4) Al cerrarse el pipe izquierdo (EOF), cierra el pipe derecho y espera
 *      a su hijo (wait) antes de terminar.
 *
 * El primer proceso es distinto: no tiene pipe izquierdo, simplemente
 * genera la secuencia 2..n y la escribe en el pipe hacia el primer filtro.
 *
 * La lógica de filtros está modelada recursivamente: la función criba()
 * crea a su hijo y se llama a sí misma (dentro del proceso hijo) para
 * procesar el siguiente filtro de la cadena.
 *
 * Los números se transmiten como enteros binarios a través de los pipes,
 * en lugar de texto, ya que no se especifica un formato de comunicación
 * entre procesos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/* Lee un entero del pipe. Devuelve 1 si se leyó un valor, 0 si es EOF. */
static int
leer_entero(int fd, int *valor)
{
	ssize_t leidos = read(fd, valor, sizeof(int));

	if (leidos == 0)
		return 0;

	if (leidos != (ssize_t) sizeof(int)) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	return 1;
}

/* Escribe un entero completo en el pipe. */
static void
escribir_entero(int fd, int valor)
{
	ssize_t escritos = write(fd, &valor, sizeof(int));

	if (escritos != (ssize_t) sizeof(int)) {
		perror("write");
		exit(EXIT_FAILURE);
	}
}

static void
cerrar_fd(int fd)
{
	if (close(fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}
}

static void
esperar_hijo(pid_t pid)
{
	int estado;

	if (waitpid(pid, &estado, 0) == -1) {
		perror("waitpid");
		exit(EXIT_FAILURE);
	}
}

static void
crear_tuberia(int fds[2])
{
	if (pipe(fds) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}
}

/*
 * Filtro recursivo: procesa los números que llegan por 'fd_izquierdo'.
 * El primer valor leído es un primo; el resto de los valores se filtran
 * y se propagan (si corresponde) al filtro derecho, creado recursivamente.
 */
static void
criba(int fd_izquierdo)
{
	int primo;
	int numero;
	int fd_derecho[2];
	pid_t pid_hijo;

	if (!leer_entero(fd_izquierdo, &primo)) {
		/* No llegó ningún valor: no hay más primos en esta rama. */
		cerrar_fd(fd_izquierdo);
		exit(EXIT_SUCCESS);
	}

	printf("primo %d\n", primo);

	crear_tuberia(fd_derecho);

	pid_hijo = fork();
	if (pid_hijo == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid_hijo == 0) {
		/* Proceso hijo: es el siguiente filtro de la cadena. */
		cerrar_fd(fd_derecho[1]);
		cerrar_fd(fd_izquierdo);
		criba(fd_derecho[0]);
		/* criba() nunca retorna: termina con exit(). */
	}

	/* Proceso actual (filtro de 'primo'): reenvía los no-múltiplos. */
	cerrar_fd(fd_derecho[0]);

	while (leer_entero(fd_izquierdo, &numero)) {
		if (numero % primo != 0)
			escribir_entero(fd_derecho[1], numero);
	}

	cerrar_fd(fd_izquierdo);
	cerrar_fd(fd_derecho[1]);

	esperar_hijo(pid_hijo);

	exit(EXIT_SUCCESS);
}

/* Valida los argumentos del programa y devuelve n, o termina con error. */
static int
parsear_n(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Uso: %s <n>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int n = atoi(argv[1]);
	if (n < 2) {
		fprintf(stderr, "n debe ser mayor o igual a 2\n");
		exit(EXIT_FAILURE);
	}

	return n;
}

/*
 * Arma la cadena de filtros: crea el proceso inicial (primer filtro) y,
 * como proceso actual, genera y escribe la secuencia 2..n hacia él.
 */
static void
generar_primos(int n)
{
	int fd[2];
	pid_t pid_hijo;

	crear_tuberia(fd);

	pid_hijo = fork();
	if (pid_hijo == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid_hijo == 0) {
		/* Proceso hijo: primer filtro de la cadena. */
		cerrar_fd(fd[1]);
		criba(fd[0]);
		/* No alcanzado. */
	}

	cerrar_fd(fd[0]);

	for (int numero = 2; numero <= n; numero++)
		escribir_entero(fd[1], numero);

	cerrar_fd(fd[1]);

	esperar_hijo(pid_hijo);
}

int
main(int argc, char *argv[])
{
	int n = parsear_n(argc, argv);

	/* Evita duplicación de salida bufferizada al hacer fork(). */
	setvbuf(stdout, NULL, _IONBF, 0);

	generar_primos(n);

	return EXIT_SUCCESS;
}