/**
 * Userlib è il core di tutta la memorizzazione degli utenti riguardo le principali
 * operazioni di: registrazione/deregistrazione login/disconnessione, e della memorizzazione
 * dei messaggi/file inviati, si serve di due strutture hash: <users> e <fdusr> nei quali
 * si memorizzano i nicname degli user e i relativi filedescriptor nel momento in cui
 * emettono richieste al serve, inoltre sono memorizzate alcune info di utilità per il runtime.
 *
 * @file userlib.h
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief Implementazione registrazione utenti + Implementazione operazione utenti
*/
#ifndef USERLIB_H_
#define USERLIB_H_

#include "icl_hash.h"
#include "msgqueue.h"
#include <pthread.h>

/**
 * @struct users_struct_t
 * @brief Struttura utilizzata dal Server chatty,
 * per memorizzare gli utenti che iscrivono e i loro messaggi
 * @var users_struct_t::users
 * 1° tabella hash usata per la memorizzazione degli utenti registrati
 *			  <key,data>=<nickname,user_data_t>
 * @var users_struct_t::fdusr
 * 2° tabella hash usata per la memorizzazione dei file descriptor degli utenti online
 *			  <key,data>=<fd(stringa),nickname>
 * @var users_struct_t::mtx
 * Variabile di mutua esclusione usate per accedere alla struttura
 * @var users_struct_t::historysize
 * Dimensione della History dei messaggi
 */
typedef struct users_struct_s{
	icl_hash_t * users;
	icl_hash_t * fdusr;
	pthread_mutex_t * mtx;
	unsigned int historysize;
	unsigned int usersOnline;
}users_struct_t;

/**
 * @struct user_data_t
 * @brief Struttura dati utilizzata come "value" per la 1° mappa utenti (users)
 * @var user_data_t::name
 * Nickname user registrato
 * @var user_data_t::fd
 * Relativo file descriptor
 * @var user_data_t::msgq
 * Coda messaggi ricevuti (history)
*/
typedef struct user_data_s{
	char name[MAX_NAME_LENGTH+1];
	unsigned long fd;
	msgqueue_t * msgq;
}user_data_t;

/**
 * @brief Crea la struttura principale per memorizzare gli utenti.
 *
 * Per la memorizzazione di un utente si utilizzano due strutture hash: 1) per memorizzare
 * la stringa dell'utente con le relative informazione di utilità e
 * 2)per memorizzare il file descriptor dell'user che richiede di loggarsi associandolo con il relativo nickname.
 * Il motivo di tale scelta implementativa è che il client potrebbe disconnettersi in "maniera implicita"
 * rendendo la disconnessione possibile soltanto per mezzo del suo file descriptor.
 *
 * @param historysize dimensione massima coda messaggi ricevuti
 * @param nbuckets dimensione inziale tabella hash utenti
 *
 * @return puntatore a users_struct_t
*/
users_struct_t * createUsersStruct(unsigned long history, unsigned long nbuckets);

/**
 * @brief Funzione che registra gli utenti nell'apposita struttura dati
 * @param tab struttura dove registrare l'utente
 * @param nick username utente che vuole essere registrato
 *
 * @return -1 se l'utente è già registrato
 * @return 0 se va a buon fine
 */
int registerUser(users_struct_t * tab, char * nick, unsigned long fd);

/**
 * @brief Connette l'utente che ne fa richiesta aggiornandone il relativo filedescriptor
 * @param tab struttura dove è registrato l'utente
 * @param nick username utente che vuole essere connesso
 * @param fd filedescriptor utente che lo richiede
 *
 * @return -1 Se l'utente non è registrato
 * @return -2 Se l'utente è già connesso
 * @return 0 Se va a buon fine
 */
int connectUser(users_struct_t * tab, char * nick, unsigned long fd);

/**
 * @brief Deregistra l'utente che ne fa richiesta
 * @param tab struttura dove è registrato l'utente
 * @param nick username utente che vuole essere deregistrato
 *
 * @return -1 Se l'utente non è registrato
 * @return 0 Se va a buon fine
 */
int unregisterUser(users_struct_t * tab, char * nick, int fd);

/**
 * @brief Disconnette l'utente che ne fa richiesta
 *
 * La disconnessione può essere di due tipo 1)Implicita e 2)Esplcita. Ciò per come è
 * strutturato il client. Infatti, il client potrebbe uscire in un qualsiasi momento
 * del suo ciclo di vita, questo perchè non è detto che intenda mandare esplicitamente
 * al server un messaggio di disconnessione. Perciò il metodo di disconnessione è
 * implementato in modo tale che prende come paramentri sia l'username che il filedescriptor
 * e qual'ora l'username passato sia NULL, la disconnessione verrà trattata in modo implicito.
 *
 * @param tab struttura dove è registrato l'utente
 * @param nick username utente che vuole essere disconnesso
 *
 * @return -1 Se l'utente non è online
 * @return -2 Se l'utente non è registrato
 * @return 0 Se va a buon fine
 */
int disconnectUser(users_struct_t * tab, char * nick, unsigned long fd);

/**
 * @brief Funzione che inizializza list con i nickname degli utenti attualmente online
 * @param tab struttura dove è registrato l'utente
 * @param list puntatore ad array di caratteri
 *
 * @return -1 Se c'è un errore
 * @return >=0 #utenti online
 */
int getOnlineList(users_struct_t * tab, char ** list);

/**
 * @brief Funzione che restituisce una copia dei messaggi in coda
 *
 * Questa funzione ritorna una copia dei messaggi nella History
 * Si preferisce farne una copia intera per motivi di consistenza,
 * in quanto un altro thread del server, potrebbe voler registrare un
 * messaggio mandato da un altro utente.
 *
 * @param tab struttura dove è registrato l'utente
 * @param nick username dell'Utente
 *
 * @return NULL se l'utente è offline
 * @return msgqueue contenente la history di "nick"
 */
msgqueue_t * getHistory(users_struct_t *tab, char * nick);

/**
 * @brief Funzione che restituisce il file descriptor associato all'utente "nick"
 * @param tab struttura dove è registrato l'utente
 * @param nick username dell'Utente
 *
 * @return -1 se nick non esiste
 * @return 0 se nick non è online
 * @return fd di nick
 */
int getUserFD(users_struct_t * tab, char * nick);

/**
 * @brief Funzione che riempie il vettore di interi fds con i fd degli user online
 * @param tab struttura dove è registrato l'utente
 * @param nick username dell'Utente che ne fa richiesta
 * @param fds puntatore a vettore di filedescriptor
 *
 * @return -1 on failure
 * @return #user online on success
 */
int getAllUsersFD(users_struct_t * tab, char * nick, int ** fds);

/**
 * @brief Funzione che posta un msg nella history di msg->receiver
 * @param tab struttura dove è registrato l'utente
 * @param msg messaggio da postare
 *
 * @return -1 on failure
 * @return 0 in caso di successo
 */
int postOnHistory(users_struct_t *tab,message_t * msg);

/**
 * @brief Funzione che posta un msg nella history di tutti gli utenti online
 * @param tab struttura dove è registrato l'utente
 * @param msg messaggio da postare
 *
 * @return -1 on failure
 * @return 0 in caso di successo
 */
int postOnHistoryAll(users_struct_t *tab, message_t * msg);

/**
 * @brief Distrugge le strutture dati relative alla memorizzazione utenti
 * @param tab tabella utenti
*/
void free_data(void * arg);

void destroyUsersStruct(users_struct_t* tab);

#endif
