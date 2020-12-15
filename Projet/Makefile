CC = gcc
OPTIONS = -Wall

all: initial archiviste journaliste

initial: initial.c
	$(CC) $(OPTIONS) initial.c -o initial

archiviste: archiviste.c include.h
	$(CC) $(OPTIONS) archiviste.c include.h -o archiviste

journaliste: journaliste.c include.h
	$(CC) $(OPTIONS) journaliste.c include.h -o journaliste
clean:
	rm initial archiviste journaliste cle.serv
	#ipcrm --all
