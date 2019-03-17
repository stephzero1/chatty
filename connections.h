/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */

 /**
  * Connections implementa il core della comunicazione tra client e server.
  * È in grado di leggere e scrivere header e data di messaggi di tipo message_t.
  * La comunicazione gira su un socket di tipo AF_UNIX.
  *
  * @see message.h
  *
  * @file connections.h
  *
  * @author Stefano Spadola 534919
  * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
  * originale dell'autore
  *
  * @brief Contine le funzioni che implementano il protocllo di comunicazione tra il client ed il server
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include "message.h"
#include "connections.h"

/**
 * @brief Apre una connessione attravverso un socket AF_UNIX verso il server
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return fd associato alla connessione in caso di successo
 * @return -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs);

// -------- server side -----

/**
 * @brief Legge l'header del messaggio dal socket
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 * @return 1 in caso di successo
 */
int readHeader(long connfd, message_hdr_t *hdr);

/**
 * @brief Legge il body del messaggio dal socket
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 * @return 1 in caso di successo
 */
int readData(long fd, message_data_t *data);

/**
 * @brief Legge l'intero messaggio (header + data) dal socket
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 * @return 1 in caso di successo
 */
int readMsg(long fd, message_t *msg);

/* da completare da parte dello studente con altri metodi di interfaccia */

// ------- client side ------
/**
 * @brief Scrive un intero messaggio sul socket
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 * @return 1 in caso di successo
 */
int sendRequest(long fd, message_t *msg);

/**
 * @brief Scrive il body del messaggio sul socket
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 * @return 1 in caso di successo
 */
int sendData(long fd, message_data_t *msg);

/* da completare da parte dello studente con eventuali altri metodi di interfaccia */

/**
 * @brief Scrive l'header del messaggio sul socket
 *
 * @param fd descrittore della connessione
 * @param hdr puntatore all'header da inviare
 * @return <=0  se c'è stato un errore
 * @return 1 in caso di successo
 */
int sendHeader(long fd, message_hdr_t *hdr);

#endif /* CONNECTIONS_H_ */
