/**
 * @file parser.h
 * @author Stefano Spadola 534919
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 * @brief Implementazione di una struttura per le variabili di configurazione
 *		  + funzione per parsare da file i valori
 */
#if !defined(PARSER_H)
#define PARSER_H_

/**
 * @struct conf_var
 * @brief struttura per memorizzare variabili di configurazione

 * @var conf_var::
 * UnixPath path creazione socket
 * @var conf_var::MaxConnections
 * numero massimo di connessioni simultanee
 * @var conf_var::ThreadsInPool
 * numero di thread del server
 * @var conf_var::MaxMsgSize
 * dimensione massima dei messaggi
 * @var conf_var::MaxFileSize
 * size massima per i file
 * @var conf_var::MaxHistMsgs
 * size massima per history messaggi
 * @var conf_var::DirName
 * cartella dove salvare file di statistiche
 * @var conf_var::StatFileName
 * nome file di statistiche
 */
typedef struct confvar{
	char * UnixPath;
	int MaxConnections;
	int ThreadsInPool;
	int MaxMsgSize;
	int MaxFileSize;
	int MaxHistMsgs;
	char * DirName;
	char * StatFileName;
}conf_var;

/**
 * @brief Funzione di supporto a "parse" che serve a estrapolare sottostringhe
 */
void trova_val(char * buf, int i, int type, int scanned);

/**
 * @brief Estrapola le variabili di configurazione del server parsandole da file
 *
 * @param conffile
 * @return conf_var (struttura contenente le variabili)
 */
conf_var * parse(char * conffile);

#endif
