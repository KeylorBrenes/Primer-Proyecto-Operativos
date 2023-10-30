#include <stdio.h>   //Para usar printf
#include <stdlib.h>  //Para usar malloc y free
#include <pthread.h> //Para usar hilos
#include <stdbool.h> //Para usar bool
#include <time.h>    //Para usar time y clock
#include <float.h>   //Para usar DBL_MAX
#include <unistd.h>  // Para usar usleep
#include <limits.h>  //Para usar INT_MAX (es el valor máximo que puede tener un int)
#include <ncurses.h> //Para usar getch y otras funciones de ncurses (para manejar la entrada del teclado)

enum Algoritmo
{
    RR,
    SJF
};                                    // Enumeración para los algoritmos de planificación
enum Algoritmo algoritmo_actual = RR; // Algoritmo de planificación actual

// Definición de una carta
typedef struct
{
    int valor; // Valor numérico de la carta (2 a 14)
    char tipo; // Tipo de la carta (A para As, C para Corazones, B para Bastos, F para Flores)
} Carta;

// Definición de un jugador
typedef struct
{
    Carta *mazo;    // Mazo de cartas del jugador
    int num_cartas; // Número de cartas restantes en el mazo
    int puntos;     // Puntos acumulados por el jugador
} Jugador;

// Definición de un juego
typedef struct
{
    Jugador jugador1;
    Jugador jugador2;
    int jugadas; // Número de jugadas realizadas en este juego
    int id;      // Identificador único del juego
} Juego;

// Variables globales
Juego *juegos;  // Array de juegos
int num_juegos; // Número de juegos

pthread_mutex_t lock; // Bloqueo para sincronización de hilos

// Nodo para la lista de bloqueados
typedef struct NodoBloqueado
{
    Juego *juego;
    struct NodoBloqueado *siguiente;
} NodoBloqueado;

NodoBloqueado *lista_bloqueados = NULL;

// Definición de un BCP (Bloque de Control de Proceso)
typedef struct
{
    int num_iteraciones; // Jugadas hechas
    int avance_pila_codigo;
    double tiempo_espera_ES; // Tiempo de espera en E/S
    double promedio_tiempo_espera_ES;
    double promedio_tiempo_espera_listo;
} BCP;

// Definición de la tabla de procesos (para almacenar los BCP de todos los hilos)
typedef struct
{
    double promedio_num_iteraciones;     // Promedio de jugadas hechas entre todos los hilos
    double promedio_tiempo_espera_ES;    // Promedio de tiempo de espera en E/S entre todos los hilos
    double promedio_tiempo_espera_listo; // Promedio de tiempo de espera en listo entre todos los hilos
} TablaProcesos;

BCP *bcp; // Un puntero a un arreglo de BCP
TablaProcesos tabla_procesos;

// Definición de un nodo para la lista de terminados
typedef struct NodoTerminado
{
    Juego *juego;
    struct NodoTerminado *siguiente;
} NodoTerminado;
// Cabeza de la lista de terminados
NodoTerminado *cabeza_terminados = NULL;
// Función para agregar un juego a la lista de terminados
void agregar_a_terminados(Juego *juego)
{
    NodoTerminado *nuevo_nodo = (NodoTerminado *)malloc(sizeof(NodoTerminado)); // Asignar memoria para el nuevo nodo
    nuevo_nodo->juego = juego;                                                  // Establecer el juego del nuevo nodo
    nuevo_nodo->siguiente = NULL;                                               // Establecer el siguiente del nuevo nodo a NULL

    if (!cabeza_terminados)
    {
        cabeza_terminados = nuevo_nodo;
    }
    else
    {
        NodoTerminado *actual = cabeza_terminados;
        while (actual->siguiente)
        {
            actual = actual->siguiente;
        }
        actual->siguiente = nuevo_nodo;
    }
}
// Función para eliminar un juego de la lista de terminados
void agregar_a_bloqueados(Juego *juego)
{
    NodoBloqueado *nuevo_nodo = (NodoBloqueado *)malloc(sizeof(NodoBloqueado));
    nuevo_nodo->juego = juego;
    nuevo_nodo->siguiente = NULL;

    if (lista_bloqueados == NULL)
    {
        lista_bloqueados = nuevo_nodo;
    }
    else
    {
        NodoBloqueado *temp = lista_bloqueados;
        while (temp->siguiente != NULL)
        {
            temp = temp->siguiente;
        }
        temp->siguiente = nuevo_nodo;
    }
}

// Función para eliminar un juego de la lista de bloqueados
void eliminar_de_bloqueados(Juego *juego)
{
    NodoBloqueado *previo = NULL;
    NodoBloqueado *actual = lista_bloqueados;

    while (actual != NULL)
    {
        if (actual->juego == juego)
        {
            if (previo == NULL)
            {
                lista_bloqueados = actual->siguiente;
            }
            else
            {
                previo->siguiente = actual->siguiente;
            }

            free(actual);
            return;
        }

        previo = actual;
        actual = actual->siguiente;
    }
}

// Función para inicializar un mazo de cartas
void inicializar_mazo(Carta *mazo)
{
    char tipos[] = {'A', 'C', 'B', 'F'};
    int index = 0;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 2; j <= 14; j++)
        {
            mazo[index].valor = j;
            mazo[index].tipo = tipos[i];
            index++;
        }
    }
}

// Función para barajar el mazo
void barajar_mazo(Carta *mazo)
{
    for (int i = 0; i < 52; i++)
    {
        int r = i + rand() % (52 - i); // Valor aleatorio entre i y 51
        Carta temp = mazo[i];          // Intercambiar mazo[i] con mazo[r]
        mazo[i] = mazo[r];
        mazo[r] = temp;
    }
}

// Función para distribuir las cartas entre dos jugadores
void distribuir_cartas(Jugador *jugador1, Jugador *jugador2, Carta *mazo)
{
    for (int i = 0; i < 26; i++)
    {
        jugador1->mazo[i] = mazo[i * 2];     // Cartas en índices pares para el jugador 1
        jugador2->mazo[i] = mazo[i * 2 + 1]; // Cartas en índices impares para el jugador 2
    }
    jugador1->num_cartas = jugador2->num_cartas = 26; // Ambos jugadores tienen 26 cartas
    jugador1->puntos = jugador2->puntos = 0;
}
// Variable global para almacenar el número aleatorio de jugadas antes de escribir en el archivo
int jugadas_para_escribir;
// Función para inicializar el valor aleatorio de jugadas_para_escribir
void inicializar_jugadas_para_escribir()
{
    jugadas_para_escribir = rand() % 11 + 5; // Valor aleatorio entre 5 y 15
}
// Función para escribir en el archivo
void escribir_en_archivo(Juego *juego)
{
    int indice_bcp = juego->id - 1;                                                // Asumiendo que el id del juego es el índice en el array bcp
    struct timespec inicio, fin;                                                   // Variables para almacenar el tiempo de inicio y fin
    clock_gettime(CLOCK_MONOTONIC, &inicio);                                       // Iniciar el contador de tiempo de espera en listo
    char nombre_archivo[20];                                                       // Nombre del archivo
    snprintf(nombre_archivo, sizeof(nombre_archivo), "jugadas_%d.txt", juego->id); // Crear el nombre del archivo

    FILE *archivo = fopen(nombre_archivo, "a"); // Abrir el archivo en modo append
    if (archivo == NULL)                        // Si el archivo no se pudo abrir, imprimir un error y regresar
    {
        perror("Error al abrir el archivo"); // Imprimir el error
        return;
    }
    // Imprimir el número de jugadas y la puntuación de cada jugador
    fprintf(archivo, "Jugador 1: %d puntos, Jugador 2: %d puntos\n",
            juego->jugador1.puntos, juego->jugador2.puntos);

    fclose(archivo);                      // Cerrar el archivo
    clock_gettime(CLOCK_MONOTONIC, &fin); // Finalizar el contador de tiempo de espera en listo
    // Calcular el tiempo de espera en listo
    double tiempo_espera = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;
    // Agregar el tiempo de espera en listo al BCP
    bcp[indice_bcp].tiempo_espera_ES += tiempo_espera;
}
// Función para agregar un juego a la lista de listos para que jugar_turno lo ejecute
void agregar_a_listos(Juego *juego);
// Función para jugar un turno
void jugar_turno(Jugador *jugador1, Jugador *jugador2, Juego *juegoActual, int numero_juego, int numero_jugada, BCP *bcp_actual)
{
    // Asegurarse de que ambos jugadores tengan cartas
    if (jugador1->num_cartas > 0 && jugador2->num_cartas > 0)
    {
        // Tirar una carta de cada jugador
        Carta carta1 = jugador1->mazo[--jugador1->num_cartas];
        Carta carta2 = jugador2->mazo[--jugador2->num_cartas];

        // Imprimir las cartas que tiraron los jugadores
        printf("Juego#%d - Jugada #%d - Jugador 1: %d%c\n", numero_juego, numero_jugada, carta1.valor, carta1.tipo);
        printf("Juego#%d - Jugada #%d - Jugador 2: %d%c\n", numero_juego, numero_jugada, carta2.valor, carta2.tipo);

        // Determinar el ganador del turno
        if (carta1.valor > carta2.valor)
        {
            jugador1->puntos += 5;
            printf("Jugador 1 gana la jugada\n");
        }
        else if (carta1.valor < carta2.valor)
        {
            jugador2->puntos += 5;
            printf("Jugador 2 gana la jugada\n");
        }
        else
        {
            // En caso de empate, repetir el turno
            printf("Empate. Jugadores vuelven a tirar.\n");
            jugador1->num_cartas++; // Devolver las cartas al mazo
            jugador2->num_cartas++;
            while (carta1.valor == carta2.valor && jugador1->num_cartas > 0 && jugador2->num_cartas > 0)
            {
                // Re-tirar una carta de cada jugador
                carta1 = jugador1->mazo[--jugador1->num_cartas];
                carta2 = jugador2->mazo[--jugador2->num_cartas];
            }
        }

        // Imprimir la puntuación actual
        printf("Puntuación actual del juego #%d: Jugador 1: %d - Jugador 2: %d\n", numero_juego, jugador1->puntos, jugador2->puntos);
    }
    // Después de cada jugada, decrementa el valor de jugadas_para_escribir
    jugadas_para_escribir--;
    // Si jugadas_para_escribir es 0, escribe en el archivo y reinicia el valor
    if (jugadas_para_escribir == 0)
    {
        clock_t inicio_ES = clock();                    // Iniciar el contador de tiempo de espera en E/S
        escribir_en_archivo(&juegos[numero_juego - 1]); // Escribir en el archivo
        agregar_a_bloqueados(juegoActual);              // Agregar el juego a la lista de bloqueados
        usleep((rand() % 6 + 5) * 1000);                // Dormir entre 5 y 10 milisegundos
        eliminar_de_bloqueados(juegoActual);            // Eliminar el juego de la lista de bloqueados
        if (juegoActual->jugador1.num_cartas > 0 && juegoActual->jugador2.num_cartas > 0)
        {
            agregar_a_listos(juegoActual); // Si el juego no ha terminado, agregarlo de nuevo a la lista de listos
        }
        clock_t fin_ES = clock(); // Finalizar el contador de tiempo de espera en E/S
        inicializar_jugadas_para_escribir();
        double tiempo_ES = ((double)(fin_ES - inicio_ES)) / CLOCKS_PER_SEC * 1000; // Tiempo de espera en E/S en milisegundos
        // Agregar el tiempo de E/S al BCP
        bcp_actual->tiempo_espera_ES += tiempo_ES;
    }
}
// Nodo para la lista de listos
typedef struct Nodo
{
    Juego *juego;
    struct Nodo *siguiente;
} Nodo;

Nodo *lista_listos = NULL;
int num_juegos;
Juego *juegos;
// Función para agregar un juego a la lista de listos
void agregar_a_listos(Juego *juego);

// Función para ejecutar los juegos en la lista de listos
void *rutina_hilo(void *arg)
{
    Juego *juego = (Juego *)arg;    // Convertir el argumento a un puntero a Juego
    int indice_bcp = juego->id - 1; // Suponiendo que el id del juego es el índice en el array bcp

    // Simulando un quantum de Round Robin con un número aleatorio entre 2 y 10
    int quantum = rand() % 9 + 2;
    printf("Quantum: %d\n", quantum);

    for (int i = 0; i < quantum && juego->jugador1.num_cartas > 0 && juego->jugador2.num_cartas > 0; i++)
    {
        bcp[indice_bcp].num_iteraciones++;                                                                                        // Incrementar el número de iteraciones
        jugar_turno(&(juego->jugador1), &(juego->jugador2), juego, juego->id, bcp[indice_bcp].num_iteraciones, &bcp[indice_bcp]); // Jugar un turno
    }

    // Si el juego no ha terminado, reubicarlo al final de la cola de listos.
    if (juego->jugador1.num_cartas > 0 && juego->jugador2.num_cartas > 0)
    {
        agregar_a_listos(juego);
    }

    return NULL; // Terminar el hilo
}
//
void agregar_a_listos(Juego *juego)
{
    Nodo *nuevo_nodo = (Nodo *)malloc(sizeof(Nodo));
    nuevo_nodo->juego = juego;
    nuevo_nodo->siguiente = NULL; // inicializar siguiente a NULL

    if (lista_listos == NULL)
    {
        // Si la lista está vacía, establecer nuevo_nodo como la cabeza de la lista
        lista_listos = nuevo_nodo;
    }
    else
    {
        // Si la lista no está vacía, encontrar el último nodo en la lista
        Nodo *temp = lista_listos;
        while (temp->siguiente != NULL)
        {
            temp = temp->siguiente;
        }
        // Insertar nuevo_nodo al final de la lista
        temp->siguiente = nuevo_nodo;
    }
}
// Función para ejecutar los juegos en la lista de listos
void ejecutar_listos()
{
    while (lista_listos != NULL)
    {
        Nodo *nodo_actual = lista_listos;
        lista_listos = lista_listos->siguiente; // Retira el juego de la cabeza de la lista de listos.
        nodo_actual->siguiente = NULL;          // Asegúrate de que el nodo_actual no tenga un siguiente, ya que será reagregado a la lista si es necesario.

        rutina_hilo((void *)nodo_actual->juego); // Ejecuta el juego en el nodo actual.

        free(nodo_actual); // Libera el nodo actual, ya que agregar_a_listos creará un nuevo nodo si es necesario.
    }
}

float calcular_promedio(Juego *juego)
{
    return (juego->jugador1.num_cartas + juego->jugador1.puntos) / 2.0;
}
// Función para seleccionar el juego con el menor promedio de cartas
Juego *seleccionar_juego_fsj()
{
    Nodo *nodo_actual = lista_listos;
    Juego *juego_seleccionado = NULL;
    int min_cartas = INT_MAX;

    while (nodo_actual != NULL)
    {
        int total_cartas = nodo_actual->juego->jugador1.num_cartas + nodo_actual->juego->jugador2.num_cartas;
        if (total_cartas < min_cartas)
        {
            min_cartas = total_cartas;
            juego_seleccionado = nodo_actual->juego;
        }
        nodo_actual = nodo_actual->siguiente;
    }

    return juego_seleccionado;
}
// Hilo para ejecutar el algoritmo FSJ (First Shortest Job)
void *rutina_hilo_fsj(void *arg)
{
    Juego *juego = (Juego *)arg;
    int indice_bcp = juego->id - 1; // Suponiendo que el id del juego es el índice en el array bcp
    while (juego->jugador1.num_cartas > 0 && juego->jugador2.num_cartas > 0)
    {
        bcp[indice_bcp].num_iteraciones++;
        jugar_turno(&(juego->jugador1), &(juego->jugador2), juego, juego->id, bcp[indice_bcp].num_iteraciones + 1, &bcp[indice_bcp]);
    }

    return NULL;
}
// Función para eliminar un juego de la lista de listos
void eliminar_juego(Juego *juego)
{
    Nodo *previo = NULL;
    Nodo *actual = lista_listos;

    while (actual != NULL)
    {
        if (actual->juego == juego)
        {
            if (previo == NULL)
            {
                lista_listos = actual->siguiente; // Eliminar el primer nodo
            }
            else
            {
                previo->siguiente = actual->siguiente; // Eliminar un nodo intermedio
            }

            free(actual);
            return;
        }

        previo = actual;
        actual = actual->siguiente;
    }
}
// Metodo para ejecutar el algoritmo FSJ (First Shortest Job)
void ejecutar_fsj()
{
    while (lista_listos != NULL)
    {
        Juego *juego_seleccionado = seleccionar_juego_fsj();
        if (juego_seleccionado == NULL)
            break;
        int indice_bcp = juego_seleccionado->id - 1;

        while (juego_seleccionado->jugador1.num_cartas > 0 && juego_seleccionado->jugador2.num_cartas > 0)
        {
            bcp[indice_bcp].num_iteraciones++;
            jugar_turno(&(juego_seleccionado->jugador1), &(juego_seleccionado->jugador2), juego_seleccionado, juego_seleccionado->id, juego_seleccionado->jugadas + 1, &bcp[indice_bcp]);
        }

        eliminar_juego(juego_seleccionado);
    }
}
// Función para imprimir las cartas de un jugador
void imprimir_cartas(Jugador *jugador)
{
    printf("Cartas del Jugador: ");
    for (int i = 0; i < 26; i++)
    { // Asumiendo que cada jugador tiene 26 cartas
        printf("%d%c ", jugador->mazo[i].valor, jugador->mazo[i].tipo);
    }
    printf("\n");
}
// Función para imprimir un juego
void imprimir_juego(Juego *juego)
{
    printf("Hilo %d (Juego %d):\n", juego->id, juego->id);
    printf("Cartas del Jugador 1:\n");
    imprimir_cartas(&(juego->jugador1));
    printf("Cartas del Jugador 2:\n");
    imprimir_cartas(&(juego->jugador2));
}
// Función para imprimir todos los juegos
void imprimir_juegos()
{
    for (int i = 0; i < num_juegos; i++)
    {
        imprimir_juego(&juegos[i]);
    }
}
// Función para seleccionar y ejecutar el algoritmo de planificación
void seleccionar_y_ejecutar_algoritmo()
{
    srand(time(NULL));   // Inicializar la semilla para la generación de números aleatorios
    if (rand() % 2 == 0) // Si el número aleatorio es par, ejecutar Round Robin, de lo contrario, ejecutar FSJ
    {
        printf("Algoritmo seleccionado: Round Robin\n");
        // imprimir_juegos();
        ejecutar_listos();
    }
    else
    {
        printf("Algoritmo seleccionado: FSJ\n");
        // imprimir_juegos();
        ejecutar_fsj();
    }
    // Dependiendo del algoritmo seleccionado, ejecutar el otro algoritmo
    // if (algoritmo_actual == RR)
    // {
    //     ejecutar_listos();
    // }
    // else
    // {
    //     ejecutar_fsj();
    // }

    // Actualizar la tabla de procesos con los promedios
    int total_iteraciones = 0;
    double total_tiempo_espera_ES = 0;
    double total_tiempo_espera_listo = 0;
    // Calcular los promedios de cada campo de la tabla de procesos
    for (int i = 0; i < num_juegos; i++)
    {
        total_iteraciones += bcp[i].num_iteraciones;
        total_tiempo_espera_ES += bcp[i].promedio_tiempo_espera_ES;
        total_tiempo_espera_listo += bcp[i].promedio_tiempo_espera_listo;
    }
    // Actualizar los promedios en la tabla de procesos
    tabla_procesos.promedio_num_iteraciones += total_iteraciones / (double)num_juegos;
    tabla_procesos.promedio_tiempo_espera_ES += total_tiempo_espera_ES / num_juegos;
    tabla_procesos.promedio_tiempo_espera_listo += total_tiempo_espera_listo / num_juegos;
}

void manejar_entrada() {
    initscr();  // Inicia el modo ncurses (esto es para que getch() funcione)
    timeout(0); // Configura la entrada para que sea no bloqueante (esto es para que getch() no bloquee el programa)
    noecho();   // Desactiva el eco de la entrada del teclado (esto es para que no se muestre el caracter que se presiona)
    while (1) {
        int ch = getch();
        if (ch == 'c') {  // Suponiendo que 'c' es la tecla para cambiar el algoritmo
            if (algoritmo_actual == RR) {
                algoritmo_actual = SJF;
            } else {
                algoritmo_actual = RR;
            }
        }
    }
    endwin();  // Finaliza el modo ncurses
}

// Función para inicializar los juegos
void inicializar_juegos()
{
    num_juegos = 5; // Suponiendo que hay 3 juegos
    juegos = (Juego *)malloc(num_juegos * sizeof(Juego));
    bcp = (BCP *)malloc(num_juegos * sizeof(BCP)); // Asignar memoria para los BCP

    for (int i = 0; i < num_juegos; i++)
    {
        juegos[i].id = i + 1; // ID único para cada juego

        // Crear el mazo de cartas y barajarlo
        Carta mazo[52];
        inicializar_mazo(mazo);
        barajar_mazo(mazo);

        // Crear dos jugadores y distribuir las cartas
        juegos[i].jugador1.mazo = (Carta *)malloc(26 * sizeof(Carta));
        juegos[i].jugador2.mazo = (Carta *)malloc(26 * sizeof(Carta));
        distribuir_cartas(&(juegos[i].jugador1), &(juegos[i].jugador2), mazo);

        agregar_a_listos(&juegos[i]);
        inicializar_jugadas_para_escribir();
    }

    for (int i = 0; i < num_juegos; i++)
    {
        bcp[i].num_iteraciones = 0;
        bcp[i].avance_pila_codigo = 0;
        bcp[i].tiempo_espera_ES = 0;
        bcp[i].promedio_tiempo_espera_ES = 0;
        bcp[i].promedio_tiempo_espera_listo = 0;
    }

    tabla_procesos.promedio_num_iteraciones = 0;
    tabla_procesos.promedio_tiempo_espera_ES = 0;
    tabla_procesos.promedio_tiempo_espera_listo = 0;
}

// Imprimir todos los campos de tabla de procesos
void imprimir_tabla_procesos()
{
    printf("Tabla de procesos:\n");
    for (int i = 0; i < num_juegos; i++)
    {
        printf("Juego %d:\n", i + 1);
        printf("Número de iteraciones: %d\n", bcp[i].num_iteraciones);
        printf("Avance de pila de código: %d\n", bcp[i].avance_pila_codigo);
        printf("Tiempo de espera en E/S: %f\n", bcp[i].tiempo_espera_ES);
        printf("Promedio de tiempo de espera en E/S: %f\n", bcp[i].promedio_tiempo_espera_ES);
        printf("Promedio de tiempo de espera en listo: %f\n", bcp[i].promedio_tiempo_espera_listo);
    }
}
int main()
{
    // Crear un hilo para manejar la entrada del teclado
    //pthread_t hilo_entrada;
    //pthread_create(&hilo_entrada, NULL, (void *)manejar_entrada, NULL);
    // Inicializar la semilla para la generación de números aleatorios
    srand(time(NULL));
    // Inicializar los juegos
    inicializar_juegos();
    // Seleccionar y ejecutar el algoritmo de planificación
    seleccionar_y_ejecutar_algoritmo();
    inicializar_jugadas_para_escribir();
    imprimir_tabla_procesos();
    // Liberar la memoria asignada de los juegos
    free(bcp);
    for (int i = 0; i < num_juegos; i++)
    {
        free(juegos[i].jugador1.mazo);
        free(juegos[i].jugador2.mazo);
    }
    free(juegos);
    // Imprimir la tabla de procesos

    return 0;
}
