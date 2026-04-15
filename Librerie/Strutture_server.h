#include <stdbool.h>
#include <pthread.h>
#include "Costanti.h"


////////////////////////////////////////////////////////////
//  Strutture dati per realizzare il vero e proprio quiz  //
////////////////////////////////////////////////////////////


// Struttura base di una domanda: testo e risposta
struct domanda{

    char testoDomanda[LUNG_MES];
    char risposta[LUNG_MES];

};

// Implementazione dei temi, con il loro nome e l'elenco di domande
struct tema{

    char nome[LUNG_MES];
    struct domanda domande[NUM_DOM];

};

// Struttura del quiz: quantità di temi presenti e elenco ordinato in base al loro numero
struct triviaQuiz{

    struct tema *elencoTemi;

};


////////////////////////////////////////////////////////////////////
//  Strutture dati base per gestire le informazioni degli utenti  //
////////////////////////////////////////////////////////////////////


// Struttura dati per salvare lo stato di un tema relativo a un determinato utente
struct statoTema{

    bool giocato;
    bool completato;

};

// Informazioni sugli utenti, il campo next permette di accodarli in una lista
struct utentiRegistrati{

    char nome[LUNG_MES];
    struct statoTema *temi;
    struct utentiRegistrati *next;

};

// Struttura dati che salva i punti degli utenti, si può organizzare in lista
struct punti{

    struct utentiRegistrati *utente;
    int punti;
    struct punti *next;

};

// Lista degli utenti registrati, accedibile in mutua esclusione
struct elencoUtenti{

    int numeroUtenti;
    struct utentiRegistrati *testa;
    pthread_mutex_t mutex;

};

// Classifica dei giocatori: tutti i campi sono array con indice associato al tema considerato
struct classifica{

    int *numCompletato;
    int *numGiocato;
    struct punti **elencoPunti;
    pthread_mutex_t *mutex;

};


////////////////////////////////////////////////////////////////
//  Strutture dati di cui vengono dotati i thread del server  //
//          contenenti tutte le strutture precedenti          //
////////////////////////////////////////////////////////////////


// Schema di una partita di gioco
struct schemaGioco{

    int socket;
    struct classifica *classifica;
    struct triviaQuiz *quiz;
    struct elencoUtenti *giocatori;

};

// Schema del menu riepilogativo del server
struct menu{

    struct triviaQuiz *quiz;
    struct classifica *classifica;
    struct elencoUtenti *giocatori;

};


//////////////////////////////////////////////////
//  Strutture dati per la gestione del SIGPIPE  //
//////////////////////////////////////////////////


// Struttura dati che per ogni thread salva: tid, elenco giocatori, utente e classifica usata
struct infoThread{

    pthread_t tid;
    struct elencoUtenti *giocatori;
    struct utentiRegistrati *u;
    struct classifica *cla;
    struct menu* men;
    int idDelSocket;
    struct infoThread *next;

};


// Struttura che che permette di accedere alla lista thread in mutua esclusione
struct gesThread{

    struct infoThread *elencoThread;
    pthread_mutex_t mutex;

};


///////////////////////////////////////////////////////
// Struttura di utilità per passare i dati ai thread //
///////////////////////////////////////////////////////


// Struttura per passare tre argomenti alla funzione del thread
struct parametri{

    struct schemaGioco *gio;
    struct menu *men;

};


//////////////////////////////////////////////////////
//  Funzioni che lavorano sulla struttura del quiz  //
//////////////////////////////////////////////////////


void compila(struct triviaQuiz *);


///////////////////////////////////////////////////////
//  Funzioni per la gestione dei dati dei giocatori  //
///////////////////////////////////////////////////////


void insUtenti(struct elencoUtenti *, struct utentiRegistrati *);
void delUtenti(struct elencoUtenti *, char *, struct classifica *);
bool validitaNome(char *, struct elencoUtenti *);

void insClassifica(struct classifica *, struct utentiRegistrati *, int, int);
void delClassifica(struct classifica *, struct utentiRegistrati *, int);


/////////////////////////////////////////////
//  Funzioni per la preparazione dei menù  //
/////////////////////////////////////////////


void stampaTemi(struct triviaQuiz *);
void stampaMenu(struct menu *);


///////////////////////////////
//  Funzioni per la partita  //
///////////////////////////////


bool corretta(struct domanda *, char *);
void * gioco(void *);
