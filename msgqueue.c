/**
 * Msqueue implementa una coda di messaggi, utilizzata dal server chatty per
 * fare lo store dei messaggi ricevuti fino a un max di "historysize".
 * @see message.h
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief Implementazione della history messaggi
*/
#include "msgqueue.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Crea una coda di messaggi (history) di dimensione dim (historysize)
 * @param dim dimensione massima coda
 *
 * @return ritorna la coda messaggi (msgqueue_t)
 */
msgqueue_t * createMsgQueue(int dim){
	if(dim<1) dim=1;
	msgqueue_t * new = malloc(sizeof(msgqueue_t));
	if(new){
		new->size=0;
		new->dim_max=dim+1;
		new->head=new->tail=NULL;
	}
	return new;
}

/**
 * @brief Crea un elemento per la history (di tipo: msgnode_t) facendone
 * una copia completa
 * @param msgtocpy messaggio che deve essere copiato nella history (message_t)
 *
 * @return ritorna il nodo della lista contenente il messaggio copiato
 */
msgnode_t * createMsgNode(message_t * msgtocpy){
	if(msgtocpy){
		//Allochiamo il nuovo nodo
		msgnode_t * new = malloc(sizeof(msgnode_t));
		if(new){
			//Allochiamo un nuovo buffer per il messaggio e ci copiamo dentro
			char * buf;
			buf = malloc(sizeof(char)*(msgtocpy->data.hdr.len));
			strncpy(buf,msgtocpy->data.buf,msgtocpy->data.hdr.len);
			//Settiamo il nodo: nodo->msg (1/2)
			new->msg=malloc(sizeof(message_t));
			setHeader(&new->msg->hdr, msgtocpy->hdr.op, msgtocpy->hdr.sender);
			setData(&new->msg->data, msgtocpy->data.hdr.receiver, buf, msgtocpy->data.hdr.len);
			//Settiamo il nodo: nodo->next(2/2)
			new->next=NULL;
			return new;
		}
		else{
			return NULL;
		}
	}
	return NULL;
}

/**
 * @brief Estrae il primo messaggio dalla hsitory
 * @param queue coda da dove estrarre il messaggio
 *
 * @return ritorno il messaggio estratto
 * @return (NULL se non ci sono messaggi)
 */
message_t * popMsgQueue(msgqueue_t * queue){
	message_t * ret = NULL;
	if(queue->size>0){
		if(queue->head->msg!=NULL){
			ret = malloc(sizeof(message_t)); //alloco messaggio da ritornare
			message_t * tmp = queue->head->msg; //Prendo il messaggio che sta in testa
			char * buf = malloc(sizeof(char)*tmp->data.hdr.len);
			strncpy(buf,tmp->data.buf,tmp->data.hdr.len);
			setHeader(&ret->hdr,tmp->hdr.op,tmp->hdr.sender);
			setData(&ret->data,tmp->data.hdr.receiver,buf,tmp->data.hdr.len);
			//ora che ho copiato il messaggio aggiorno in testa
			msgnode_t * newhead=queue->head->next; //nuova testa;
			destroyMsgNode(queue->head);
			queue->head=newhead; //AGGIORNO IL PUNTATORE IN testa
			queue->size--; //Diminuisco la dimenesione
		}
	}
	return ret;
}

/**
 * @brief Inserisce a fine coda il messaggio
 * @param queue history dove inserire i messaggi
 * @param msgtocpy il messaggio che deve essere copiato nella history
 *
 * @return -1 in caso di fallimento (queueu==NULL || msgtocpy ==NULL)
 * @return 0 in caso di successo
 */
int pushMsgQueue(msgqueue_t * queue, message_t * msgtocpy){

	int ret=-1;
	if(queue && msgtocpy){
		//Creiamo un nuovo nodo
		msgnode_t * new = createMsgNode(msgtocpy);

		//Inseriamolo
		if(new){
			if(!queue->head){ //Se è il 1°
				queue->head=queue->tail=new;
				queue->size++;
			}
			else{ //Se è in corso
				queue->tail->next=new;
				queue->tail=new;

				if(queue->size<queue->dim_max) queue->size++;
				else if(queue->size==queue->dim_max){
					msgnode_t * newhead=queue->head->next;
					destroyMsgNode(queue->head);
					queue->head=newhead;
				}
			}
			ret = 0;
		}
	}
	return ret;
}

/**
 * @brief Funzione che dealloca dalla memoria tutta la history
 * @param queue coda dei messaggi da eliminare
 */
void destroyMsgQueue(msgqueue_t * queue){
	while(queue->head){
		msgnode_t * newhead=queue->head->next;
		destroyMsgNode(queue->head);
		queue->head=newhead;
	}
	free(queue);
}

/**
 * @function destroyMsgNode
 * @brief Distrugge un nodo della history
 * @param node nodo da deallocare (msgnode_t)
 */
void destroyMsgNode(msgnode_t * node){
	free(node->msg->data.buf);//elimino il buffer messaggio.
	free(node->msg);
	//Distruggo il nodo
	free(node);
	//node=NULL;
}
