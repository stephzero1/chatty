/**
 * Userlib è il core di tutta la memorizzazione degli utenti riguardo le principali
 * operazioni di: registrazione/deregistrazione login/disconnessione, e della memorizzazione
 * dei messaggi/file inviati, si serve di due strutture hash: <users> e <fdusr> nei quali
 * si memorizzano i nicname degli user e i relativi filedescriptor nel momento in cui
 * emettono richieste al server, inoltre sono memorizzate alcune info di utilità per il runtime.
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief  Implementazione registrazione utenti + Implementazione operazione utenti
*/
#include "userlib.h"
#include "msgqueue.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
/**
 * @brief Funzione di clean up, elimina il campo <value> della 1° tabella hash utenti (users)
*/
void free_data(void * arg){
	user_data_t * data=(user_data_t*)arg;
	destroyMsgQueue(data->msgq);
	free(data);
}

/**
 * @brief Funzione di supporto per convertire in stringa un long
*/
static char * toString(unsigned long fd){
	const int n = snprintf(NULL, 0, "%lu", fd);
	assert(n > 0);
	char *keytemp=malloc(sizeof(char)*(n+1));
	int c = snprintf(keytemp, n+1, "%lu", fd);
	assert(keytemp[n] == '\0');
	assert(c == n);
	return keytemp;
}

/**
 * @brief Crea la struttura principale per memorizzare gli utenti.
 *
 * Per la memorizzazione di un utente si utilizzano due strutture hash:
 * 1) per memorizzare la stringa dell'utente con le relative informazione di utilità
 * 2)per memorizzare il file descriptor dell'user che richiede di loggarsi associandolo con il relativo nickname.
 * Il motivo di tale scelta implementativa è che il client potrebbe disconnettersi in "maniera implicita"
 * rendendo la disconnessione possibile soltanto per mezzo del suo file descriptor.
 *
 * @param historysize dimensione massima coda messaggi ricevuti
 * @param nbuckets dimensione inziale tabella hash utenti
 *
 * @return puntatore a users_struct_t
*/
users_struct_t * createUsersStruct(unsigned long historysize, unsigned long nbuckets){

	if(historysize<1) historysize=1;
	if(nbuckets<0) nbuckets=4;

	users_struct_t * us = malloc(sizeof(users_struct_t));

	us->users=icl_hash_create(nbuckets,NULL,NULL);
	us->fdusr=icl_hash_create(nbuckets,NULL,NULL);

	us->mtx=malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(us->mtx,NULL);

	us->historysize=historysize;
	us->usersOnline=0;

	return us;
}

/**
 * @brief Funzione che registra gli utenti nell'apposita struttura dati e li connette
 * @param tab struttura dove registrare l'utente
 * @param nick username utente che vuole essere registrato
 * @param fd filedescriptor utente che vuole essere connesso
 *
 * @return -2 se c'è un errore di inserimento utente
 * @return -1 se l'utente è già registrato
 * @return 0 se va a buon fine
 */
int registerUser(users_struct_t * tab, char * nick, unsigned long fd){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	if(!icl_hash_find(tab->users,nick)){//Se è NULL => è un User nuovo
		user_data_t * data = malloc(sizeof(user_data_t));
		//Alloco il nome sulla struttura permamente
		strncpy(data->name,nick,MAX_NAME_LENGTH+1);
		data->fd=fd;
		data->msgq=createMsgQueue(tab->historysize);
		//Inserisco negli user registrati
		icl_entry_t * entry1=icl_hash_insert(tab->users,data->name,data);
		if(!entry1) ret=-2;
		//Inserisco negli user online
		char *keytemp=toString(fd);
		icl_entry_t * entry2=icl_hash_insert(tab->fdusr,keytemp,data->name);
		if(entry2) ret=0;
		tab->usersOnline++;
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

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
int connectUser(users_struct_t * tab, char * nick, unsigned long fd){//Work
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	user_data_t * user = icl_hash_find(tab->users,nick);
	if(user){//Se è registrato
		if(user->fd==-1){//E se deve ancora collegarsi
			user->fd=fd;
			tab->usersOnline++;
			//Aggiungiamo alla seconda tabella hash
			//Convertiamo il fd in stringa:
			char *keytemp=toString(fd);
			icl_entry_t * entry=icl_hash_insert(tab->fdusr,keytemp,user->name);
			if(entry) ret=0; //Collegato!
		}
		else ret=-2; //!già collegato
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Deregistra l'utente che ne fa richiesta (disconnettendolo prima)
 * @param tab struttura dove è registrato l'utente
 * @param nick username utente che vuole essere deregistrato
 * @param fd filedescriptor utente che vuole deregistrarsi
 *
 * @return -1 Se l'utente non è registrato
 * @return 0 Se va a buon fine
 */
int unregisterUser(users_struct_t * tab, char * nick, int fd){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	user_data_t * user = icl_hash_find(tab->users,nick);
	if(user){
		user->fd=-1;
		if(icl_hash_delete(tab->users,nick,NULL,free_data)==0) ret=0;
		char * tmp=toString(fd);
		if(icl_hash_delete(tab->fdusr,tmp,free,NULL)==0) ret=0;
		free(tmp);
		tab->usersOnline--;
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

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
int disconnectUser(users_struct_t * tab, char * nick, unsigned long fd){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	char *keytemp=toString(fd);
	//Nick==NULL -->(Disconnessione implicita)
	if(!nick){
		nick=icl_hash_find(tab->fdusr,keytemp);
	}
	user_data_t * user = icl_hash_find(tab->users,nick);
	if(user){
		if(user->fd!=-1){
			printf("Disconnetto client: [%lu]\n",user->fd);
			tab->usersOnline--;
			user->fd=-1;
			ret=icl_hash_delete(tab->fdusr,keytemp,free,NULL);//0 on Success -1 on failure
		}
	}
	else ret=-2;
	free(keytemp);
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Funzione che inizializza list con i nickname degli utenti attualmente online
 * @param tab struttura dove è registrato l'utente
 * @param list puntatore ad array di caratteri
 *
 * @return -1 Se c'è un errore //Impossibile
 * @return >=0 #utenti online
 */
int getOnlineList(users_struct_t * tab, char ** list){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	//Allochiamo la lista
	int dim=tab->usersOnline;
	*list=malloc(sizeof(char)*dim*(MAX_NAME_LENGTH+1));
	char *tmp=*list;
	//Scorriamo l'hashmap degli utenti
	int i; icl_entry_t * entry; char *kp; user_data_t *dp; //variabile che servono al foreachs
	icl_hash_foreach(tab->users,i,entry,kp,dp){
		if(dp->fd!=-1){
			strncpy(tmp,dp->name,(MAX_NAME_LENGTH+1));
			tmp+=(MAX_NAME_LENGTH+1);
		}
	}
	ret=dim;
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

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
msgqueue_t * getHistory(users_struct_t *tab, char * nick){
	msgqueue_t * ret = NULL;
	pthread_mutex_lock(tab->mtx);
	user_data_t * user = icl_hash_find(tab->users,nick);
	if(user){//devo farne una copia completa (altre strutture potrebbero accedervi)
		printf("Ho %zu mex in coda!!!!\n",user->msgq->size);
		if((ret=createMsgQueue(user->msgq->size))!=NULL){
			msgnode_t * curr = user->msgq->head;
			while(curr){ //Deep Copy di tutti i messaggi nella nuova coda
				pushMsgQueue(ret,curr->msg);
				curr=curr->next;
			}
		}
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Funzione che restituisce il file descriptor associato all'utente "nick"
 * @param tab struttura dove è registrato l'utente
 * @param nick username dell'Utente
 *
 * @return -1 se nick non esiste
 * @return 0 se nick non è online
 * @return fd di nick
 */
int getUserFD(users_struct_t * tab, char * nick){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	user_data_t * user = icl_hash_find(tab->users,nick);
	if(user){//Se c'è l'user
		ret=0;
		if(user->fd!=-1) ret=user->fd; //Se è connesso Prendo il relativo fd
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Funzione che riempie il vettore di interi fds con i fd degli user online
 * @param tab struttura dove è registrato l'utente
 * @param nick username dell'Utente che ne fa richiesta
 * @param fds puntatore a vettore di filedescriptor
 *
 * @return -1 on failure
 * @return #user online on success
 */
int getAllUsersFD(users_struct_t * tab, char * nick, int ** fds){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	ret=tab->usersOnline-1; //Tutti, meno chi ne fa richiesta
	*fds=malloc(sizeof(int)*ret);
	int *tmp=*fds;
	int j=0;
	int i; icl_entry_t * entry; char *kp; user_data_t *dp; //variabile che servono al foreachs
	icl_hash_foreach(tab->users,i,entry,kp,dp){//Per tutti gli utenti (copiati)
		if(strcmp(dp->name,nick)!=0){//Se non voglio inviare un messaggio a se stesso
			if(dp->fd!=-1) {
				tmp[j]=dp->fd;
				j++;
			}
		}
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Funzione che posta un msg nella history di msg->receiver
 * @param tab struttura dove è registrato l'utente
 * @param msg messaggio da postare
 *
 * @return -1 on failure
 * @return 0 in caso di successo
 */
int postOnHistory(users_struct_t *tab, message_t * msg){
	int ret=-1;
	pthread_mutex_lock(tab->mtx);
	user_data_t * user = icl_hash_find(tab->users,msg->data.hdr.receiver);
	if(user){//Se l'user esiste
		if(pushMsgQueue(user->msgq,msg)==0){//Se lo inserisco
			ret=0;
		}
	}
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Funzione che posta un msg nella history di tutti gli utenti online
 * @param tab struttura dove è registrato l'utente
 * @param msg messaggio da postare
 *
 * @return -1 on failure
 * @return 0 in caso di successo
 */
int postOnHistoryAll(users_struct_t *tab, message_t * msg){
	int ret=0;
	int fail=0;
	pthread_mutex_lock(tab->mtx);
	int i; icl_entry_t * entry; char *kp; user_data_t *dp; //variabile che servono al foreachs
	icl_hash_foreach(tab->users,i,entry,kp,dp){//Per tutti gli utenti (copiati)
		if(strcmp(dp->name,msg->hdr.sender)!=0){//Se non voglio inviare un messaggio a se stesso
			if(pushMsgQueue(dp->msgq,msg)==0) ret++; //Se va a buon fine!
			else fail=1;
		}
	}
	if(fail)ret=-1;
	pthread_mutex_unlock(tab->mtx);
	return ret;
}

/**
 * @brief Distrugge le strutture dati relative alla memorizzazione utenti
 * @param tab tabella utenti
*/
void destroyUsersStruct(users_struct_t * tab){
	icl_hash_destroy(tab->users,NULL,free_data);
	icl_hash_destroy(tab->fdusr,free,NULL);
	pthread_mutex_destroy(tab->mtx);
	free(tab->mtx);
	free(tab);
}
