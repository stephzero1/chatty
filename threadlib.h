/**
 * Threadlib implementa un pool di thread secondo il modello Master + Worker.
 * Nella libreria inoltre vengono implementate delle funzioni per istanziare
 * ed avviare i thread con dei task e una funzione per eseguire una cleanup.
 *
 * @file threadlib.h
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief Libreria che implementa un pool di thread.
 */
#if !defined(THREADLIB_H)
#define THREADLIB_H_

#include <pthread.h>

/**
 * @struct pool
 * @brief struttura che implementa un pool di thread
 * @var pool::size
 * numero di thread da voler creare nel pool
 * @var pool::thread
 * thread del pool
 */
typedef struct pool_struct{
	int size;
	pthread_t * thread;
}pool;

/**
 * @brief crea un pool di numt threads
 *
 * @param numt numero di thread worker che si vogliono creare
 * @return pool ritorna un pool di numt threads
 */
pool * createPool(int numt);

/**
 * @brief Inizializza un pool di thread con un task (routine)
 *
 * @param pool il pool di thread da voler inizializzare
 * @param start_routine il task da voler passare ai threads
 */
void initPool(pool * pool, void *(*start_routine) (void *));

/**
 * @brief Ripulisce le strutture utilizzate dal pool e il pool stesso
 *
 * @param pool il pool di thread da deallocare
*/
void destroyPool(pool * pool);

#endif
