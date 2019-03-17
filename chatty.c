/** @file chatty.c
 *
 * Chatty è un server che permette a dei client di poter chattare fra di loro,
 * consentendogli di scambiarsi messaggi sia testuali che file.
 * Il server è implementato in modo da riuscire a servire in maniera concorrente
 * più client, attraverso un pool di thread che riescono a servire in maniera
 * efficente i client e i messaggi scambiati.
 * Il design adottato è quello di un server Master-Slave (Master-Worker), dove
 * in questo caso, il thread main del processo chatty, lancia "n" Worker, e si mette
 * in ascolto su un socket di tipo AF_UNIX per servire i client.
 * Il main quindi accoglie i fd dei client da servire, e schedula i Worker per
 * poter eseguire l'operazione richiesta dagli utenti della chat.
 * I Worker cicleranno all'infinito estraendo da una coda i filedescriptor da servire,
 * aspettando di essere terminati da uno dei segnali registrati dal main.
 *
 *	@author Stefano Spadola 534919
 *	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *	originale dell'autore
 *	@brief Implementazione del Server chatty
*/
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>

#include <errno.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>

#include <fcntl.h>
#include <sys/mman.h>

#include "connections.h"
#include "queuelib.h"
#include "threadlib.h"
#include "parser.h"
#include "icl_hash.h"
#include "userlib.h"
#include "stats.h"

static void printMsg(message_t *msg){
	printf("|Messaggio letto:\n");
	printf("	|OP: %d\n",msg->hdr.op);
	printf("	|Sender: %s\n",msg->hdr.sender);
	printf("	|Receiver: %s\n",msg->data.hdr.receiver);
	printf("	|MsgLength: %d\n",msg->data.hdr.len);
	printf("	|Msg: %s\n",msg->data.buf);
	printf("\n");
	fflush(stdout);
}

//Struttura per le var di configurazione del Server
conf_var * config;

//Coda per i filedescriptor dei client
queue * coda;

//Struttura che memorizza gli utenti registrati/connessi
users_struct_t * usr;

//fd_set per la select
fd_set set;
//Mutex per: fd_set set
static pthread_mutex_t mtx_set = PTHREAD_MUTEX_INITIALIZER;

//MUTEXAGGIUNTA ESKEREEE
static pthread_mutex_t mtx_online=PTHREAD_MUTEX_INITIALIZER;

//Struttura che memorizza le statistiche del server
struct statistics chattyStats = {0,0,0,0,0,0,0};
static pthread_mutex_t mtxstats = PTHREAD_MUTEX_INITIALIZER;
//Funzione che aggiorna le statistiche del server
static void updateStats(int reg, int conn, int del, int ndel, int fdel, int fndel, int err){
	pthread_mutex_lock(&mtxstats);
	chattyStats.nusers+=reg;
	chattyStats.nonline+=conn;
	chattyStats.ndelivered+=del;
	chattyStats.nnotdelivered+=ndel;
	chattyStats.nfiledelivered+=fdel;
	chattyStats.nfilenotdelivered+=fndel;
	chattyStats.nerrors+=err;
	pthread_mutex_unlock(&mtxstats);
}

//Variabile per terminazione Server
volatile sig_atomic_t alive=1;

/**
 * @brief Funzione di terminazione Server (SIGINT/SIGQUIT/SIGTERM)
 */
void terminateServer(){
	alive=0;
	enQueue(coda,KILLTHREAD); //Messaggio speciale di terminazione
	pthread_cond_broadcast(&(coda->cnd1));
}

/**
 * @brief Funzione che stampa le statistiche del Server (SIGUSR1)
 */
void plotStats(){
	FILE * fd;
	fd=fopen(config->StatFileName,"a");
	if(fd){
		pthread_mutex_lock(&mtxstats);
		printStats(fd);
		pthread_mutex_unlock(&mtxstats);
	}
	fclose(fd);
}

/**
 * @brief Funzione che cattura i segnali (TERMINAZIONE + SIGUSR1)
 */
void signalHandler(){

	struct sigaction ignoreSIGPIPE;
	struct sigaction TERMINATE;
	struct sigaction handleSIGUSR;

	memset(&ignoreSIGPIPE,0,sizeof(ignoreSIGPIPE));
	memset(&TERMINATE,0,sizeof(TERMINATE));
	memset(&handleSIGUSR,0,sizeof(handleSIGUSR));

	ignoreSIGPIPE.sa_handler= SIG_IGN;
	TERMINATE.sa_handler = terminateServer;
	handleSIGUSR.sa_handler = plotStats;

	//Ignoro SIGPIPE
	if(sigaction(SIGPIPE,&ignoreSIGPIPE,NULL)==-1){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	//Gestico con un handler: SIGINT, SIGTERM, SIGQUIT
	if(sigaction(SIGINT,&TERMINATE,NULL)==-1){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGTERM,&TERMINATE,NULL)==-1){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGQUIT,&TERMINATE,NULL)==-1){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	//Gestisco SIGUSR1 per stampare le statistiche
	if(sigaction(SIGUSR1,&handleSIGUSR,NULL)==-1){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}

/**
 * @brief Funzione usata dai Thread Worker per gestire le OP
 *
 * La funzione prende il messaggio del client, precedentemente letto dal thread worker
 * e lo switcha fra le varie operazioni possibile, dando una risposta al client secondo
 * le varie casistiche.
 *
 * @param fd filedescriptor del client da servire
 * @param msg il messaggio letto dal socket
 * @return 0 in caso di successo (il Server è riuscito a gestire la richiesta del client)
 * @return -1 in caso di fallimento
 */
int executeReq(int fd, message_t msg){

	//Messaggio di risposta
	message_t reply;

	//Se il sender è nullo OP_FAIL
	if(msg.hdr.sender == NULL || strlen(msg.hdr.sender)==0){
		setHeader(&(reply.hdr),OP_FAIL,"");
		sendHeader(fd,&(reply.hdr));
		return -1;
	}

	op_t op= msg.hdr.op;

	switch(op){ //Vari casi
		case REGISTER_OP:{
			int nOnline=-1;
			char * usrOn;
			if(registerUser(usr,msg.hdr.sender,fd)==0){
				//OK! Nick registrato e connesso
				updateStats(1,1,0,0,0,0,0);
				printf("Registrato e Connesso\n");
				fflush(stdout);

				//Otteniamo ora la lista degli utenti connessi
				nOnline = getOnlineList(usr,&usrOn);
				setHeader(&(reply.hdr), OP_OK, "");
				setData(&(reply.data),"",usrOn,nOnline*(MAX_NAME_LENGTH+1));
			}
			else{//Errore Registrazione
				//Aggiorno le statistiche (operazioni fallite)
				updateStats(0,0,0,0,0,0,1);
				//User già registrato, invio messaggio di ERRORE
				printf("User già registrato\n");
				fflush(stdout);
				setHeader(&(reply.hdr), OP_NICK_ALREADY, "");
				//L'errore comprende anche l'inserimento in hashtable...
			}
			//Invio i messaggi al client
			if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore
				printf("Errore in REGISTER_OP: sendHeader\n");
				return -1;
			}
			if(nOnline>=0){//Se devo inviare anche lista online
				if(sendData(fd,&(reply.data))<=0){
					printf("Errore in REGISTER_OP: sendData\n");
					free(usrOn);
					return -1;
				}
				free(usrOn);
			}
			printf("FINE REGISTER_OP\n");
		}break;

		case CONNECT_OP:{
			int ret;
			int nOnline=-1;
			char * usrOn;
			//Provo a connettermi
			ret=connectUser(usr,msg.hdr.sender,fd);
			if(ret==0){//CONNESSO!
				printf("Connesso!\n");
				fflush(stdout);
				updateStats(0,1,0,0,0,0,0);
				//Otteniamo la lista degli utenti connessi
				nOnline = getOnlineList(usr,&usrOn);
				setHeader(&(reply.hdr), OP_OK, "");
				setData(&(reply.data),"",usrOn,nOnline*(MAX_NAME_LENGTH+1));
			}
			else{//Errore nella connessione
				updateStats(0,0,0,0,0,0,1);
				printf("CONNECT_OP ERROR: ");
				if(ret==-1){
					printf("Utente non registrato\n");
					setHeader(&(reply.hdr),OP_NICK_UNKNOWN,"");
				}
				if(ret==-2){
					printf("Utente già loggato\n");
					setHeader(&(reply.hdr),OP_NICK_ALREADY,"");
				}
			}

			//Invio i messaggi al client
			//Invio header
			if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore, dealloco
				printf("Errore in CONNECT_OP: sendHeader\n");
				return -1;
			}
			if(nOnline>=0){ //Se ho da inviare data invio
				if(sendData(fd,&(reply.data))<=0){
					printf("Errore in CONNECT_OP: sendData\n");
					free(usrOn);
					return -1;
				}
				free(usrOn);
			}
			printf("Fine CONNECT_OP\n");
			fflush(stdout);
		}break;

	    case POSTTXT_OP:{

			message_t new;

			//Controlliamo prima la lunghezza del messaggio
			int len=msg.data.hdr.len;
			if(len>config->MaxMsgSize){ //Messaggio troppo lungo
				printf("Messaggio troppo lungo\n");
				fflush(stdout);
				updateStats(0, 0, 0, 0, 0, 0, 1);
				setHeader(&(reply.hdr), OP_MSG_TOOLONG, "");
				//Invio il messaggio di errore
				if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore, dealloco
					printf("Errore in POSTTXT_OP: sendHeader\n");
					return -1;
				}
				return 0; //Esco
			}

			//Mi faccio restiture il fd del receiver
			int receiver_fd=getUserFD(usr,msg.data.hdr.receiver);
			if(receiver_fd>=0){//Se l'user esiste (e ho/non ho il suo fd)
				//Posto il messaggio nella history comunque
				msg.hdr.op=TXT_MESSAGE;
				if(postOnHistory(usr,&msg)==0){//Se posto con successo
					updateStats(0, 0, 0, 1, 0, 0, 0);
					setHeader(&(reply.hdr),OP_OK,"");
				}
				else{
					printf("Errore in POSTTXT_OP: postOnHistory\n");
					fflush(stdout);
					return -1;
				}
				if(receiver_fd!=0){//Vuol dire che l'user è online, invio direttamente
					//Travasiamo il messaggio
					char * buf = malloc(sizeof(char)*msg.data.hdr.len);
					strncpy(buf,msg.data.buf,msg.data.hdr.len);
					setHeader(&(new.hdr),msg.hdr.op,msg.hdr.sender);
					setData(&(new.data),msg.data.hdr.receiver,buf,msg.data.hdr.len);
					pthread_mutex_lock(&mtx_online);
					if(sendRequest(receiver_fd,&new)==1){
						pthread_mutex_unlock(&mtx_online);
						printf("Messaggio inviato all'utente online!!!\n");
						updateStats(0, 0, 1, -1, 0, 0, 0);
						free(buf); //buf==new.data.buf
					}
					else{
						pthread_mutex_unlock(&mtx_online);
						printf("Errore in POSTTXT_OP: l'user si è scollegato\n");
						free(buf);  //buf==new.data.buf
						fflush(stdout);
					}
				}
			}
			else if(receiver_fd==-1){//L'user non esiste
				updateStats(0, 0, 0, 0, 0, 0, 1);
				setHeader(&(reply.hdr),OP_NICK_UNKNOWN,"");
			}
			//Invio finale messaggio
			if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore, dealloco
				printf("Errore in POSTTXT_OP: sendHeader\n");
				return -1;
			}
			printf("FINE POSTTXT_OP\n");
			fflush(stdout);
		}break;

	    case POSTTXTALL_OP:{
			message_t new;

			//Controlliamo prima la lunghezza del messaggio
			int len=msg.data.hdr.len;
			if(len>config->MaxMsgSize){
				//Messaggio troppo lungo
				printf("Messaggio troppo lungo\n");
				fflush(stdout);
				updateStats(0, 0, 0, 0, 0, 0, 1);
				setHeader(&(reply.hdr), OP_MSG_TOOLONG, "");
			}
			else{//Posso inviare
				msg.hdr.op=TXT_MESSAGE;
				//Invio primo nella history a tutti
				int nposted=postOnHistoryAll(usr,&msg);
				if(nposted>=0){//Se va a buon fine
					//Aggiorno le statistiche
					updateStats(0,0,0,nposted,0,0,0);
					setHeader(&(reply.hdr),OP_OK,"");
				}
				else{//Errore esco.
					printf("Errore in POSTTXTALL_OP: postOnHistoryAll\n");
					return -1;
				}
				/* Ora invio a quelli online*/
				int * fdlist;
				int nOnline=getAllUsersFD(usr,msg.hdr.sender,&fdlist);
				if(nOnline>=0){
					//Travasiamo il messaggio
					char * buf = malloc(sizeof(char)*msg.data.hdr.len);
					strncpy(buf,msg.data.buf,msg.data.hdr.len);
					setHeader(&(new.hdr),msg.hdr.op,msg.hdr.sender);
					setData(&(new.data),msg.data.hdr.receiver,buf,msg.data.hdr.len);
					//Inviamo a tutti gli user online
					for(int i=0; i<nOnline; i++){
						pthread_mutex_lock(&mtx_online);
						if(sendRequest(fdlist[i],&new)==1){ //ignoriamo eventuali users disconnessi
							pthread_mutex_unlock(&mtx_online);
							updateStats(0,0,1,0,0,0,0);
						}
						else{
							pthread_mutex_unlock(&mtx_online);
						}
						printf("Inviato a %d!\n",fdlist[i]);
						fflush(stdout);
					}
					free(new.data.buf);
				}
				free(fdlist);
			}
			//rispondo al client che ne ha fatto richiesta:
			if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore, dealloco
				printf("Errore in POSTTXT_OP: sendHeader\n");
				return -1;
			}
		}break;

	    case POSTFILE_OP:{
			message_data_t new;
			readData(fd,&new);
			//Controllo la lunghezza
			int len=new.hdr.len;
			if(len/1024>config->MaxFileSize){
				//Messaggio troppo lungo
				printf("Messaggio troppo lungo\n");
				fflush(stdout);
				updateStats(0, 0, 0, 0, 0, 0, 1);
				setHeader(&(reply.hdr), OP_MSG_TOOLONG, "");
				free(new.buf);
			}
			else{
				//Mi costruisco il path
				char * filep;
				int len=strlen(config->DirName)+1;
				//Cerco eventuali "./"
				char * bslash;
				bslash=strrchr(msg.data.buf,'/');
				if(bslash){
					bslash++;
					len=len+strlen(bslash)+1;
				}
				else{
					len=len+msg.data.hdr.len;
				}
				filep=malloc(sizeof(char)*len);
				strncpy(filep,config->DirName,strlen(config->DirName)+1);
				strncat(filep,"/",2);
				if(bslash) strncat(filep,bslash,strlen(bslash)+1);
				else strncat(filep,msg.data.buf,msg.data.hdr.len);

				//File pronto!
				FILE * fsend;
				fsend=fopen(filep,"wb");
				if(fsend){//Se riesco ad aprirlo
					free(filep);
					//Scrivo
					int wt;
					wt = fwrite(new.buf,sizeof(char),new.hdr.len,fsend);
					fflush(fsend);
					if(wt!=new.hdr.len){
						printf("Damn...\n");
						fflush(stdout);
						fclose(fsend);
						free(new.buf);
						return -1; //Errore
					}
					//Sono riuscito a scrivere!
					free(new.buf);
					fclose(fsend);
					//Mi faccio dare il fd del receiver
					int receiver_fd=getUserFD(usr,msg.data.hdr.receiver);
					if(receiver_fd>=0){//Se l'user esiste (e ho/non ho il suo fd)
						//Posto il messaggio nella history comunque
						msg.hdr.op=FILE_MESSAGE;
						if(postOnHistory(usr,&msg)==0){//Se posto con success
							printf("FILE postato nella History\n");
							fflush(stdout);
							updateStats(0, 0, 0, 0, 0, 1, 0);
							setHeader(&(reply.hdr),OP_OK,"");
						}
						else{
							printf("Errore in POSTFILE_OP: postOnHistory\n");
							fflush(stdout);
						}
						if(receiver_fd!=0){//Vuol dire che l'user è online, invio direttamente
							message_t new1;
							setHeader(&(new1.hdr),msg.hdr.op,msg.hdr.sender);
							setData(&(new1.data),msg.data.hdr.receiver,msg.data.buf,msg.data.hdr.len);
							printMsg(&new1);
							pthread_mutex_lock(&mtx_online);
							if(sendRequest(receiver_fd,&new1)==1){
								pthread_mutex_unlock(&mtx_online);
								printf("FILE inviato direttamente\n");
								fflush(stdout);
								updateStats(0, 0, 0, 0, 1, -1, 0);
							}
							else{
								pthread_mutex_unlock(&mtx_online);
								printf("Errore in POSTTXT_OP: sendRequest\n");
								fflush(stdout);
							}
						}
					}
					else if(receiver_fd==-1){//L'user non esiste
						updateStats(0, 0, 0, 0, 0, 0, 1);
						setHeader(&(reply.hdr),OP_NICK_UNKNOWN,"");
					}
				}
				else{//Fail! Non sono riuscito ad aprirlo
					perror("Open file in POSTFILE_OP");
					free(new.buf);
					free(filep);
					return -1;
				}

			}
			//Invio il messaggio di risposta
			if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore, dealloco
				printf("Errore in POSTTXT_OP: sendHeader\n");
				return -1;
			}
		}break;

	    case GETFILE_OP:{
			//Costruisco il path dal mex
			char * filep;
			int len=strlen(config->DirName)+1+strlen(msg.data.buf)+1;
			filep=malloc(sizeof(char)*len);
			strncpy(filep,config->DirName,strlen(config->DirName)+1);
			strncat(filep,"/",strlen("/")+1);
			strncat(filep,msg.data.buf,strlen(msg.data.buf)+1);

			//Lo mappo in memoria
			int f = open(filep, O_RDONLY);
			if(f<0){//Se non riesco ad aprire il file: mando errore
				setHeader(&(reply.hdr),OP_NO_SUCH_FILE,"");
				if(sendHeader(fd,&(reply.hdr))<=0){ //Errore
					free(filep);
					return -1;
				}
			}
			else{
				char * filem;
				struct stat st;
		        if (stat(filep, &st) == -1 || !S_ISREG(st.st_mode)) {
					updateStats(0, 0, 0, 0, 0, 0, 1);
					setHeader(&(reply.hdr), OP_NO_SUCH_FILE, "");
					if(sendHeader(fd, &(reply.hdr))<= 0){
						close(f);
						free(filep);
						return -1;
					}
		        }

		        // Leggo il file
		   		filem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, f, 0);

		        // Errore mmap mando errore al client
				if (filem == MAP_FAILED) {
					close(f);
					updateStats(0, 0, 0, 0, 0, 0, 1);
					setHeader(&(reply.hdr), OP_NO_SUCH_FILE, "");
					if(sendHeader(fd, &(reply.hdr))<=0){
						free(filep);
						return -1;
					}
		        }
		        // File pronto per essere inviato
				else{
					close(f);
					updateStats(0, 0, 0, 0, 1, -1, 0);
					// Invio header
					setHeader(&(reply.hdr), OP_OK, "");
					if(sendHeader(fd, &(reply.hdr))<=0){
						free(filep);
						return -1;
	          		}

					// Invio dati
					setData(&(reply.data), "", filem, st.st_size);
					if(sendData(fd, &(reply.data))<0){
						free(filep);
						return -1;
					}
					munmap(filem,st.st_size);
					free(filep);
		        }
		      }
			  printf("FINE OP GETFILE SENDER[%s]\n", msg.hdr.sender);
			  fflush(stdout);
		}break;

	    case GETPREVMSGS_OP:{
			msgqueue_t * ret=getHistory(usr,msg.hdr.sender);
			if(ret){//Se va a buon fine
				size_t nummsg= ret->size;
				setHeader(&(reply.hdr), OP_OK, "");
				setData(&(reply.data),"",(char *)&nummsg,sizeof(size_t));
				//Invio l'esito che siamo pronti ad inviare altri messsaggi
				if(sendRequest(fd,&reply)<=0){ //Se c'è un errore nell'invio esco
					printf("Errore in GETPREVMSGS_OP: sendHeader\n");
					return -1;
				}
				if(ret->size>0){
					msgnode_t * curr = ret->head;
					while(curr){
						printMsg(curr->msg);
						if(sendRequest(fd,curr->msg)<=0){
							printf("Errore in GETPREVMSGS_OP: sendHeader\n");
							return -1;
						}
						curr=curr->next;
					}
				}
				destroyMsgQueue(ret);
			}
			else{//Non c'è nessuna lista
				setHeader(&(reply.hdr), OP_FAIL, "");
				if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore
					printf("Errore in GETPREVMSGS_OP: sendHeader\n");
					return -1;
				}
			}
			//Tutto andato a buon fine
		}break;

	    case USRLIST_OP:{
			int nOnline=-1;
			char * usrOn;
			nOnline = getOnlineList(usr,&usrOn);
			setHeader(&(reply.hdr), OP_OK, "");
			setData(&(reply.data),"",usrOn,usr->usersOnline*(MAX_NAME_LENGTH+1));
			//Invio i messaggi al client
			if(sendHeader(fd,&(reply.hdr))<=0){ //Se c'è un errore, dealloco
				printf("Errore in USRLIST_OP: sendHeader\n");
				free(usrOn);
				return -1;
			}
			if(nOnline!=-1){
				if(sendData(fd,&(reply.data))<=0){
					printf("Errore in USRLIST_OP: sendData\n");
					free(usrOn);
					return -1;
				}
				free(usrOn);
			}
		}break;

	    case UNREGISTER_OP:{
			if(unregisterUser(usr,msg.hdr.sender,fd)==0){
				//Aggiorno le statistiche
				updateStats(-1,-1,0,0,0,0,0);
				//Setto messaggio di risposta
				setHeader(&(reply.hdr),OP_OK,"");
			}
			else{//User non registrato
				updateStats(0,0,0,0,0,0,1);
				setHeader(&(reply.hdr),OP_NICK_UNKNOWN,"");
			}
			//Invio il messaggio
			if(sendHeader(fd, &(reply.hdr))<=0)	return -1;
		}break;

	    case DISCONNECT_OP:{
			if(disconnectUser(usr,msg.hdr.sender,0)==0){
				//Disconnessione avvenuta!
				//Aggiorno le statistiche
				updateStats(0,-1,0,0,0,0,0);
				setHeader(&(reply.hdr),OP_OK,"");
			}
			else{//Gestisce assieme -1 (non online) -2 (non registrato)
				updateStats(0,0,0,0,0,0,1);
				setHeader(&(reply.hdr),OP_NICK_UNKNOWN,"");
			}
			//Invio il messaggio
			if(sendHeader(fd, &(reply.hdr))<=0)	return -1;
			//Andata a buon fine
		}break;

		default:{
			//Operazione sconosciuta
			setHeader(&(reply.hdr),OP_FAIL,"");
			sendHeader(fd,&(reply.hdr));
			return -1; //Comunico al chiamante che deve disconnettere
		}
	}
	return 0; //Tutto andato a buon fine!
}

/**
 * @brief Funzione passata ai thread worker
 *
 * La funzione esegue un ciclo infinito (finchè non viene interrotto da uno dei segnali mascherati)
 * dentro il quale estrae da una coda concorrente i file descriptor dei client che devono essere serviti.
 * Appena estratto, legge dal socket l'intero messaggio e lo passa alla funzione executeReq che lo processa.
 * In base all'esito della funzione decide se rimettere il tutto dentro al set dei fd da servire,
 * oppure di disconnettere il client
 */
void * worker(void * arg){
	message_t new;

	while(alive){

		int client=deQueue(coda); //Si blocca sennò
		if(!alive) break; //Terminazione thread
		if(client==-1) continue; //cuncurrency
		printf("*------@START@------*\n");
		printf("Client: %d\n",client);

		if(readMsg(client,&new)==1){ //Lettura andata a buon fine
			printMsg(&new);
			//Servo la richiesta del client
			int esito=executeReq(client,new);
			free(new.data.buf);
			new.data.buf=NULL;
			//Controllo l'esito della richiesta
		    if(esito==0){ //Se è andata a buon fine, metto in coda il client!
				printf("@OK: Servito\n\n");
				fflush(stdout);
				pthread_mutex_lock(&mtx_set);
				FD_SET(client,&set);
				pthread_mutex_unlock(&mtx_set);
			}
		    else{ //Esito negativo, disconnetto il client
				printf("@ERR: Client non servito!\n\n");
				fflush(stdout);
				if(disconnectUser(usr,NULL,client)==0) updateStats(0,-1,0,0,0,0,0);
				close(client);
			}
		}
		else{ //Il client si è disconnesso
			if(disconnectUser(usr,NULL,client)==0) updateStats(0,-1,0,0,0,0,0);
			close(client);
		}
		printf("*-------@END@-------*\n\n");
	}
	return NULL;
}

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f config\n", progname);
	exit(EXIT_FAILURE);
}

int main(int argc, char * argv[]){

	printf("...Starting Chatty...\n\n");

	char * conffile=NULL;
	int opt;

	if(argc != 3){usage(argv[0]);}

	while((opt = getopt(argc, argv, "f:"))!= -1){
		switch(opt){
			case 'f':
				conffile = strdup(optarg);
				break;
			default:
				usage(argv[0]);
		}
	}
	//Ottenuto il file di configurazione, lo contralliamo e lo parsiamo
	config = parse(conffile);
	free(conffile);

	//Avvio l'handler che gestisce i segnali
	signalHandler();

	//creo coda per i fd
	coda=createQueue(config->MaxConnections);
	if(!coda) exit(EXIT_FAILURE);

	//creo struttura per registrare utenti
	usr = createUsersStruct(config->MaxHistMsgs,config->MaxConnections);
	if(!usr) exit(EXIT_FAILURE);

	//Scollego vecchio socket
	unlink(config->UnixPath);

	//Preparo i fd per la comunicazione con il socket
	int fd_sk;
	int fd_c;
	int fd_num=0;
	int fd;
	fd_set rdset;
	int notused;

	//Creiamo il socket
	fd_sk=socket(AF_UNIX,SOCK_STREAM,0);
	if(fd_sk<0){perror("socket in main()"); exit(EXIT_FAILURE);} //TERMINO in caso di errore
	struct sockaddr_un address;
	memset(&address,'0',sizeof(address));
	address.sun_family=AF_UNIX;
	strncpy(address.sun_path,config->UnixPath,strlen(config->UnixPath)+1);

	notused=bind(fd_sk,(struct sockaddr*)&address,sizeof(address));
	if(notused==-1){perror("bind");exit(EXIT_FAILURE);}
	notused=listen(fd_sk,config->MaxConnections);
	if(notused==-1){perror("listen");exit(EXIT_FAILURE);}

	FD_ZERO(&set);
	FD_ZERO(&rdset);
	FD_SET(fd_sk,&set);

	if(fd_sk>fd_num) fd_num=fd_sk;

	//Creo il pool di thread worker
	pool * pool=createPool(config->ThreadsInPool);
	initPool(pool,&worker);

	while(alive){

		//Necessario ridefinire il timeout
		struct timeval timeout={0,150};

		//aggiorno
		pthread_mutex_lock(&mtx_set);
		rdset=set;
		pthread_mutex_unlock(&mtx_set);
		int notused;
		notused=select(fd_num+1,&rdset,NULL,NULL,&timeout);
		if(notused<0){continue;}

		//Scandisco la maschera
		for(fd=0; fd<=fd_num; fd++){
			if(FD_ISSET(fd,&rdset)){
				if(fd==fd_sk){

					// Controlliamo se non supera il massimo di connessione
					int ok = 1;
					pthread_mutex_lock(&mtxstats);
					if( chattyStats.nonline >= config->MaxConnections ) ok = 0;
					pthread_mutex_unlock(&mtxstats);
	  				if( !ok ) continue ;

					// Ok! Creo il file descriptor
					fd_c=accept(fd_sk,NULL,NULL);
					pthread_mutex_lock(&mtx_set);
					FD_SET(fd_c,&set);
					pthread_mutex_unlock(&mtx_set);
					if(fd_c>fd_num) fd_num=fd_c;
				}
				else{
					pthread_mutex_lock(&mtx_set);
					FD_CLR(fd,&set);
					pthread_mutex_unlock(&mtx_set);
					enQueue(coda,fd);
				}
			}
		}
	}
	printf("Termino MAIN\n");
	for (int i = 0; i < config->ThreadsInPool; i++) {
      printf("*Thread %d terminated\n", i);
      pthread_join(pool->thread[i], NULL);
  	}

	printf("Cleaning up...\n");
	destroyQueue(coda);
	destroyPool(pool);
	destroyUsersStruct(usr);
	free(config->UnixPath);
	free(config->DirName);
	free(config->StatFileName);
	free(config);
	fflush(stdout);

	return 0;
}
