#include "include.h"

// Nécessairement globale pour les supprimer à la terminaison

int file_mess; // ID de la file
int mem_part; // ID du segment de mémoire partagée
int semap; // ID sémaphore

tab_article *tab_theme;
int *file_attente;
int *nb_article;

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
   
    semctl(semap,1,IPC_RMID,NULL);
    shmctl(mem_part,IPC_RMID,NULL);
    msgctl(file_mess,IPC_RMID, NULL);
    shmdt(file_attente);
    shmdt(tab_theme);
    shmdt(nb_article);
    exit(EXIT_SUCCESS);
}

// Génère un texte aléatoire de 4 lettres
void texte_aleatoire(char *s){
    int i;
    char lettre;
    
    for (i = 0; i < 4; i++){
        lettre = rand() % ('z' - 'a' + 1) + 'a';
        s[i] = lettre;
    }
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
    // Demande aux archives: 10 éléments parmi lesquels: 7 consultations, 2 publications, 1 effacement
    // On piochera aléatoirement dedans
    char demande_archive[] = {'c', 'c', 'c', 'c', 'c', 'c', 'c',
                              'p', 'p', 
                              'e'};
    int nombre_aleatoire; // Nombre aléatoire entre 0 et 9
    int theme_aleatoire; // Nombre entre 0 et nb_theme-1
    int article_aleatoire; // Nombre entre 0 et MAX_ARTICLE - 1
    char param_nb_archiviste[20];
    char nom_de_code[20];
    char param_nb_theme[20];
    char numero_theme[3];
    char numero_article[3];
    char lettre_c[1] = {'c'};
    char lettre_p[1] = {'p'};
    char lettre_e[1] = {'e'};
    char param_texte_article[5]; // Paramètre texte article, on prendra aaaa par défaut
    
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
   
    // Attachement du tableau des thèmes
    tab_theme = shmat(mem_part,NULL,0);
    if (tab_theme == (void*)-1){
	    printf("Pb attachement\n");
	    shmctl(mem_part,IPC_RMID,NULL);
	    exit(-1);
    }


     // Attachement du nombre d'articles
    nb_article = shmat(mem_part,NULL,0);
    if (nb_article == (int*)-1){
	    printf("Pb attachement\n");
	    shmctl(mem_part,IPC_RMID,NULL);
	    exit(-1);
    }
    // Attachement de la file d'attente
    file_attente = shmat(mem_part,NULL,0);
    if (file_attente==(int *) -1){
	    printf("Pb attachement\n");
	/* Il faut detruire le SMP puisqu'on l'a cree : */
	    shmctl(mem_part,IPC_RMID,NULL);
	    exit(-1);
    }

    /* On cree le semaphore (meme cle) :                     */
    semap = semget(cle, 1, IPC_CREAT | IPC_EXCL | 0660); 
    if (semap==-1){
	    printf("Pb creation semaphore ou il existe deja\n");
	/* Il faut detruire le SMP puisqu'on l'a cree : */
	    shmctl(mem_part,IPC_RMID,NULL);
	/* Le detachement du SMP se fera a la terminaison */
	    exit(-1);
    }
    

    /* On l'initialise :                                     */
    res_init = semctl(semap,1, SETALL, val_init);
    if (res_init==-1){
	    perror("Initialisation semctl");
	/* On detruit les IPC deje crees : */
	    semctl(semap,1,IPC_RMID,NULL);
	    shmctl(mem_part,IPC_RMID,NULL);
	    exit(-1);
    }     
    
    // Tout est bon on initialise nb_article et file d'attente
 
    file_attente[0] = 0;
    nb_article[0] = 0;
    
    // Création de la file de messages
    file_mess = msgget(cle, IPC_CREAT | IPC_EXCL | 0660);
    if (file_mess==-1){
        fprintf(stderr,"Pb creation file de message\n");
        semctl(semap,1,IPC_RMID,NULL);
        shmctl(mem_part,IPC_RMID,NULL);
        exit(-1);
    }

    // Création des archivistes
    for(i = 0; i < nb_archivistes; i++){
        pid_archive = fork();
        switch(pid_archive){
            case -1:
                break;
            case 0:
                sprintf(nom_de_code, "%d", i);
                sprintf(param_nb_theme, "%d", nb_themes);
                execlp("./archiviste", "archiviste", nom_de_code, param_nb_theme, NULL);
                perror("Création des archivistes "); // En principe jamais atteint
                break;
        }
    }
    // Création d'une infinité de journalistes 
    
    while(1){
        
        // Choix d'un nombre aléatoire entre 0 et 9
        nombre_aleatoire = rand()%10;
        // Choix d'un nombre aléatoire compris entre 0 et nb_themes - 1
        theme_aleatoire = rand() % nb_themes;
        // CHoix d'un nombre aléatoire compris entre 0 et MAX_ARTICLE - 1
        article_aleatoire = rand() % MAX_ARTICLE;

        sprintf(param_nb_archiviste, "%d", nb_archivistes);
        sprintf(param_nb_theme, "%d", nb_themes);
        sprintf(numero_theme, "%d", theme_aleatoire);
        sprintf(numero_article, "%d", article_aleatoire);
        texte_aleatoire(param_texte_article); // Texte aléatoire de 4 lettres
        // Création des fils
        pid_journaliste = fork();

        switch(pid_journaliste){

            case -1: // Père
                break;

            case 0: // Fils
            // Choix dans la demande d'archive
            switch(demande_archive[nombre_aleatoire]){
                
                case 'c': // Consultation
                    execlp("./journaliste", "journaliste", param_nb_archiviste, lettre_c, numero_theme, numero_article, NULL);
                    break;

                case 'p': // Création
                    execlp("./journaliste", "journaliste", param_nb_archiviste, lettre_p, numero_theme, param_texte_article, NULL);
                    break;

                case 'e': // Effacement
                    execlp("./journaliste", "journaliste", param_nb_archiviste, lettre_e, numero_theme, numero_article, NULL);
                    break;
                
                default:
                    printf("Problème de liaison avec notre journaliste\n");
                    exit(-1);
                    break;
            }
            exit(-1); // Jamais atteint (normalement)
            default:
                break;
        }
    }
    exit(0); // Jamais atteint
}   