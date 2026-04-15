#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "Librerie/Costanti.h"
#include "Librerie/Comunicazione.h"


///////////////////////////////////
//  Qualche funzione di utilità  //
///////////////////////////////////


// Funzione di stampa del titolo
void stampaTitolo(){

    // Pulisco il terminale e stampo il titolo
    system("clear");
    printf("///////////////////////////////\n");
    printf("//  TRIVIA QUIZ MULTIPLAYER  //\n");
    printf("///////////////////////////////\n\n");

}

// Scrive a schermo la classifica del tema
void stampaClassifica(int socketId){

    int numGiocatori;
    char buf[LUNG_MES];
    int punti;
    char nome[LUNG_MES];
    
    printf("\n\nCLASSIFICA:\n\n");
    
    for (int i = 0; i < NUM_TEMI; i++){

        // Per prima cosa serve sapere quanti giocatori si hanno per il tema
        ricezione(buf, socketId, false);
        numGiocatori = atoi(buf);

        printf("Punteggio del tema %d\n", i + 1);

        // Per ogni giocatore ricevo nome e punti
        for (int j = 0; j < numGiocatori; j++){

            ricezione(buf, socketId, false);
            strcpy(nome, buf);

            ricezione(buf, socketId, false);
            punti = atoi(buf);
            
            printf("- %s %d\n", nome, punti);
        }
        printf("\n");

    }

}

// Il SIGPIPE indica la disconnessione del server, e come richiesto da specifiche va gestita
void serverDisconnesso(int segnale){

    printf("Il server si è disconnesso\n");
    exit(1);

}


///////////////////////////////////////
//  Effettivo programma lato client  //
///////////////////////////////////////


int main(int argc, char *argv[]){

    // Variabili generiche
    int successo = 0;
    char nomeQuiz[LUNG_MES];

    // Variabili per navigare nei menù
    int opzioneScelta = 0;

    // Variabili per la comunicazione
    int socketId;
    struct sockaddr_in indServer;
    char buf[LUNG_MES];

    // Variabili per la scelta del nome
    char nomeScelto[LUNG_MES];
    char validita[LUNG_MES];

    // Varibili per la gestione del quiz
    int numTemiGiocabili;
    char *nomiTemiGiocabili[NUM_TEMI];
    bool rispCorretta;
    char domanda[LUNG_MES];

    // Controllo che il client venga lanciato in maniera corretta
    if (argc != 2){
        printf("Il client va lanciato scrivendo ./client %d ovvero il num di porta\n", PORTA_USATA);
        exit(1);
    }

    // Prima di rendere operativa l'applicazione va gestita la risposta alla disconnessione server
    signal(SIGPIPE, serverDisconnesso);

    // Avvio dell'applicazione lato client
    while(true){

        // Creazione del menù lato client
        do{

            stampaTitolo();

            printf("Menù:\n");
            printf("1 - Vai al gioco\n");
            printf("2 - Chiudi\n\n");
            printf("Digita la tua scelta: ");

            // Inserisco in opzioneScelta la sua scelta
            successo = scanf("%d", &opzioneScelta);
            
            // Analizzo la risposta dell'utente
            if (successo != 1 || (opzioneScelta != 2 && opzioneScelta != 1)) printf("\nScelta non valida\n\n");
            else if (opzioneScelta == 2) goto fine;

            // Non potendomi fidare dell'utente, estraggo tutti i numeri che ha digitato
            while (getchar() != '\n'){;}

        } while (opzioneScelta != 1);

        // Dato che l'utente ha premuto 1: creazione socket
        socketId = socket(AF_INET, SOCK_STREAM, 0);
        if (socketId == -1){
            perror("Creazione socket fallita");
            exit(1);
        }

        // Preparazione connessione al server
        indServer.sin_port = htons(atoi(argv[1]));
        indServer.sin_family = AF_INET;
        inet_pton(AF_INET, IP_SERVER, &indServer.sin_addr);

        // Effettiva connessione al server
        successo = connect(socketId, (struct sockaddr *) &indServer, sizeof(indServer));
        if (successo == -1){
            perror("Errore di connessione");
            exit(1);
        }

        // Ora che si è connessi bisogna scegliere un nome
        stampaTitolo();
        sceltaNome:
        while (true){

            printf("Inserisci il tuo nome: ");

            // Controllo il nome scelto
            if (fgets(nomeScelto, sizeof(nomeScelto), stdin) != NULL){

                // Controllo se l'utente inserisce troppi caratteri controllando se manca \n a fine nome
                if(strlen(nomeScelto) > 0 && nomeScelto[strlen(nomeScelto) - 1] != '\n'){
                    while ((getchar()) != '\n'){;}
                    printf("La lunghezza massima è %d caratteri\n", LUNG_MES);
                    continue;
                }
                
                // Devo inserire il fine stringa al posto dello \n
                if (strlen(nomeScelto) > 0) nomeScelto[strlen(nomeScelto) - 1] = '\0';

                // Non posso accettare un nome vuoto
                if (strlen(nomeScelto) == 0){
                    printf("Il nome non può essere vuoto\n");
                    continue;
                }

                // Se invec tutto va bene procedo all'invio al server
                break;

            }
            else printf("Si è verificato un errore nella lettura del nome\n");
        }

        // Comunico il nuovo nome al server per ulteriori controlli
        invio(nomeScelto, socketId, false);
        ricezione(validita, socketId, false);

        // Controllo della validità
        if (strcmp(validita, "0") == 0){
            printf("\nIl nome è già utilizzato da un altro utente\n\n");
            goto sceltaNome;
        }

        // Selezionato un nome, è il momento di giocare
        while(true){
            
            // Secondo il protocollo il server mi comunica quanti quiz sono disponibili per l'utente
            ricezione(buf, socketId, false);
            numTemiGiocabili = atoi(buf);

            // Se l'utente ha giocato a tutti i temi esce dall'app
            if (numTemiGiocabili == 0){
                printf("Hai giocato a tutti i quiz\n");
                goto fine;
            }

            // Altrimenti stampo all'utente i temi disponibili
            printf("\n\nTemi attualmente giocabili:\n\n");
            for (int i = 1; i <= numTemiGiocabili; i++){

                // Ricevo i nomi dei temi giocabili dal server
                ricezione(buf, socketId, false);

                // Li salvo
                nomiTemiGiocabili[i - 1] = (char *)malloc((strlen(buf) + 1) * sizeof(char));
                if( nomiTemiGiocabili[i - 1] == NULL){
                    perror("Errore nella creazione del nome del tema");
                    exit(1);
                }
                strcpy(nomiTemiGiocabili[i - 1], buf);

                // E li mostro all'utente
                printf("%d -> %s\n", i, buf);

            }

            // Permetto all'utente di scegliere a quale tema giocare
            do{
                printf("\nFai la tua scelta: ");

                // Azzero opzioneScelta che era stata usata anche prima
                opzioneScelta = 0;

                // Prendo la cifra digitata dalla tastiera
                if (fgets(buf, sizeof(buf), stdin) != NULL) {

                    size_t len = strlen(buf);

                    // Controllo se l'utente inserisce troppi caratteri controllando se manca \n a fine nome
                    if(len > 0 && buf[len - 1] != '\n'){
                        while ((getchar()) != '\n'){;}
                        printf("Tema non valido");
                        continue;
                    }

                    // Rimuovo il carattere di nuova linea se presente
                    if (len > 0) buf[len - 1] = '\0';

                    // Controllo se l'utente vuole vedere la classifica
                    if (strcmp(buf, "show score") == 0) {

                        // Devo comunicarlo al server così che inizi a inviarmi i dati
                        invio(buf, socketId, false);

                        // Prendo i dati in arrivo e li scrivo sullo schermo
                        stampaClassifica(socketId);

                        // Si torna a chiedere all'utente cosa vuole fare
                        continue;

                    }
                    
                    // Controllo se l'utente vuole uscire dal quiz
                    if (strcmp(buf, "endquiz") == 0){

                        printf("Hai deciso di chiudere il quiz\n");

                        // Lo comunico anche al server
                        invio(buf, socketId, false);
                        goto fineQuiz;

                    }

                    // Se invece ha scritto un numero controllo che sia effettivamente un tema disponibile
                    successo = sscanf(buf, "%d", &opzioneScelta);
                    if (successo != 1 || opzioneScelta > numTemiGiocabili || opzioneScelta < 1) printf("Tema non giocabile\n");

                    }
                    else printf("Non sono riuscito a leggere\n");

            } while (opzioneScelta > numTemiGiocabili || opzioneScelta < 1);

            // Ripulisco un po' la memoria, mi basta sapere solo il nome del tema giocato
            strcpy(nomeQuiz, nomiTemiGiocabili[opzioneScelta - 1]);
            for(int j=0; j < numTemiGiocabili; j++) free(nomiTemiGiocabili[j]);

            // Comunico la scelta al server        
            sprintf(buf, "%d", opzioneScelta);
            invio(buf, socketId, false);

            // Stampo il nome del quiz
            printf("\n\n%s\n", nomeQuiz);

            // Ora il server inizia a inviare le domande
            for (int i = 0; i < NUM_DOM; i++){

                // Prendo la domanda
                ricezione(domanda, socketId, false);

                // Fino a che l'utente non risponde qualcosa di sensato
                while (true){

                    size_t lung;

                    // Stampo domanda e prendo risposta
                    printf("\n%s\n\nRisposta scritta in minuscolo: ", domanda);
                    if (fgets(buf, sizeof(buf), stdin) == NULL){
                        printf("Ho fallito nel leggere la risposta da tastiera\n");
                        continue;
                    }

                    // Controllo che l'utente non abbia inserito troppi caratteri
                    lung = strlen(buf);
                    if(lung > 0 && buf[lung - 1] != '\n'){
                        while ((getchar()) != '\n'){;}
                        printf("Il massimo numero di caratteri consentiti è %d\n", LUNG_MES);
                        continue;
                    }

                    // Inserisco fine stringa e aggiorno la lunghezza
                    if (lung > 0){
                        buf[lung - 1] = '\0';
                    }

                    // Controllo se l'utente non avesse scritto nulla (ovvero solo /n)
                    if (lung == 1){
                        printf("Sicuramente la risposta non è il nulla\n");
                        continue;
                    }

                    // Se l'utente ha dato una risposta la invio al server
                    break;

                }

                invio(buf, socketId, false);

                // Guardo che risposta ho passato al server per sapere cosa aspettarmi
                if (strcmp(buf, "show score") == 0){
                    stampaClassifica(socketId);
                    i--;
                    continue;
                }
                if (strcmp(buf, "endquiz") == 0){
                    printf("\nHai deciso di chiudere il quiz\n\n");
                    goto fineQuiz;
                }

                // Se l'utente non ha usato un comando speciale, mi aspetto la correzione del server
                ricezione(buf, socketId, false);
                rispCorretta = (strcmp(buf, "1") == 0)? true : false;

                // La comunico all'utente
                if (rispCorretta) printf("\nRisposta corretta\n");
                else printf("\nRisposta errata\n");

            }

        }

        // Chiusura connessione al server e ritorno al menu iniziale
        fineQuiz:
        close(socketId);

    }

    // Chiusura programma
    fine:
    return 0;

}