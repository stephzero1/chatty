/**
 * Msqueue implementa una coda di messaggi, utilizzata dal server chatty per
 * fare lo store dei messaggi ricevuti fino a un max di "historysize".
 * @see message.h
 *
 * @file msgqueue.h
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief Implementazione della history messaggi
*/
#ifndef MSGQUEUE_H_
#define MSGQUEUE_H_

#include "message.h"

/**
 * @struct msgnode_t
 * @brief Struttura che identifica un messaggio (msg + puntatore a next msg)
 * @var msgnode_t::msg
 * variabile di tipo message_t (contenente il messaggio da salvare)
 * @var msgnode_t::next
 * puntatore al messaggio successivo
 */
typedef struct msgnode_s{
	message_t * msg; //Messaggio da salvare
	struct msgnode_s * next;
}msgnode_t;

/**
 * @struct msgqueue_t
 * @brief Implementazione di una lista di messaggi di dimensione finita (historysize)
 * @var msgqueue_t::size
 * dimensione corrente della lista (<=dim_max)
 * @var msgqueue_t::dim_max
 * dimensione massima della history (historysize)
 * @var msgqueue_t::head
 * puntatore al messaggio in testa
 * @var msgqueue_t::tail
 * puntatore al messaggio in coda
*/
typedef struct msgqueue_s{
	size_t size;
	size_t dim_max;
	msgnode_t * head;
	msgnode_t * tail;
}msgqueue_t;

/**
 * @brief Crea una coda di messaggi (history) di dimensione dim (historysize)
 * @param dim dimensione massima coda
 *
 * @return ritorna la coda messaggi (msgqueue_t)
 */
msgqueue_t * createMsgQueue(int dim);

/**
 * @brief Crea un elemento per la history (di tipo: msgnode_t) facendone
 * una copia completa
 * @param msgtocpy messaggio che deve essere copiato nella history (message_t)
 *
 * @return ritorna il nodo della lista contenente il messaggio copiato
 */
msgnode_t * createMsgNode();

/**
 * @brief Estrae il primo messaggio dalla hsitory
 * @param queue coda da dove estrarre il messaggio
 *
 * @return ritorno il messaggio estratto
 * @return (NULL se non ci sono messaggi)
 */
message_t * popMsgQueue(msgqueue_t * queue);

/**
 * @brief Inserisce a fine coda il messaggio
 * @param queue history dove inserire i messaggi
 * @param msgtocpy il messaggio che deve essere copiato nella history
 *
 * @return -1 in caso di fallimento (queueu==NULL || msgtocpy ==NULL)
 * @return 0 in caso di successo
 */
int pushMsgQueue(msgqueue_t * queue, message_t * msg);

/**
 * @brief Funzione che dealloca dalla memoria tutta la history
 * @param queue coda dei messaggi da eliminare
 */
void destroyMsgQueue(msgqueue_t * queue);

/**
 * @function destroyMsgNode
 * @brief Distrugge un nodo della history
 * @param node nodo da deallocare (msgnode_t)
 */
void destroyMsgNode(msgnode_t * node);

#endif
