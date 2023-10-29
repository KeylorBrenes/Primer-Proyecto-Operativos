#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>
#include <unistd.h> // Para usar usleep

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
    // ... (otras variables relacionadas con el juego, como el estado de E/S, etc.)
} Juego;

// Variables globales
Juego *juegos;  // Array de juegos
int num_juegos; // Número de juegos

pthread_mutex_t lock; // Bloqueo para sincronización de hilos

typedef struct
{
    int num_iteraciones; // Jugadas hechas
    int avance_pila_codigo;
    double tiempo_espera_ES; // Tiempo de espera en E/S
    double promedio_tiempo_espera_ES;
    double promedio_tiempo_espera_listo;
} BCP;

typedef struct
{
    double promedio_num_iteraciones;     // Promedio de jugadas hechas entre todos los hilos
    double promedio_tiempo_espera_ES;    // Promedio de tiempo de espera en E/S entre todos los hilos
    double promedio_tiempo_espera_listo; // Promedio de tiempo de espera en listo entre todos los hilos
} TablaProcesos;

BCP *bcp; // Un puntero a un arreglo de BCP
TablaProcesos tabla_procesos;

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
        int r = i + rand() % (52 - i);
        Carta temp = mazo[i];
        mazo[i] = mazo[r];
        mazo[r] = temp;
    }
}

// Función para distribuir las cartas entre dos jugadores
void distribuir_cartas(Jugador *jugador1, Jugador *jugador2, Carta *mazo)
{
    for (int i = 0; i < 26; i++)
    {
        jugador1->mazo[i] = mazo[i * 2];
        jugador2->mazo[i] = mazo[i * 2 + 1];
    }
    jugador1->num_cartas = jugador2->num_cartas = 26;
    jugador1->puntos = jugador2->puntos = 0;
}

void inicializar_jugadas_para_escribir();

// Función para jugar un turno
void jugar_turno(Jugador *jugador1, Jugador *jugador2, Juego *juegoActual, int numero_juego, int numero_jugada)
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
        escribir_en_archivo(&juegos[numero_juego - 1]);
        usleep((rand() % 6 + 5) * 1000); // Dormir entre 5 y 10 milisegundos
        inicializar_jugadas_para_escribir();
    }
}

typedef struct Nodo
{
    Juego *juego;
    struct Nodo *siguiente;
} Nodo;

Nodo *lista_listos = NULL;
int num_juegos;
Juego *juegos;

void escribir_en_archivo(Juego *juego);
void agregar_a_listos(Juego *juego);

void *rutina_hilo(void *arg)
{
    Juego *juego = (Juego *)arg;
    int indice_bcp = juego->id - 1; // Suponiendo que el id del juego es el índice en el array bcp

    // Simulando un quantum de Round Robin con un número aleatorio entre 2 y 10
    int quantum = rand() % 9 + 2;
    printf("Quantum: %d\n", quantum);

    for (int i = 0; i < quantum && juego->jugador1.num_cartas > 0 && juego->jugador2.num_cartas > 0; i++)
    {
        bcp[indice_bcp].num_iteraciones++;
        jugar_turno(&(juego->jugador1), &(juego->jugador2), juego, juego->id, bcp[indice_bcp].num_iteraciones);
    }

    // Si el juego no ha terminado, reubicarlo al final de la cola de listos.
    if (juego->jugador1.num_cartas > 0 && juego->jugador2.num_cartas > 0)
    {
        agregar_a_listos(juego);
    }

    return NULL;
}

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

void ejecutar_listos()
{
    while (lista_listos != NULL)
    {
        Nodo *nodo_actual = lista_listos;
        lista_listos = lista_listos->siguiente; // Retira el juego de la cabeza de la lista de listos.
        nodo_actual->siguiente = NULL;          // Asegúrate de que el nodo_actual no tenga un siguiente, ya que será reagregado a la lista si es necesario.

        rutina_hilo((void *)nodo_actual->juego);

        free(nodo_actual); // Libera el nodo actual, ya que agregar_a_listos creará un nuevo nodo si es necesario.
    }
}

float calcular_promedio(Juego *juego)
{
    return (juego->jugador1.num_cartas + juego->jugador1.puntos) / 2.0;
}

Juego *seleccionar_juego_fsj()
{
    Nodo *nodo_actual = lista_listos;
    Juego *juego_seleccionado = NULL;
    float promedio_minimo = FLT_MAX;

    while (nodo_actual != NULL)
    {
        float promedio = calcular_promedio(nodo_actual->juego);
        if (promedio < promedio_minimo)
        {
            promedio_minimo = promedio;
            juego_seleccionado = nodo_actual->juego;
        }
        nodo_actual = nodo_actual->siguiente;
    }

    return juego_seleccionado;
}

void *rutina_hilo_fsj(void *arg)
{
    Juego *juego = (Juego *)arg;
    int indice_bcp = juego->id - 1; // Suponiendo que el id del juego es el índice en el array bcp
    while (juego->jugador1.num_cartas > 0 && juego->jugador2.num_cartas > 0)
    {
        bcp[indice_bcp].num_iteraciones++;
        jugar_turno(&(juego->jugador1), &(juego->jugador2), juego, juego->id, bcp[indice_bcp].num_iteraciones + 1);
    }

    // Simulando operación de E/S: escribir en un archivo (esto se implementará en el futuro)
    printf("Operación de E/S: escribiendo en archivo...\n");

    return NULL;
}

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

void ejecutar_fsj()
{
    while (lista_listos != NULL)
    {
        Juego *juego_seleccionado = seleccionar_juego_fsj();
        if (juego_seleccionado == NULL)
            break; // No hay juegos en la lista de listos

        pthread_t hilo;
        pthread_create(&hilo, NULL, rutina_hilo_fsj, (void *)juego_seleccionado);
        pthread_join(hilo, NULL);

        eliminar_juego(juego_seleccionado); // Eliminar el juego seleccionado de la lista de listos
    }
}

void imprimir_cartas(Jugador *jugador)
{
    printf("Cartas del Jugador: ");
    for (int i = 0; i < 26; i++)
    { // Asumiendo que cada jugador tiene 26 cartas
        printf("%d%c ", jugador->mazo[i].valor, jugador->mazo[i].tipo);
    }
    printf("\n");
}

void imprimir_juego(Juego *juego)
{
    printf("Hilo %d (Juego %d):\n", juego->id, juego->id);
    printf("Cartas del Jugador 1:\n");
    imprimir_cartas(&(juego->jugador1));
    printf("Cartas del Jugador 2:\n");
    imprimir_cartas(&(juego->jugador2));
}

void imprimir_juegos()
{
    for (int i = 0; i < num_juegos; i++)
    {
        imprimir_juego(&juegos[i]);
    }
}

void seleccionar_y_ejecutar_algoritmo()
{
    srand(time(NULL));
    if (rand() % 2 == 0)
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

    // Actualizar la tabla de procesos con los promedios
    int total_iteraciones = 0;
    double total_tiempo_espera_ES = 0;
    double total_tiempo_espera_listo = 0;

    for (int i = 0; i < num_juegos; i++)
    {
        total_iteraciones += bcp[i].num_iteraciones;
        total_tiempo_espera_ES += bcp[i].promedio_tiempo_espera_ES;
        total_tiempo_espera_listo += bcp[i].promedio_tiempo_espera_listo;
    }

    tabla_procesos.promedio_num_iteraciones = total_iteraciones / (double)num_juegos;
    tabla_procesos.promedio_tiempo_espera_ES = total_tiempo_espera_ES / num_juegos;
    tabla_procesos.promedio_tiempo_espera_listo = total_tiempo_espera_listo / num_juegos;
}

typedef struct NodoBloqueado
{
    Juego *juego;
    struct NodoBloqueado *siguiente;
} NodoBloqueado;

NodoBloqueado *lista_bloqueados = NULL;

// Variable global para almacenar el número aleatorio de jugadas antes de escribir en el archivo
int jugadas_para_escribir;
// Función para inicializar el valor aleatorio de jugadas_para_escribir
void inicializar_jugadas_para_escribir()
{
    jugadas_para_escribir = rand() % 11 + 5; // Valor aleatorio entre 5 y 15
}
void escribir_en_archivo(Juego *juego)
{
    char nombre_archivo[20];
    snprintf(nombre_archivo, sizeof(nombre_archivo), "jugadas_%d.txt", juego->id);

    FILE *archivo = fopen(nombre_archivo, "a");
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo");
        return;
    }

    fprintf(archivo, "Jugador 1: %d puntos, Jugador 2: %d puntos\n",
            juego->jugador1.puntos, juego->jugador2.puntos);

    fclose(archivo);
}

void inicializar_juegos()
{
    num_juegos = 3; // Suponiendo que hay 3 juegos
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

int main()
{
    // Inicializar la semilla para la generación de números aleatorios
    srand(time(NULL));
    // Inicializar los juegos
    inicializar_juegos();
    // Seleccionar y ejecutar el algoritmo de planificación
    seleccionar_y_ejecutar_algoritmo();
    inicializar_jugadas_para_escribir();
    free(bcp);
    for (int i = 0; i < num_juegos; i++)
    {
        free(juegos[i].jugador1.mazo);
        free(juegos[i].jugador2.mazo);
    }
    free(juegos);

    printf("Promedio de jugadas hechas: %f\n", tabla_procesos.promedio_num_iteraciones);
    printf("Promedio de tiempo de espera en E/S: %f\n", tabla_procesos.promedio_tiempo_espera_ES);
    printf("Promedio de tiempo de espera en listo: %f\n", tabla_procesos.promedio_tiempo_espera_listo);

    return 0;
}
