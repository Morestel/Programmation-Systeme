#include "include.h"

int mem_part;
int semap;
int file_mess;

// Fonction usage
void usage(char *s){
    printf("Usage : %s <nb_archiviste> <nb_themes>\n", s);
    printf("nb_archiviste >= 2\nnb_themes >=2\n");
    exit(EXIT_FAILURE);
}

// Sigaction
void mon_sigaction(int signal, void (*f)(int)){
    struct sigaction action;
    /* Routine de traitement du signal */
    action.sa_handler = f;
    /* Pas de signaux supplementaires a bloquer */
    sigemptyset(&action.sa_mask);
    /* Pas d'options*/
    action.sa_flags = 0;
    /* Positionnement de la capture du signal */
    sigaction(signal,&action,NULL);
}

// Fonction d'arrêt à la suite d'un signal: Supprime les IPC / Sémaphores / Segment mémoire partagée
void arret(int s){
    couleur(ROUGE);
    printf("Signal de fin reçu\n");
    couleur(REINIT);
    semctl(semap,1,IPC_RMID,NULL);
    shmctl(mem_part,IPC_RMID,NULL);
    msgctl(file_mess,IPC_RMID, NULL);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]){
    int nb_archivistes, nb_themes;
    pid_t pid = getpid();
    key_t cle;
    struct stat st;
    int *tab; // Entier du SMP
    sigset_t  masque_attente;

    requete_t requete;
    //reponse_t reponse;
    int nb_lus;

    struct sembuf P = {0,-1,SEM_UNDO};
    struct sembuf V = {0,1,SEM_UNDO};
    
    if (argc != 3)
        usage(argv[0]);

    nb_archivistes = atoi(argv[1]);
    nb_themes = atoi(argv[2]);
    nb_archivistes++;
    nb_themes++;
    /* Preparation du masque du sigsuspend,                   */
    /* Seuls SIGUSR1 et SIGUSR2 seront demasques              */
    sigfillset(&masque_attente);
    sigdelset(&masque_attente,SIGUSR1);
    sigdelset(&masque_attente,SIGUSR2);

    mon_sigaction(SIGUSR1, arret);
    mon_sigaction(SIGUSR2, arret);
    mon_sigaction(SIGINT, arret);
    mon_sigaction(SIGSTOP, arret);
    mon_sigaction(SIGTSTP, arret);

    sigemptyset(&masque_attente);

    /* Creation de la cle :          */
        /* 1 - On teste si le fichier cle existe dans le repertoire courant : */
    if ((stat(FICHIER_CLE,&st) == -1) &&
	(open(FICHIER_CLE, O_RDONLY | O_CREAT | O_EXCL, 0660) == -1)){
	    fprintf(stderr,"Pb creation fichier cle\n");
	    exit(-1);
    }

    cle = ftok(FICHIER_CLE,LETTRE_CODE);
    if (cle==-1){
	    printf("Pb creation cle\n");
	    exit(-1);
    }

    // Récupération du SMP
    mem_part = shmget(cle,sizeof(int),0);
    if (mem_part==-1){
	    printf("(%d) Pb recuperation SMP\n",pid);
	    exit(-1);
    }

    // Attachement du SMP
    tab = shmat(mem_part,NULL,0);
    if (tab==(int *)-1){
	printf("(%d) Pb attachement SMP\n",pid);
	exit(-1);
    }

    // Récupération des sémaphores
    semap = semget(cle,1,0);
    if (semap==-1){
	    printf("(%d) Pb recuperation semaphore\n",pid);
	    exit(-1);
    }

    // Récupération de la file de message
    file_mess = msgget(cle,0);
	if (file_mess ==-1){
	    printf("(%d) Pb recuperation file de message\n",pid);
	    exit(-1);
    }

    // Execution de la boucle infinie
    for(;;){

        // Attente des requêtes
        if ((nb_lus = msgrcv(file_mess, &requete, sizeof(requete_t), 1, 0)) == -1){
            printf("Erreur de lecture, erreur numéro %d\n", errno);
            raise(SIGUSR1);
        }

        // On a reçu la requête (normalement) - On la traite
        sleep(1); // Simulation de travail
    }
}