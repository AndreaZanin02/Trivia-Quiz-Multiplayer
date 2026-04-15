#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "Strutture_server.h"
#include "Costanti.h"
#include "Comunicazione.h"

extern struct gesThread informazioni;


//////////////////////////////////////////////////////
//  Funzioni che lavorano sulla struttura del quiz  //
//////////////////////////////////////////////////////


// Funzione di utilità che legge un file fino al carattere richiesto
int lettura(char *testo, int file, int cursore, char carattere){

    // Conto i caratteri che leggo per aggiornare poi il cursore
    int caratteriLetti = 0;

    // Sposta il puntatore di lettura all'inizio usando funzione di libreria
    lseek(file, cursore, SEEK_SET);

    // Leggo un carattere alla volta fino a che non arrivo al carattere cercato
    while (read(file, testo, 1) > 0 && *(testo) != carattere){
        testo++;
        caratteriLetti++;
    }

    // A quel punto aggiungo fine stringa
    *(testo) = '\0';

    // Sommo al cursore i caratteri letti
    return cursore + caratteriLetti + 1;

}

// Legge da file le domande e inizializza la struttura triviaQUiz
void compila(struct triviaQuiz *quiz){

    int successo, file, cursore;
    char testo[LUNG_MES];

    // Alloco lo spazio per contenere i temi
    quiz->elencoTemi = (struct tema *)malloc(sizeof(struct tema) * NUM_TEMI);
    if(quiz->elencoTemi == NULL){
        perror("Errore allocazione elencoTemi");
        exit(1);
    }

    // Vado nella cartella delle domande con la funz di libreria chdir()
    successo = chdir("Domande");
    if(successo != 0){
        perror("Errore aprendo la cartella domande");
        exit(1);
    }

    // Per ogni singolo tema del quiz
    for(int i=0; i < NUM_TEMI; i++){
        // Creo la struttura del tema
        struct tema t;
        cursore = 0;

        // Prendo il nome del file
        char nomeFile[LUNG_NOME_FILE];
        sprintf(nomeFile, "%d-tema.txt", i + 1);

        // Lo apro in sola lettura
        file = open(nomeFile, O_RDONLY);
        if(file == -1){
            perror("Errore durante apertura del file");
            exit(1);
        }

        // La prima linea del file contiene il nome del tema
        cursore = lettura(testo, file, cursore, '\n');
        strcpy(t.nome, testo);

        // Le successive NUM_DOM righe sono le domande con le relative risposte
        for(int j=0; j < NUM_DOM; j++){
            struct domanda dom;

            // Prendo testo domanda
            cursore = lettura(testo, file, cursore, '&');
            strcpy(dom.testoDomanda, testo);

            // Prendo risposta
            cursore = lettura(testo, file, cursore, '\r');
            strcpy((dom.risposta), testo);

            // Metto la domanda nel tema
            t.domande[j] = dom;
        }

        // Attacco il tema al quiz
        quiz->elencoTemi[i] = t;

        // Il file non serve più, lo chiudo
        close(file);
    }

}


///////////////////////////////////////////////////////
//  Funzioni per la gestione dei dati dei giocatori  //
///////////////////////////////////////////////////////


// Inserisce un nuovo giocatore in lista, lavorando in mutua esclusione
void insUtenti(struct elencoUtenti *giocatori, struct utentiRegistrati *nuovoG){

    pthread_mutex_lock(&giocatori->mutex);

    // Ricerca della fine della lista
    struct utentiRegistrati **i = &giocatori->testa;
    while((*i) != NULL) i = &(*i)->next;

    // Inserimento nuovo giocatore
    (*i) = nuovoG;
    giocatori->numeroUtenti++;

    // Liberazione mutex
    pthread_mutex_unlock(&giocatori->mutex);

}

// Cancella un giocatore dalla lista (usando il suo nome), in mutua eslusione
void delUtenti(struct elencoUtenti *giocatori, char *nomeCercato, struct classifica *cla){

    bool trovato = false;
    struct utentiRegistrati **i = NULL;
    struct utentiRegistrati *bersaglio = NULL;

    pthread_mutex_lock(&giocatori->mutex);

    i = &giocatori->testa;

    // Ricerca utente
    while((*i) != NULL){
        if(strcmp((*i)->nome, nomeCercato) == 0){
            trovato = true;
            break;
        }
        i = &(*i)->next;
    }

    // Se si è trovato, eliminiamo l'utente
    if(trovato){

        //Lo estraggo dalla lista
        bersaglio = (*i);
        (*i) = (*i)->next;

        // Controllo se ha completato dei temi
        for(int j=0; j < NUM_TEMI; j++){
            if(bersaglio->temi[j].completato) cla->numCompletato[j]--;
        }

        // Lo dealloco
        free(bersaglio);
        giocatori->numeroUtenti--;

    }
    else{
        printf("Giocatore non esistente");
    }

    pthread_mutex_unlock(&giocatori->mutex);

}

// Funzione che verifica la correttezza del nome inserito
bool validitaNome(char *nuovoNome, struct elencoUtenti *giocatori){

    struct utentiRegistrati *u = giocatori->testa;
    while(u != NULL){
        if(strcmp(u->nome, nuovoNome) == 0) return false;
        u = u->next;
    }
    return true;

}

// Funzione che inserisce un punteggio in modo ordinato nella classifica
void insClassifica(struct classifica *cla, struct utentiRegistrati *u, int punteggio, int numeroTema){

    pthread_mutex_lock(&cla->mutex[numeroTema-1]);

    // Puntatore da usare per l'inserimento
    struct punti **p = &cla->elencoPunti[numeroTema-1];
    struct punti *corrente = cla->elencoPunti[numeroTema-1];

    // Creazione della struttura dati con il punteggio
    struct punti *nuovoPunt = (struct punti *)malloc(sizeof(struct punti));
    if (nuovoPunt == NULL){
        perror("Errore durante l'allocazione del nuovo punteggio");
        exit(1);
    }
    nuovoPunt->utente = u;
    nuovoPunt->punti = punteggio;
    nuovoPunt->next = NULL;

    // Incremento numero dei partecipenti
    cla->numGiocato[numeroTema-1]++;

    //Inserimento ordinato in lista
    if (*p == NULL || (*p)->punti <= punteggio) {
        nuovoPunt->next = *p;
        *p = nuovoPunt;

        pthread_mutex_unlock(&cla->mutex[numeroTema-1]);
        return;
    }

    // Nel caso non dovessimo inserire in testa
    while (corrente->next != NULL && corrente->next->punti > punteggio) {
        corrente = corrente->next;
    }
    nuovoPunt->next = corrente->next;
    corrente->next = nuovoPunt;

    pthread_mutex_unlock(&cla->mutex[numeroTema-1]);

}

// Elimina i punteggi di un utente, lavorando in mutua esclusione
void delClassifica(struct classifica *cla, struct utentiRegistrati *u, int tema){

    struct punti *pun = NULL;
    struct punti *prev = NULL;
    int temaOriginale = tema;

    if(tema == 0) tema = 1;

    // Se temaOriginale == 0 per ogni tema, altrimenti solo per quello richiesto
    while (tema < NUM_TEMI + 1){

        // Se l'utente lo ha giocato
        if (u->temi[tema-1].giocato){
            pthread_mutex_lock(&cla->mutex[tema-1]);

            // Decremento il numero di persone che lo giocano
            cla->numGiocato[tema-1]--;

            // Punto il vettore punteggi del tema
            pun = cla->elencoPunti[tema-1];

            // Cerco il giocatore u tra i punteggi del tema e elimino il suo nodo
            while (pun != NULL && strcmp(pun->utente->nome, u->nome) == 0) {
                struct punti *temp = pun;
                cla->elencoPunti[tema-1] = pun->next;
                pun = pun->next;
                free(temp);
            }
            while (pun != NULL) {
                while (pun != NULL && strcmp(pun->utente->nome, u->nome) != 0) {
                    prev = pun;
                    pun = pun->next;
                }

                if (pun == NULL) {
                    pthread_mutex_unlock(&cla->mutex[tema-1]);
                    return;
                }

                prev->next = pun->next;
                free(pun);
                pun = prev->next;
            }

            pthread_mutex_unlock(&cla->mutex[tema-1]);
        }

        if(temaOriginale != 0) break;
        tema++;

    }

}


/////////////////////////////////////////////
//  Funzioni per la preparazione dei menù  //
/////////////////////////////////////////////


// Visualizza l'elenco dei temi disponibili sul server
void stampaTemi(struct triviaQuiz *quiz){

    // Cornice del titolo del gioco
    printf("///////////////////////////////\n");
    printf("//  TRIVIA QUIZ MULTIPLAYER  //\n");
    printf("///////////////////////////////\n\n");
    printf("Temi disponibili:\n");

    for(int i = 1; i <= NUM_TEMI; i++) printf("%d  %s\n", i, quiz->elencoTemi[i-1].nome);
    printf("\n");

}

// Stampa il menù del server
void stampaMenu(struct menu *m){

    // Elimino il menù precedente dallo schermo
    system("clear");

    // Punto le cose che mi servono nel menù per evitare catene di ->
    struct triviaQuiz *quiz = m->quiz;
    struct elencoUtenti *giocatori = m->giocatori;
    struct classifica *cla = m->classifica;

    // Stampa dei temi dei quiz
    stampaTemi(quiz);

    // Stampo la lista dei partecipanti accedendo in mutua esclusione
    printf("Partecipanti (%d)\n", giocatori->numeroUtenti);
    pthread_mutex_lock(&giocatori->mutex);
    for(struct utentiRegistrati *i = giocatori->testa; i != NULL; i = i->next){
        printf("- %s\n", i->nome);
    }
    pthread_mutex_unlock(&giocatori->mutex);
    printf("\n");

    // Stampo la classifica in mutua esclusione
    for(int i=0; i < NUM_TEMI; i++){
        pthread_mutex_lock(&cla->mutex[i]);
        printf("Punteggio tema %d\n", i + 1);
        for(struct punti *j = cla->elencoPunti[i]; j != NULL; j = j->next){
            printf("- %s %d\n", j->utente->nome, j->punti);
        }
        printf("\n");
        pthread_mutex_unlock(&cla->mutex[i]);
    }

    // Stampo i temi completati in mutua esclusione
    pthread_mutex_lock(&giocatori->mutex);
    for (int i = 0; i < NUM_TEMI; i++){
        printf("Quiz Tema %d completato da:\n", i + 1);
        if (cla->numCompletato > 0){
            for(struct utentiRegistrati *j = giocatori->testa; j != NULL; j = j->next){
                if (j->temi[i].completato) printf("- %s\n", j->nome);    
            }
        }
        printf("\n");
    }
    pthread_mutex_unlock(&giocatori->mutex);

}


///////////////////////////////
//  Funzioni per la partita  //
///////////////////////////////


// True se la risposta data è corretta, altrimenti false
bool corretta(struct domanda *dom, char *risp){

    if(strcmp(dom->risposta, risp) == 0) return true;
    return false;

}

// Funzione che viene passata ai thread che fanno giocare gli utenti
void * gioco(void *parametriThread){

    int temaScelto, punteggioUtente = 0;
    int domandeInviate = 0;
    struct utentiRegistrati *u = NULL;
    struct punti *itInvioPunteggi = NULL;
    char nomeNuovoUtente[LUNG_MES];
    char mes[LUNG_MES];
    bool successo = false;

    // Creo puntatori ai campi di gio (struttura dati dello schema di gioco)
    struct triviaQuiz *quiz = ((struct parametri *)parametriThread)->gio->quiz;
    struct classifica *cla = ((struct parametri *)parametriThread)->gio->classifica;
    struct elencoUtenti *giocatori = ((struct parametri *)parametriThread)->gio->giocatori;
    int socketClient = ((struct parametri *)parametriThread)->gio->socket;
    struct menu *men = ((struct parametri *)parametriThread)->men;

    // Creo la struttura per gestire il SIGPIPE e la inserisco in lista
    struct infoThread *info = (struct infoThread *)malloc(sizeof(struct infoThread));
    if( info == NULL){
        perror("Errore durante l'allocazione della variabile infoThread");
        exit(1);
    }
    info->tid = pthread_self();
    info->cla = cla;
    info->giocatori = giocatori;
    info->idDelSocket = socketClient;
    info->men = men;
    info->u = NULL;
    info->next = informazioni.elencoThread;
    informazioni.elencoThread = info;

    // Prendo il nome del nuovo giocatore finché non ne fornisce uno corretto
    do{
        ricezione(nomeNuovoUtente, socketClient, true);
        successo = validitaNome(nomeNuovoUtente, giocatori);
        if(successo) sprintf(mes, "1");
        else sprintf(mes, "0");
        invio(mes, socketClient, true);
    }while(!successo);

    // A questo punto il nome inserito è corretto: creo la struttura
    u = (struct utentiRegistrati *)malloc(sizeof(struct utentiRegistrati));
    if( u == NULL){
        perror("Errore durante l'allocazione del nuovo utente");
        exit(1);
    }

    // La inizializzo
    strcpy(u->nome, nomeNuovoUtente);
    u->next = NULL;

    // Alloco anche il suo vettore con lo stato dei temi
    u->temi = (struct statoTema *)malloc(NUM_TEMI * sizeof(struct statoTema));
    if( u->temi == NULL){
        perror("Errore durante l'allocazione dello stato dei temi dell'utente");
        exit(1);
    }

    // E inizializzo ogni sua cella
    for (int i = 0; i < NUM_TEMI; i++){
        u->temi[i].giocato = false;
        u->temi[i].completato = false;
    }

    // Aggiungo u all'elenco di giocatori e nelle informazioni, completando la sua iscrizione
    insUtenti(giocatori, u);
    info->u = u;
    stampaMenu(men);

    // Finché l'utente vuole e può giocare, lo facciamo divertire
    for(;;){

        int j = 0;
        int numNonGiocati = 0;

        // Controllo che l'utente possa giocare ancora
        for (int i = 0; i < NUM_TEMI; i++){
            if (u->temi[i].giocato == false) numNonGiocati++;
        }

        // Creo vettore per associare codice inviato dal client al numero di tema 
        int *vetTemiMostrati = (int *)malloc(numNonGiocati * sizeof(int));
        if(vetTemiMostrati == NULL){
            perror("Errore nella creazione vettore temi non giocati");
            exit(1);
        }

        // Comunico al client a quanti temi può ancora giocare (anche se fosse 0)
        sprintf(mes, "%d", numNonGiocati);
        invio(mes, socketClient, true);

        // E se ha finito chiudo la connessione, eliminando l'utente come richiesto da specifiche
        if (numNonGiocati == 0){ 
            delClassifica(cla, u, 0);
            delUtenti(giocatori, u->nome, cla);
            close(socketClient);
            free(vetTemiMostrati);
            stampaMenu(men);
            goto fine;
        }

        // Altrimenti gli comunico anche quali temi può ancora giocare
        for (int i = 0; i < NUM_TEMI; i++){

            if (!u->temi[i].giocato){

            strcpy(mes, quiz->elencoTemi[i].nome);
            invio(mes, socketClient, true);

            // Dato che nel menù utente potrebbero non essere mostrati tutti i temi, salvo nella cella il nome del tema
            vetTemiMostrati[j++] = i + 1;

            }

        }

        // L'utente sceglie un tema al quale giocare o manda un comando
        ricezione(mes, socketClient, true);

        // Analisi della risposta
        analisi:
        if(strcmp(mes, "endquiz") == 0){

            // Rimuovo le info del thread dalla lista
            struct infoThread *corrente = informazioni.elencoThread;
            struct infoThread *precedente = NULL;
            while (corrente != NULL){
                int tidCercato = pthread_self();
                if (pthread_equal(corrente->tid, tidCercato)){
                    if (precedente == NULL) informazioni.elencoThread = corrente->next;
                    else precedente->next = corrente->next;
                    free(corrente->cla);
                    free(corrente->giocatori);
                    free(corrente->men);
                    free(corrente->u);
                    free(corrente->next);
                    free(corrente);
                }
                precedente = corrente;
                corrente = corrente->next;
            }

            // Chiudo il gioco e esco dal ciclo
            delClassifica(cla, u, 0);
            delUtenti(giocatori, u->nome, cla);
            close(socketClient);
            free(vetTemiMostrati);
            stampaMenu(men);
            goto fine;

        }
        if(strcmp(mes, "show score") == 0){

            //Mostro i punteggi di ogni tema all'utente
            for (int i = 0; i < NUM_TEMI; i++){

                pthread_mutex_lock(&cla->mutex[i]);

                // Comunico al client quanti giocatori sono presenti in classifica
                sprintf(mes, "%d", cla->numGiocato[i]);
                invio(mes, socketClient, true);

                // Inizializzo l'iteratore per l'invio dei punteggi
                itInvioPunteggi = cla->elencoPunti[i];

                // Procedo all'invio di ogni coppia nome punteggio
                for(itInvioPunteggi = cla->elencoPunti[i]; itInvioPunteggi != NULL; itInvioPunteggi = itInvioPunteggi->next){
                    
                    sprintf(mes, "%s", itInvioPunteggi->utente->nome);
                    invio(mes, socketClient, true);
                    sprintf(mes, "%d", itInvioPunteggi->punti);
                    invio(mes, socketClient, true);

                }            

                pthread_mutex_unlock(&cla->mutex[i]);

            }

            // Analizzo il prossimo suo messaggio
            ricezione(mes, socketClient, true);
            goto analisi;

        }

        // Altrimenti prendo il nuovo tema a cui giocare
        temaScelto = atoi(mes);
        if(temaScelto > numNonGiocati || temaScelto < 1){
            printf("Hai scelto un tema non disponibile\n");
            exit(1);
        }

        // Prendo il nome del temaScelt-esimo tema mostrato
        temaScelto = vetTemiMostrati[temaScelto - 1];

        // Segno che l'utente sta giocando il tema
        u->temi[temaScelto - 1].giocato = true;
        punteggioUtente = 0;
        insClassifica(cla, u, punteggioUtente, temaScelto);

        // Invio le domande all'utente e aspetto la sua risposta
        domandeInviate = 0;
        while(domandeInviate < NUM_DOM){

            strcpy(mes, quiz->elencoTemi[temaScelto - 1].domande[domandeInviate].testoDomanda);
            invio(mes, socketClient, true);  
            ricezione(mes, socketClient, true);

            // Se vuole interrompere il gioco lo faccio uscire
            if (strcmp(mes, "endquiz") == 0){

                // Rimuovo le info del thread dalla lista
                struct infoThread *corrente = informazioni.elencoThread;
                struct infoThread *precedente = NULL;
                int tidCercato = pthread_self();
                
                while (corrente != NULL){
                    if (pthread_equal(corrente->tid, tidCercato)){
                        if (precedente == NULL) informazioni.elencoThread = corrente->next;
                        else precedente->next = corrente->next;
                        free(corrente->cla);
                        free(corrente->giocatori);
                        free(corrente->men);
                        free(corrente->u);
                        free(corrente->next);
                        free(corrente);
                    }
                    precedente = corrente;
                    corrente = corrente->next;
                }
                
                delClassifica(cla, u, 0);
                delUtenti(giocatori, u->nome, cla);
                close(socketClient);
                stampaMenu(men);
                goto fine;

            }

            // Se desidera vedere la classifica, gliela mostro e torno alla domanda
            if (strcmp(mes, "show score") == 0){

                //Mostro i punteggi di ogni tema all'utente
                for (int k = 0; k < NUM_TEMI; k++){
                    pthread_mutex_lock(&cla->mutex[k]);

                    // Comunico al client quanti giocatori sono presenti in classifica
                    sprintf(mes, "%d", cla->numGiocato[k]);
                    invio(mes, socketClient, true);

                    // Inizializzo l'iteratore per l'invio dei punteggi
                    itInvioPunteggi = cla->elencoPunti[k];

                    // Procedo all'invio di ogni coppia nome punteggio
                    for(itInvioPunteggi = cla->elencoPunti[k]; itInvioPunteggi != NULL; itInvioPunteggi = itInvioPunteggi->next){
                        sprintf(mes, "%s", itInvioPunteggi->utente->nome);
                        invio(mes, socketClient, true);
                        sprintf(mes, "%d", itInvioPunteggi->punti);
                        invio(mes, socketClient, true);
                    }            

                    pthread_mutex_unlock(&cla->mutex[k]);

                }
                continue;

            }
            else{

                // Se risponde controllo la risposta e la comunico al client
                bool ok;
                ok = corretta(&quiz->elencoTemi[temaScelto - 1].domande[domandeInviate], mes);
                invio((ok)? "1" : "0", socketClient, true);

                // Aggiorno punteggio in classifica
                delClassifica(cla, u, temaScelto);

                // Aggiorno i punti
                if(ok) punteggioUtente++;

                // Inserisco il giocatore in classifica
                insClassifica(cla, u, punteggioUtente, temaScelto);
                stampaMenu(men);

            }

            domandeInviate++;

        }

        // Se si arriva qua l'utente ha completato il quiz
        u->temi[temaScelto - 1].completato = true;
        cla->numCompletato[temaScelto - 1]++;
        stampaMenu(men);

        // Prima di ripetere il ciclo elimino la memoria allocata
        free(vetTemiMostrati);

    }
    
    // Il thread ha finito, quindi esce
    fine:
    pthread_exit(NULL);

}
