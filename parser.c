/**
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 * @brief Implementazione di una struttura per le variabili di configurazione
 *		  + funzione per parsare da file i valori
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include <sys/types.h>

#define N 1024 //Supponiamo un massimo di 1024 caratteri per stringa

//Array di appoggio che ci dice se abbiamo tutti i valori richiesti per avviare il server
int found[8]={0,0,0,0,0,0,0,0};
conf_var * config;

/**
 * @brief Funzione di supporto a "parse" che serve a estrapolare sottostringhe
 */
void trova_val(char * buf, int i, int type, int scanned){

	char * tmp = malloc(sizeof(char)*N);
	int j=0; //Dimensione finale del valore
	int listen=0; //Se =1, il ciclo si mette in ascolto del valore

	while(i<scanned){
		if(buf[i]=='='){listen=1;}
		else if(buf[i]!=' ' && buf[i]!=9 && buf[i]!='\n' && listen){
			tmp[j]=buf[i];
			j++;
		}
		i++;
	}
	tmp[j]='\0';
	j++;

	//Se il valore c'è effettivamente riempe il campo corrispettivo
	if(listen==1){
		switch(type){
			case 0 :
				config->UnixPath=malloc(sizeof(char)*j);
				memset(config->UnixPath, 0, j*sizeof(char));
				strncpy(config->UnixPath, tmp, j);
				found[0]=1;
				break;
			case 1 :
				config->MaxConnections=atoi(tmp);
				found[1]=1;
				break;
			case 2 :
				config->ThreadsInPool=atoi(tmp);
				found[2]=1;
				break;
			case 3 :
				config->MaxMsgSize=atoi(tmp);
				found[3]=1;
				break;
			case 4 :
				config->MaxFileSize=atoi(tmp);
				found[4]=1;
				break;
			case 5 :
				config->MaxHistMsgs=atoi(tmp);
				found[5]=1;
				break;
			case 6 :
				config->DirName=malloc(sizeof(char)*j);
				memset(config->DirName, 0, j*sizeof(char));
				strncpy(config->DirName, tmp, j);
				found[6]=1;
				break;
			case 7 :
				config->StatFileName=malloc(sizeof(char)*j);
				memset(config->StatFileName, 0, j*sizeof(char));
				strncpy(config->StatFileName, tmp, j);
				found[7]=1;
				break;
		}
	}
	free(tmp);
}

/**
 * @brief Estrapola le variabili di configurazione del server parsandole da file
 *
 * @param conffile
 * @return conf_var (struttura contenente le variabili)
 */
conf_var * parse(char * conffile){

	config = malloc(sizeof(conf_var));

	FILE *fd;
	if((fd=fopen(conffile,"r"))==NULL){
		perror("Errore apertura conffile");
		exit(EXIT_FAILURE);
	}

	char  * buffer=NULL;
	size_t len = 0;
	ssize_t scanned;
	int i=0;

	while((scanned = getline(&buffer,&len,fd))!=-1){
		if(buffer[0]!='#' && buffer[0]!='\n' && buffer[0]!='\0' && buffer[0]!=EOF){
			i=0;
			while(i<scanned){
				if(buffer[i]==' ' || buffer[i]==9) break;
				else i++;
			}
			if(strncmp(buffer,"UnixPath",i)==0){ trova_val(buffer,i,0,scanned); }
			else if(strncmp(buffer,"MaxConnections",i)==0){ trova_val(buffer,i,1,scanned); }
			else if(strncmp(buffer,"ThreadsInPool",i)==0){ trova_val(buffer,i,2,scanned); }
			else if(strncmp(buffer,"MaxMsgSize",i)==0){ trova_val(buffer,i,3,scanned); }
			else if(strncmp(buffer,"MaxFileSize",i)==0){ trova_val(buffer,i,4,scanned); }
			else if(strncmp(buffer,"MaxHistMsgs",i)==0){ trova_val(buffer,i,5,scanned); }
			else if(strncmp(buffer,"DirName",i)==0){ trova_val(buffer,i,6,scanned); }
			else if(strncmp(buffer,"StatFileName",i)==0){ trova_val(buffer,i,7,scanned); }
		}
	}
	free(buffer);
	int z=0;
	while(found[z]==1){
		z++;
	}
	if(z==8){
		fclose(fd);
		return config;
	}
	else{
		fclose(fd);
		free(config);
		printf("Struttura di conffile errata o incompleta");
		exit(EXIT_FAILURE);
	}
}
