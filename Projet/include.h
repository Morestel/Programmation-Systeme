#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>



#define FICHIER_CLE "cle.serv"
#define LETTRE_CODE 'a'



/* Couleurs dans xterm */
#define couleur(param) printf("\033[%sm",param)

#define NOIR  "30"
#define ROUGE "31"
#define VERT  "32"
#define JAUNE "33"
#define BLEU  "34"
#define CYAN  "36"
#define BLANC "37"
#define REINIT "0"

// Issu de types.h
typedef struct
{
    long type;
    int theme; // 
    int numero_article; // Numéro de l'article
    char nature; // Nature de la requête (c, p, e)
    char *texte_article; // Contenu de l'article
    pid_t expediteur;
} 
requete_t;

typedef struct
{
    long type;
    char texte[5];
    char texte_erreur[100];
    int code_archiviste;
} 
reponse_t;

#define MAX_ARTICLE 100
// Tableau des thèmes, chaque case représente un tableau qui est un thème. Un thème est défini par son numéro (indice) et son contenu (ce qu'il contient)
typedef char tab_article[MAX_ARTICLE][5];