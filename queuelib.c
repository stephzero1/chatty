/**
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 * @brief Implementazione di una coda che viene acceduta in maniera concorrente
 */
#include <stdio.h>
#include <stdlib.h>
#include "queuelib.h"

/**
 * @brief funzione interna che restituisce il modulo positivo
 */
static inline int pos_mod(int i, int n) {
    return (i % n + n) % n;
}

/**
 * @param coda
 *
 * @return true se la coda è piena, false se la coda è non piena
 */
int isFull(queue * coda){
	return ((coda->rear)==pos_mod(coda->front-1,coda->dim));
}

/**
 * @param coda
 *
 * @return true se la coda è vuota, false se la coda è non vuota
 */
int isEmpty(queue * coda){
	return (coda->front==-1);
}

/**
 * @brief Crea una coda concorrente
 * @param dim dimensione coda
 *
 * @return la coda concorrente
*/
queue * createQueue(int dim){
	queue * coda=malloc(sizeof(queue));
	if(!coda){
		perror("malloc in createQueue (coda)");
		exit(EXIT_FAILURE);
	}
	coda->front=coda->rear=-1;
	coda->dim=dim;
	coda->elem=malloc(sizeof(int)*dim);
	if(!(coda->elem)){
		perror("malloc in createQueue (coda->elem)");
		exit(EXIT_FAILURE);
	}
	for(int i=0; i<dim; i++){
		coda->elem[i]=0;
	}
	//Inizializzo mutex e var di condizionamento
	pthread_mutex_init(&(coda->mtx),NULL);
	pthread_cond_init(&(coda->cnd1),NULL);
	return coda;
}

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
int enQueue(queue * coda, int elem){

	int ret=-1; //Valore di ritorno

	pthread_mutex_lock(&(coda->mtx));
	int DIM=coda->dim;
	if(!isFull(coda)){
		//Se c'è spazio allora scrivo
		if(coda->front==-1) coda->front=0; //se è il 1° elemento aggiorno il front a 0
		coda->rear=(coda->rear+1)%DIM;
		coda->elem[coda->rear]=elem;
		ret = 0;
		pthread_cond_signal(&(coda->cnd1));
	}
	pthread_mutex_unlock(&(coda->mtx));

	return ret;
}

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
int deQueue(queue * coda){
	int ret=-1; //Valore di ritorno

	pthread_mutex_lock(&(coda->mtx));
	int DIM=coda->dim;
	while(isEmpty(coda)){
		pthread_cond_wait(&(coda->cnd1),&(coda->mtx)); //Sto in attesa che si riempia
	}
	if(!isEmpty(coda)){
		//estraggo l'elemento
		ret=coda->elem[coda->front];
		if(ret==KILLTHREAD) {pthread_mutex_unlock(&(coda->mtx)); return ret;} //Protocollo di terminazione thread
		if(coda->front==coda->rear) coda->front=coda->rear=-1; //se c'è un solo elmento
		else coda->front=(coda->front+1)%DIM;
	}
	pthread_mutex_unlock(&(coda->mtx));

	return ret;
}

/**
 * @brief libera la memoria da tutte le strutture utilizzate dalla coda
 * @param coda coda da eliminare
*/
void destroyQueue(queue * coda){
	pthread_mutex_lock(&(coda->mtx));
	pthread_mutex_destroy(&(coda->mtx));
	pthread_cond_destroy(&(coda->cnd1));
	free(coda->elem);
	free(coda);
}
