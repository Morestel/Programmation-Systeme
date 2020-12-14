#include "include.h"

// Nécessairement globale pour les supprimer à la terminaison

int file_mess; // ID de la file
int mem_part; // ID du segment de mémoire partagée
int semap; // ID sémaphore


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
    int nb_themes, nb_archivistes;
    int i;
    sigset_t  masque_attente;

    key_t cle;
    pid_t pid_archive;
    pid_t pid_journaliste;
    struct stat st;
    int res_init;
    unsigned short val_init[1] = {1};
    int *tab;
    // Demande aux archives: 10 éléments parmi lesquels: 7 consultations, 2 publications, 1 effacement
    // On piochera aléatoirement dedans
    char demande_archive[10] = {'c', 'c', 'c', 'c', 'c', 'c', 'c',
                                'p', 'p', 
                                'e'};
    int nombre_aleatoire; // Nombre aléatoire entre 0 et 9

    char param_nb_archiviste[20];
    char arg2[20];
    char numero_theme[3];
    char numero_article[3];
    //char param_texte_article[4];
   
    // Initialisation de srand, utilisée pour les demandes aux archives
    srand(time(NULL)); 
    // Test du nombre d'arguments
    if (argc != 3){
        usage(argv[0]);
    }

    // Récupération des paramètres
    nb_archivistes = atoi(argv[1]);
    nb_themes = atoi(argv[2]);

    // Test de la véracité des arguments
    if (nb_archivistes < 2 || nb_themes  < 2){
        usage(argv[0]);
    }

    /* Preparation du masque du sigsuspend,                   */
    sigfillset(&masque_attente);
    sigdelset(&masque_attente,SIGUSR1);
    sigdelset(&masque_attente,SIGUSR2);
    sigdelset(&masque_attente, SIGINT);
    sigdelset(&masque_attente, SIGSTOP);
    sigdelset(&masque_attente, SIGTSTP);
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

    /* On cree le SMP et on teste s'il existe deja :    */
    mem_part = shmget(cle,sizeof(int),IPC_CREAT | IPC_EXCL | 0660);
    if (mem_part==-1){
	    printf("Pb creation SMP ou il existe deja\n");
	    exit(-1);
    }
    
    /* Attachement de la memoire partagee :          */
    tab = shmat(mem_part,NULL,0);
    if (tab==(int *) -1){
	    printf("Pb attachement\n");
	/* Il faut detruire le SMP puisqu'on l'a cree : */
	    shmctl(mem_part,IPC_RMID,NULL);
	    exit(-1);
    }

    /* On cree le semaphore (meme cle) :                     */
    semap = semget(cle,1,IPC_CREAT | IPC_EXCL | 0660);
    if (semap==-1){
	    printf("Pb creation semaphore ou il existe deja\n");
	/* Il faut detruire le SMP puisqu'on l'a cree : */
	    shmctl(mem_part,IPC_RMID,NULL);
	/* Le detachement du SMP se fera a la terminaison */
	    exit(-1);
    }

    /* On l'initialise :                                     */
    res_init = semctl(semap,1,SETALL,val_init);
    if (res_init==-1){
	    printf("Pb initialisation semaphore\n");
	/* On detruit les IPC deje crees : */
	    semctl(semap,1,IPC_RMID,NULL);
	    shmctl(mem_part,IPC_RMID,NULL);
	    exit(-1);
    }     

    // Tout est bon on initialise tab
    tab[0] = 0; 

    file_mess = msgget(cle,IPC_CREAT | IPC_EXCL | 0660);
    if (file_mess==-1){
        fprintf(stderr,"Pb creation file de message\n");
        semctl(semap,1,IPC_RMID,NULL);
        shmctl(mem_part,IPC_RMID,NULL);
        exit(-1);
    }

    // Création des archivistes
    printf("CREATION ARCHIVISTE DAB DAB\n");
    for(i = 0; i < nb_archivistes; i++){
        pid_archive = fork();
        switch(pid_archive){
            case -1:
                break;
            case 0:
                sprintf(param_nb_archiviste, "%d", nb_archivistes);
                sprintf(arg2, "%d", nb_themes);
                execlp("./archiviste", "archiviste", param_nb_archiviste, arg2, NULL);
                perror("Création des archivistes "); // En principe jamais atteint
                break;
        }
    }
    printf("CREATION JOURNALISTE LMAO\n");
    // Création d'une infinité de journalistes 
    while(1){
        // Choix d'un nombre aléatoire entre 0 et 9
        nombre_aleatoire = rand()%10;
        
        // Création des fils
        pid_journaliste = fork();
        sprintf(param_nb_archiviste, "%d", nb_archivistes);
        sprintf(arg2, "%d", nb_themes);
        switch(pid_journaliste){
            case -1:
                break;
            case 0: // Fils
            
            // Choix dans la demande d'archive
            switch(demande_archive[nombre_aleatoire]){
                
                case 'c': // Consultation
                    sprintf(numero_theme, "%d", 0);
                   //sprintf(mode_acces, "%c", 'c');
                    sprintf(numero_article, "%d", 1);
                    execlp("./journaliste", "journaliste", param_nb_archiviste, 'c', numero_theme, numero_article, NULL);
                    printf("JOURNALISTE C FAIT MON SAC EST FAIT\n");
                    break;

                case 'p': // Création
                    sprintf(numero_theme, "%d", 0);
                    //sprintf(param_texte_article, "%s", "truc");
                    execlp("./journaliste", "journaliste", param_nb_archiviste, 'p', numero_theme, "truc"/*param_texte_article*/, NULL);
                    break;

                case 'e': // Effacement
                    sprintf(numero_theme, "%d", 0);
                   //sprintf(mode_acces, "%c", 'c');
                    sprintf(numero_article, "%d", 1);
                    execlp("./journaliste", "journaliste", param_nb_archiviste, 'e', numero_theme, numero_article, NULL);
                    break;
                
                default:
                    printf("Problème de liaison avec notre journaliste\n");
                    exit(-1);
                    break;
            }
            break;
            exit(-1); // Jamais atteint (normalement)
        }
    }
    exit(0); // Jamais atteint
}   