#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "Costanti.h"
#include "Comunicazione.h"


/////////////////////////////////////////////////////////////////////////////////////////////////
//  Implementazione delle funzioni di comunicazione usabili sia dall'app client che dal server //
/////////////////////////////////////////////////////////////////////////////////////////////////


// Funzione che serve a facilitare l'invio dei pacchetti
void invio(char* buf, int sd, bool server){

    uint32_t lungh;
    int successo;
    int lunOriginaria;

    // La funzione calcola e comunica lunghezza messaggio
    lunOriginaria = strlen(buf);
    lungh = htonl(lunOriginaria);
    successo = send(sd, &lungh, sizeof(uint32_t), 0);

    // Controllo errori
    if(successo != sizeof(uint32_t)){

        // Se non ha inviato nulla il socket probabilmente era stato chiuso
        if(successo==-1 && (errno==ECONNRESET || errno==EPIPE)){

            // Chiudo anche io il socket inutilizzabile
            close(sd);

            // Essendo utilizzata sia dai thread (nel server) che da processi normali (nel client), ho bisogno
            // di utilizzare la corretta funzione di uscita
            if(server) pthread_exit(NULL);
            else exit(2);

        }

        // Altrimenti informo della presenza di un errore e termino
        else{

            perror("Si è verificato un errore nell'invio di lungh mess");
            close(sd);

            // Esce con codice di errore
            if(server) pthread_exit(NULL);
            else exit(1);

        }

    }

    // Dopo aver inviato informazioni sulla lunghezza, invia il mess (è senza fine stringa)
    successo = send(sd, buf, lunOriginaria, 0);

    // Controllo errori nell'invio messaggio
    if(successo != lunOriginaria){

        // Se non ha inviato nulla il socket probabilmente era stato chiuso
        if(successo==-1 && (errno==ECONNRESET || errno==EPIPE)){

            // Chiudo anche io il socket inutilizzabile
            close(sd);

            // Essendo utilizzata sia dai thread che da processi normali, ho bisogno
            // di utilizzare la corretta funzione di uscita
            if(server) pthread_exit(NULL);
            else exit(2);

        }

        // Altrimenti informo della presenza di un errore e termino
        else{

            perror("Si è verificato un errore nell'invio del mess");
            close(sd);

            if(server) pthread_exit(NULL);
            else exit(1);

        }

    }

}


// Funzione di ricezione, complementare a quella di invio
void ricezione(char* buf, int sd, bool server){

    uint32_t lungh;
    int successo;

    // Attendo arrivo della lunghezza dato che il mittente usa sicuramente invio()
    successo = recv(sd, &lungh, sizeof(uint32_t), 0);
    lungh = ntohl(lungh);

    // Controllo errori
    if (successo == 0){

        // Se l'utente chiude l'applicazione, faccio attivare l'interruzione SIGPIPE che pulisce il server dai dati
        if(server){
            raise(SIGPIPE);
            pthread_exit(NULL);
        }

        // Se non ho ricevuto nulla la connessione è stata chiusa, mi libero del socket
        close(sd);
        printf("La connessione è terminata\n");
        exit(2);

    }
    
    // Se il messaggio è arrivato solo in parte si è verificato un errore
    else if (successo != sizeof(uint32_t)){

        perror("Errore nella ricezione lunghezza");
        close(sd);

        // Esco con codice 1 che è il parametro di exit per gli errori
        if(server) pthread_exit(NULL);
        else exit(1);

    }

    // Attendo arrivo vero e proprio messaggio
    successo = recv(sd, buf, lungh, 0);

    // Controllo se la connessione si fosse interrotta
    if (successo == 0){

        printf("La connessione è terminata\n");
        close(sd);

        // Termino
        if(server) pthread_exit(NULL);
        else exit(2);

    }

    // Altrimenti presenza errore
    else if (successo != lungh){

        perror("Errore durante la ricezione del messaggio");
        close(sd);
        if(server) pthread_exit(NULL);
        else exit(1);

    }

    // Se tutto va bene mi ricordo di inserire marca di fine stringa
    buf[lungh] = '\0';

}
