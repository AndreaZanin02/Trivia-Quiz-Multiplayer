#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "Librerie/Costanti.h"
#include "Librerie/Strutture_server.h"
#include "Librerie/Comunicazione.h"

// Dichiaro la struttura dati da passare all'handler del SIGPIPE, deve essere globale
struct gesThread informazioni;

// All'arrivo del SIGPIPE eseguo l'endquiz dell'utente che si è disconnesso
void disconnessione(int s){

    pthread_t tidCercato = pthread_self();
    struct elencoUtenti *giocatori;
    struct utentiRegistrati *u;
    struct classifica *cla;
    struct menu *men;
    int idDelSocket;

    pthread_mutex_lock(&informazioni.mutex);

    // Cerco e rimuovo il thread con tid == a quello del thread che ha ricevuto il SIGPIPE
    struct infoThread *corrente = informazioni.elencoThread;
    struct infoThread *precedente = NULL;
    while (corrente != NULL){
        if (pthread_equal(corrente->tid, tidCercato)){
            if (precedente == NULL) informazioni.elencoThread = corrente->next;
            else precedente->next = corrente->next;
            break;
        }
        precedente = corrente;
        corrente = corrente->next;
    }
    if(corrente == NULL) return;
    giocatori = corrente->giocatori;
    u = corrente->u;
    cla = corrente->cla;
    idDelSocket = corrente->idDelSocket;
    men = corrente->men;

    free(corrente);

    pthread_mutex_unlock(&informazioni.mutex);

    // Se l'utente si era registrato al momento della disconnessione
    if(u != NULL){

        // Lo elimino dalle classifiche
        delClassifica(cla, u, 0);

        // E anche dalla lista di tutti gli utenti
        delUtenti(giocatori, u->nome, cla);

    }

    // Chiudo il socket
    close(idDelSocket);

    // Aggiorno il menu
    stampaMenu(men);

    // A questo punto termino il thread
    pthread_exit(NULL);

}

int main(){

    // Variabili per inizializzare il socket
    struct sockaddr_in indServer;
    int socketClient, socketServer;
    struct sockaddr_in indClient;
    socklen_t lungIndClient = sizeof(indClient);
    
    // Strutture dati server
    struct menu *m;
    struct schemaGioco *g;
    struct classifica *cla;
    struct elencoUtenti *giocatori;
    struct triviaQuiz quiz;

    // Variabili di utilità
    int successo, flags;
    struct parametri *parametriThread;

    // Inizializzo il quiz leggendo dal file
    compila(&quiz);

    // Inizializzo la variabile globale informazioni
    informazioni.elencoThread = NULL;
    pthread_mutex_init(&informazioni.mutex, NULL);

    // Creo la classifica e tutti i suoi campi
    cla = (struct classifica *)malloc(sizeof(struct classifica));
    if(cla == NULL){
        perror("Errore nella creazione classifica");
        exit(1);
    }
    cla->mutex = malloc(NUM_TEMI * sizeof(pthread_mutex_t));
    if(cla->mutex == NULL){
        perror("Errore nella creazione vettore semafori");
        exit(1);
    }
    cla->numGiocato = malloc(NUM_TEMI * sizeof(int));
    if(cla->numGiocato == NULL){
        perror("Errore nella creazione vettore con numero partecipanti");
        exit(1);
    }
    cla->numCompletato = malloc(NUM_TEMI * sizeof(int));
    if(cla->numCompletato == NULL){
        perror("Errore nella creazione vettore con numero di utenti che hanno completato i temi");
        exit(1);
    }
    cla->elencoPunti = malloc(NUM_TEMI * sizeof(struct punti));
    if(cla->elencoPunti == NULL){
        perror("Errore nella creazione vettore punteggi");
        exit(1);
    }
    for (int i = 0; i < NUM_TEMI; i++){
        pthread_mutex_init(&cla->mutex[i], NULL);
        cla->numGiocato[i] = 0;
        cla->numCompletato[i] = 0;
        cla->elencoPunti[i] = NULL;
    }

    // Creo la lista di utenti
    giocatori = (struct elencoUtenti *)malloc(sizeof(struct elencoUtenti));
    if(giocatori == NULL){
        perror("Errore nella creazione lista di utenti");
        exit(1);
    }
    giocatori->numeroUtenti = 0;
    giocatori->testa = NULL;
    pthread_mutex_init(&giocatori->mutex, NULL);

    // Creo il menù riepilogativo visibile sul server
    m = (struct menu *)malloc(sizeof(struct menu));
    if(m == NULL){
        perror("Errore nella creazione del menu");
        exit(1);
    }
    m->quiz = &quiz;
    m->classifica = cla;
    m->giocatori = giocatori;

    // Stampo il menù iniziale del server
    stampaMenu(m);

    // Prima di iniziare inibisco SIGPIPE, sennò un client disconnesso causerebbe chiusura processo server
    signal(SIGPIPE, disconnessione);

    // Imposto la modalità non bloccante
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // Creazione del socket di comunicazione
    socketServer = socket(AF_INET, SOCK_STREAM, 0);
    if (socketServer == -1){
        perror("Errore di creazione del socket server");
        exit(1);
    }

    // Inizializzazione campi del socket
    indServer.sin_family = AF_INET;
    indServer.sin_port = htons(PORTA_USATA);
    inet_pton(AF_INET, IP_SERVER, &indServer.sin_addr);

    // Operazione di bind del socket
    successo = bind(socketServer, (struct sockaddr *)&indServer, sizeof(indServer));
    if (successo == -1){
        perror("Errore durante il binding del socket");
        exit(1);
    }

    // Il server può gestire un massimo di MAX_CLIENT contemporaneamente
    successo = listen(socketServer, MAX_CLIENT);
    if (successo == -1){
        perror("Errore durante la listen del socket");
        exit(1);
    }

    // Ora il server è operativo e pronto ad accettare le connessioni
    while (true){

        socketClient = accept(socketServer, (struct sockaddr *)&indClient, &lungIndClient);
        if (socketClient == -1){
            perror("Errore di connessione");
            exit(1);
        }

        // Ogni client viene gestito da un thread
        pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
        if(thread == NULL){
            perror("Errore nella allocazione del thread");
            exit(1);
        }

        // Creo la struttura del gioco
        g = (struct schemaGioco *)malloc(sizeof(struct schemaGioco));
        if(g == NULL){
            perror("Errore nella creazione dello schema del gioco");
            exit(1);
        }
        g->socket = socketClient;
        g->classifica = cla;
        g->quiz = &quiz;
        g->giocatori = giocatori;

        // Creo la variabile da passare al thread
        parametriThread = malloc(sizeof(parametriThread));
        if(parametriThread == NULL){
            perror("Errore nella creazione dei parametri per il thread");
            exit(1);
        }

        // Li inserisco nella struttura da passare al thread
        parametriThread->men = m;
        parametriThread->gio = g;

        // Creo il thread
        successo = pthread_create(thread, NULL, gioco, (void *)parametriThread);
        if (successo){
            perror("Errore durante la creazione di un thread");
            exit(1);
        }

    }

    return 0;

}