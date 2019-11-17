#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define N_PARADAS 5
#define EN_RUTA 0
#define EN_PARADA 1
#define MIN_SLEEP_RUTA 0
#define MAX_SLEEP_RUTA 5
#define MAX_USUARIOS 40
#define USUARIOS 4


int estado = EN_RUTA;
int parada_actual = 0;
int n_ocupantes = 0;

// número de personas que desean subir en cada parada
int esperando_parada[N_PARADAS];

// número de personas que desean bajar en cada parada
int esperando_bajar[N_PARADAS];

/* Ahora los cerrojos y semáforos */
// Creamos un megacerrojo y ya si eso luego lo dividimos más
pthread_mutex_t meverything;

/* Variables condicionales */
pthread_cond_t cpueden_subir;
pthread_cond_t cpueden_bajar;
pthread_cond_t cenparada;

void imprimir_estado() {
    // TODO: Lock mutexes
    
    for (int i = 0; i < N_PARADAS; ++i) {
        printf("o Parada %d, S%02d/B%02d\n", esperando_parada[i], esperando_bajar[i]);
        printf("|%s\n", (parada_actual==i)?"Bus here":"");
    }

    // TODO: Unlock mutexes
}

void Autobus_En_Parada() {
    /* Ajustar el estado y bloquear el autobús hasta que no haya pasajeros que
     * quieran bajar y/o subir la parada actual. Después se pone en marcha */
}

void Conducir_Hasta_Siguiente_Parada() {
    /* Establecer un Retardo que simule el trayecto y actualizar numero de
     * parada */
    // TODO: Lock
    
    estado = EN_RUTA;

    // TODO: Unlock


    sleep(random()%(MAX_SLEEP_RUTA-MIN_SLEEP_RUTA)+MIN_SLEEP_RUTA);

    // TODO: Lock
    parada_actual = (parada_actual+1)%N_PARADAS;

    signal(/*idk*/);

    // TODO: Unlock
}

void Subir_Autobus(int id_usuario, int origen) {
    /* El usuario indicará que quiere subir en la parada 'origen', esperará
     * a que el autobús se pare en dicha parada y subirá. El id_usuario puede
     * utilizarse para proporcionar información de depuración */
}

void Bajar_Autobus(int id_usuario, int destino) {
    /* El usuario indicará que quiere bajar en la parada 'destino', esperará
     * a que el autobús se pare en dicha parada y bajará. El id_usuario puede
     * utilizarse para proporcionar infomación de depuración */
}

void * thread_autobus(void * args) {
    while (/*condicion*/) {
        Autobus_En_Parada();
        Conducir_Hasta_Siguiente_Parada();
    }
}

void * thread_usuario(void * arg) {
    int id_usuario;
    // Obtener id de usuario

    while (/*condicion*/) {
        a = rand() % N_PARADAS;
        do {
            b = rand() % N_PARADAS;
        } while (a == b);

        Usuario(id_usuario, a, b);
    }
}

void Usuario(int id_usuario, int origen, int destino) {
    // Esperar a que el autobus esté en parada origen para subir
    Subir_Autobus(id_usuario, origen);
    // Bajarme en estacion destino
    Bajar_Autobis(id_usuario, destino);
}

int main(int argc, char* argv[])
{
    int i;
    int nUsuarios = USUARIOS;
    // Definición de variables locales a main
    // Obtener de los argumentos del programa la capacidad del autobus,
    // el numero de usuarios y el numero de apradas
    
    if (nUsuarios > MAX_USUARIOS) {
        fprintf(stderr, "Max number of users is %d\n", MAX_USUARIOS);
    }

    // Inicializamos los mutex
    pthread_mutex_init(&meverything, NULL);

    // Iniciamos las variables condicionales
    pthread_cond_init(&cenparada, NULL); 
    pthread_cond_init(&cpueden_subir, NULL);
    pthread_cond_init(&cpueden_bajar, NULL);

    // Crear el thread Autobus
    
    for (i = 0; i < nUsuarios; ++i) {
        // Crear thread para el usuario i
    }

    // Esperar terminación de los hilos
    
    // Destruimos los mutex
    pthread_mutex_destroy(&meverything);

    // Destruimos las variables condicionales
    pthread_cond_destroy(&cenparada);
    pthread_cond_destroy(&cpueden_subir);
    pthread_cond_destroy(&cpueden_bajar);

	return 0;
}

