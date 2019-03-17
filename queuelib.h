/**
 * @file queuelib.h
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 * @brief Implementazione di una coda che viene acceduta in maniera concorrente
 */
#if !defined(QUEUELIB_H)
#define QUEUELIB_H_
#define KILLTHREAD 99999999
#include <pthread.h>

/**
 * @struct queue
 * @brief Struttura che implemente una coda concorrente
 *
 * La coda è strutturata in modo tale da essere usata in maniera concorrente
 * fra più thread, quindi utilizza una variabile di mutua esclusione che ne
 * garantisce la consistenza. Inoltre il comportamento della coda è modellato
 * con la caratteristica di bloccare (sospendere) chi richiede di estrarre
 * un elemento se essa è vuota, quindi utilizza una variabile di condizione
 * utilizzata in modo da far sospende il thread che voglia estrarre un elemento
 * dalla coda vuota, e svegliato dal primo che ne inserisce un elemento.
 *
 * @var queue::front
 * primo elemento da estrarre nella coda
 * @var queue::rear
 * ultimo elemento inserito nella coda
 * @var queue::dim
 * dimensione della coda
 * @var queue::elem
 * elemento della coda (fd client accodato)
 * @var queue::mtx
 * variabile di mutua esclusione per accesso concorrente
 * @var queue::cnd1
 * variabile di condizione per coda vuota
*/
typedef struct queue_struct{
	int front;
	int rear;
	int dim;
	int * elem;
	pthread_mutex_t mtx;
	pthread_cond_t cnd1;
}queue;

/**
 * @param coda
 *
 * @return true se la coda è piena, false se la coda è non piena
 */
int isFull(queue * coda);

/**
 * @param coda
 *
 * @return true se la coda è vuota, false se la coda è non vuota
 */
int isEmpty(queue * coda);

/**
 * @brief Crea una coda concorrente
 * @param dim dimensione coda
 *
 * @return la coda concorrente
*/
queue * createQueue(int dim);

/**
 * @brief Funzione che accoda un elemento alla Coda
 *
 * La procedura di enQueue, inserisce un elemento in coda se e solo se essa è non piena
 * e non appena termina l'inserimento, prima di rilasciare la lock, segnala
 * un eventuale thread che si è sospeso in attesa di estrarre.
 *
 * @param coda coda da gestire
 * @param elem elemento da accodare

 * @return 0 se accoda a buon fine
 * @return -1 se c'è un errore
*/
int enQueue(queue * coda, int elem);

/**
 * @brief Funzione che estrae un elemento dalla Coda
 *
 * La funzione deQueue, estrae un elemento per conto del chiamante
 * e lo restituisce se ovviamente la coda è non vuota.
 * In caso di coda vuota, si sospende autonomamente sulla variabile di
 * condizionamento cnd1, in attesa di essere risvegliato da qualche produttore.
 * Caso di riguardo è quando estrae il messaggio KILLTHREAD, esso indica che
 * il chiamante di deQueue, deve terminare.
 *
 * @param coda coda da gestire

 * @return elemento in testa alla coda (si sospende se è vuota)
 * @return -1 se c'è qualche errore
*/
int deQueue(queue * coda);

/**
 * @brief libera la memoria da tutte le strutture utilizzate dalla coda
 * @param coda coda da eliminare
*/
void destroyQueue(queue * coda);

#endif
