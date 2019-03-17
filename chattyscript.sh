#Stefano Spadola 534919
#Si dichiara che il contenuto di questo file e' in ogni sua parte opera originale dell'autore
#brief Contiene lo script che estrae dal file di configurazione la cartella
#!/bin/bash

if [[ $# == 0 || $# != 2 ]]; then
    echo "use $0 <chattyconfx> <time>"
    exit 1
fi

for i in "$@"; do
	if [[ $i == --help ]]; then
		echo "use $0 <chattyconfx> <time>"
		exit 1
	fi
done

if ! [[ -e $1 ]]; then
	echo "$1"
	exit 1
fi

exec 3<$1

while read -u 3 line; do
	unset string
	I=0
	for i in $line; do
		string[$I]=$i
		(( I++ ))
	done
	if [[ ${string[0]} == DirName ]]; then
		if [[ $2 == 0 ]]; then
			cd ${string[2]}
			ls
		else
			find ${string[2]} -type f -mmin +$2 -exec rm {} \;
		fi
	fi
done

exit 0
