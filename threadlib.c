/**
 * Threadlib implementa un pool di thread secondo il modello Master + Worker.
 * Nella libreria inoltre vengono implementate delle funzioni per istanziare
 * ed avviare i thread con dei task e una funzione per eseguire una cleanup.
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief Libreria che implementa un pool di thread.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "threadlib.h"

/**
 * @brief crea un pool di numt threads
 *
 * @param numt numero di thread worker che si vogliono creare
 * @return pool ritorna un pool di numt threads
 */
pool * createPool(int numt){
	//Controllo i vincoli del pool e in caso esco
	if(numt<0){
		fprintf(stderr,"errore createPool: deve essereci almeno 1 thread");
		exit(EXIT_FAILURE);
	}

	//Alloco il pool
	pool * pool=malloc(sizeof(struct pool_struct));
	if(!pool){
		perror("Errore malloc pool in createPool");
		exit(EXIT_FAILURE);
	}

	//Alloco i thread del pool
	pool->size=numt;
	pool->thread=malloc(sizeof(pthread_t)*numt);
	if((pool->thread)==NULL){
		perror("Errore malloc pthread_t in createPool");
		exit(EXIT_FAILURE);
	}

	return pool;
}

/**
 * @brief Inizializza un pool di thread con un task (routine)
 *
 * @param pool il pool di thread da voler inizializzare
 * @param start_routine il task da voler passare ai threads
 */
void initPool(pool * pool, void *(*start_routine) (void *)){
	for(int i=0; i<pool->size; i++){
		pthread_create(&(pool->thread[i]),NULL,start_routine,NULL);
	}
}

/**
 * @brief Ripulisce le strutture utilizzate dal pool e il pool stesso
 *
 * @param pool il pool di thread da deallocare
*/
void destroyPool(pool * pool){
	free(pool->thread);
	free(pool);
}
