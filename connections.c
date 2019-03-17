/**
 * Connections implementa il core della comunicazione tra client e server.
 * È in grado di leggere e scrivere header e data di messaggi di tipo message_t.
 * La comunicazione gira su un socket di tipo AF_UNIX.
 *
 * @see message.h
 *
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 *
 * @brief Contine le funzioni che implementano il protocllo di comunicazione tra il client ed il server
*/
#include <sys/socket.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "connections.h"
#include "message.h"
#include "ops.h"
#include "config.h"

//Crediti per readn e writen: Unix Network Programming, Volume 1: The Sockets Networking API, 3rd Edition
ssize_t readn(int fd, void *vptr, size_t n){
	size_t  nleft;
	ssize_t nread;
	char   *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
			nread = 0;      /* and call readn() again */
			else return (-1);
		}
		else if (nread == 0) break;              /* EOF */
		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft);         /* return >= 0 */
}

ssize_t writen(int fd, void *vptr, size_t n){
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)	nwritten = 0;   /* and call write() again */
			else return (-1);    /* error */
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return (n);
}


/*	###		Funzioni di utilità		###	*/

/**
 * @brief Funzione che esegue la lettura da un buffer
 * @param connfd fd del client
 * @param buffer buffer in cui scrivere il contenuto letto dal client
 * @param length lunghezza buffer da leggere
 *
 * @return 0 se va a buon fine
 * @returno -1 se c'è un errore
 */
static int read_buffer(long connfd, char *buffer, unsigned int length){
	char *tmp=buffer;
	while(length>0){
		int rd=read(connfd,tmp,length);
		if(rd<0) return -1;
		buffer += rd;
		length -= rd;
	}
	return 0;
}

/**
 * @brief Funzione che esegue la scrittura di un buffer su un socket
 * @param connfd fd del client
 * @param buffer buffer di caratteri
 * @param length lunghezza buffer
 *
 * @return 0 se va a buon fine
 * @return-1 se c'è un errore
 */
static int write_buffer(long connfd, char *buffer, unsigned int length){
	while(length>0){
		int wt=write(connfd,buffer,length);
		if(wt<0) return -1;
		buffer += wt;
		length -= wt;
	}
	return 0;
}

/* ### Fine #### */

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
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	//Controllo che rispetti i vincoli dell'header
	if(strlen(path)>UNIX_PATH_MAX){
		fprintf(stderr,"openConnection: path > UNIX_PATH_MAX\n");
		return -1;
	}
	if(ntimes>MAX_RETRIES){
		fprintf(stdout,"openConnection: ntimes > MAX_RETRIES\n");
		ntimes=MAX_RETRIES;
	}
	if(secs>MAX_SLEEPING){
		fprintf(stdout,"openConnection: secs > MAX_SLEEPING\n");
		secs=MAX_SLEEPING;
	}

	//Creo il socket per la comunicazione con il server
	int fd_c=socket(AF_UNIX,SOCK_STREAM,0);
	if(fd_c==-1){perror("openConnection"); return -1;}

	struct sockaddr_un address;
	memset(&address,'0',sizeof(address));
	address.sun_family=AF_UNIX;
	strncpy(address.sun_path,path,UNIX_PATH_MAX);

	for(int i=0; i<ntimes; i++){
		if(connect(fd_c,(struct sockaddr *)&address,sizeof(address))==-1){
			if(errno==ENOENT) sleep(secs);
			else return -1;
		}
		else return fd_c;
	}
	return -1;
}

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
int readHeader(long connfd, message_hdr_t *hdr){
	memset(hdr, 0, sizeof(message_hdr_t));
	if(connfd<0 || hdr==NULL){
		fprintf(stdout,"readHeader: parametri nulli\n");
		return 0;
	}
	int rd=readn(connfd,hdr,sizeof(message_hdr_t));
	if(rd<=0) return rd; //0 o -1

	return 1;
}

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
int readData(long fd, message_data_t *data){
	if(fd<0 || data==NULL){
		fprintf(stderr,"readData: parametri nulli\n");
		return 0;
	}
	//Lettura di "data->hdr"
	int rd=readn(fd,&(data->hdr),sizeof(message_data_hdr_t));
	if(rd<=0) return rd;
	//Lettura di "data->buf"
	if(data->hdr.len==0) data->buf=NULL;
	else{
		data->buf=malloc(sizeof(char)*data->hdr.len);
		rd=read_buffer(fd,data->buf,data->hdr.len);
		if(rd<0){
			free(data->buf);
			return -1;
		}
	}
	return 1;
}

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
int readMsg(long fd, message_t *msg){
	//i controlli li fanno readHeader e readData
	int ret;
	ret=readHeader(fd,&(msg->hdr));
	if (ret<=0) return ret;
	ret=readData(fd,&(msg->data));
	if (ret<=0) return ret;

	return 1; //Tutto ok
}

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
int sendRequest(long fd, message_t *msg){
	//i controlli le fanno le altre func
	int ret;
	ret=sendHeader(fd,&(msg->hdr));
	if (ret==-1) return -1;
	ret=sendData(fd,&(msg->data));
	if (ret==-1) return -1;

	return 1; //tutto ok
}

/**
 * @brief Scrive il body del messaggio sul socket
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 * @return 1 in caso di successo
 */
int sendData(long fd, message_data_t *msg){
	//Scrivo il data->header
	int wt=writen(fd,&(msg->hdr),sizeof(message_data_hdr_t));
	if(wt<0) return -1;
	//Devo ora scriver il buffer
	if(write_buffer(fd,msg->buf,msg->hdr.len)==-1){
		return -1;
	}
	return 1;
}

/**
 * @brief Scrive l'header del messaggio sul socket
 *
 * @param fd descrittore della connessione
 * @param hdr puntatore all'header da inviare
 * @return <=0  se c'è stato un errore
 * @return 1 in caso di successo
 */
int sendHeader(long fd, message_hdr_t *hdr){
	int wt;
	wt=writen(fd,hdr,sizeof(message_hdr_t));
	if(wt<0) return -1;
	return 1;
}
