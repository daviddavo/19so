// David Cantador Piedras ***REMOVED***W
// David Davó Laviña ***REMOVED***

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#define N_PARADAS 5
#define EN_RUTA 0
#define EN_PARADA 1
#define MIN_SLEEP_RUTA 1
#define MAX_SLEEP_RUTA 7
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
pthread_cond_t cpuede_arrancar;
pthread_cond_t cpueden_subir;
pthread_cond_t cenparada;

void Autobus_En_Parada() {
    /* Ajustar el estado y bloquear el autobús hasta que no haya pasajeros que
     * quieran bajar y/o subir la parada actual. Después se pone en marcha */
    pthread_mutex_lock(&meverything);
    estado = EN_PARADA;
    printf("o Parada % 2d, S% 2d/B% 2d\n", parada_actual, esperando_parada[parada_actual], esperando_bajar[parada_actual]);

    // Que se suban los que se tengan que subir y se bajen los que se tengan que bajar
    pthread_cond_broadcast(&cenparada);
    pthread_mutex_unlock(&meverything);

    // WAIT UNTIL EVERYONE IS ON THE BUS
    pthread_mutex_lock(&meverything);
    while (esperando_parada[parada_actual] > 0 || esperando_bajar[parada_actual] > 0) {
        pthread_cond_wait(&cpuede_arrancar, &meverything);
    }

    pthread_mutex_unlock(&meverything);
}

void Conducir_Hasta_Siguiente_Parada() {
    /* Establecer un Retardo que simule el trayecto y actualizar numero de
     * parada */

    pthread_mutex_lock(&meverything);
    estado = EN_RUTA;
    printf("> En ruta %d/%d\n", n_ocupantes, MAX_USUARIOS);
    pthread_mutex_unlock(&meverything);

    sleep(random()%(MAX_SLEEP_RUTA-MIN_SLEEP_RUTA)+MIN_SLEEP_RUTA);

    pthread_mutex_lock(&meverything);
    parada_actual = (parada_actual+1)%N_PARADAS;

    pthread_cond_signal(&cenparada);
    pthread_mutex_unlock(&meverything);
}

void Subir_Autobus(int id_usuario, int origen) {
    /* El usuario indicará que quiere subir en la parada 'origen', esperará
     * a que el autobús se pare en dicha parada y subirá. El id_usuario puede
     * utilizarse para proporcionar información de depuración */
    pthread_mutex_lock(&meverything);
    esperando_parada[origen]++;
    printf("Usuario %d esperando para subir en %d\n", id_usuario, origen);

    while (!(estado == EN_PARADA && parada_actual == origen)) {
        pthread_cond_wait(&cenparada, &meverything);
    }

    printf("Usuario %d subiendo al bus\n", id_usuario);
    
    n_ocupantes++;
    esperando_parada[origen]--;
    if (esperando_parada[origen] == 0) {
        pthread_cond_signal(&cpuede_arrancar);
    }

    pthread_mutex_unlock(&meverything);
}

void Bajar_Autobus(int id_usuario, int destino) {
    /* El usuario indicará que quiere bajar en la parada 'destino', esperará
     * a que el autobús se pare en dicha parada y bajará. El id_usuario puede
     * utilizarse para proporcionar infomación de depuración */
    pthread_mutex_lock(&meverything);
    esperando_bajar[destino]++;
    printf("Usuario %d esperando para bajarse en %d\n", id_usuario, destino);

    while(!(estado == EN_PARADA && parada_actual == destino)) {
        pthread_cond_wait(&cenparada, &meverything);
    }

    printf("Usuario %d bajando del bus\n", id_usuario);
    n_ocupantes--;
    esperando_bajar[destino]--;
    if (esperando_bajar[destino] == 0) {
        pthread_cond_signal(&cpuede_arrancar);
    }

    pthread_mutex_unlock(&meverything);
}

void * thread_autobus(void * args) {
    printf("Arrancando el autobus\n");

    while (1 /* TODO: condicion*/) {
        Autobus_En_Parada();
        Conducir_Hasta_Siguiente_Parada();
    }
}

void Usuario(int id_usuario, int origen, int destino) {
    // Esperar a que el autobus esté en parada origen para subir
    Subir_Autobus(id_usuario, origen);

    // Bajarme en estacion destino
    Bajar_Autobus(id_usuario, destino);
}

void * thread_usuario(void * arg) {
    int id_usuario = -1, a, b;
    // Obtener id de usuario
    id_usuario = pthread_self();

    while (1) {
        a = rand() % N_PARADAS;
        do {
            b = rand() % N_PARADAS;
        } while (a == b);

        Usuario(id_usuario, a, b);
        sleep(1+random()%USUARIOS);
    }
}

int main(int argc, char* argv[])
{
    int i;
    int nUsuarios = USUARIOS;
    pthread_t tbus;
    pthread_t * tusuarios;
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
    pthread_cond_init(&cpuede_arrancar, NULL);

    tusuarios = malloc(sizeof(pthread_t) * nUsuarios);
    for (i = 0; i < nUsuarios; ++i) {
        // Crear thread para el usuario i
        pthread_create(&tusuarios[i], NULL, thread_usuario, NULL);
    }

    // Crear el thread Autobus
    if (pthread_create(&tbus, NULL, thread_autobus, NULL)) {
        fprintf(stderr, "Error creating thread bus\n");
    };

    // Esperar terminación de los hilos
    // El bus parará cuando no haya más usuarios
    pthread_join(tbus, NULL);

    free(tusuarios);
    
    // Destruimos los mutex
    pthread_mutex_destroy(&meverything);

    // Destruimos las variables condicionales
    pthread_cond_destroy(&cenparada);
    pthread_cond_destroy(&cpuede_arrancar);
    pthread_cond_destroy(&cpueden_subir);

	return 0;
}

