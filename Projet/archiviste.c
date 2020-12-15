#include "include.h"

int mem_part;
int semap;
int file_mess;
int nb_lecteur = 0;

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

int Puisje(int sem, int n){
    struct sembuf op = {sem, -n, SEM_UNDO};
    return semop(semap, &op, 1);
}

int Vasy(int sem, int n){
    struct sembuf op = {sem, n, SEM_UNDO};
    return semop(semap, &op, 1);
}

int main(int argc, char *argv[]){
    int nom_de_code, nb_themes;
    int i;
    pid_t pid = getpid();
    key_t cle;
    struct stat st;
    tab_article *tab_theme; // Tableau des thèmes           
    int *file_attente; // File d'attente en mémoire partagée
    sigset_t  masque_attente;
    int msg_rcv;
    requete_t requete;
    reponse_t reponse;
    int publication = 0;
    requete.type = 5;
    reponse.type = 3;
    if (argc != 3)
        usage(argv[0]);

    nom_de_code = atoi(argv[1]);
    nb_themes = atoi(argv[2]);

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
    tab_theme = shmat(mem_part,NULL,0);
    if (tab_theme==(void *)-1){
	printf("(%d) Pb attachement SMP\n",pid);
	exit(-1);
    }

    // Attachement de la file d'attente du SMP
    file_attente = shmat(mem_part,NULL,0);
    if (file_attente==(int *)-1){
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
    
    // L'archiviste se présente
    printf("Archiviste - Nom de code : %d\n", nom_de_code);

    // Execution de la boucle infinie
    for(;;){
        // Initialisation de la réponse
        strcpy(reponse.texte_erreur, "Aucun problème");
        strcpy(reponse.texte, "aaaa");
        reponse.code_archiviste = nom_de_code;

        // Attente des requêtes
        
        if ((msg_rcv = msgrcv(file_mess, &requete, sizeof(requete_t), requete.type, 0)) == -1){
            printf("Erreur de lecture, erreur numéro %d\n", errno);
            raise(SIGUSR1);
        }

        // Traitement des données
        if (requete.theme > nb_themes){
            strcpy(reponse.texte_erreur, "Le thème choisi ne fait pas parti de la liste des thèmes");
            msgsnd(file_mess, &reponse, sizeof(reponse_t), 0);
        }

        // Chaque archiviste à sa propre petite file d'attente à l'indice de son nom de code
        // Le nombre de personnes dans la file d'attente est incrémenté de 1 à chaque tour de bouche (A chaque fois qu'il reçoit une requête)
        // Toutefois on protegera la file d'attente avec un mutex
        semctl(semap, 0, SETVAL, 1);
        file_attente[nom_de_code]++;
    
        sleep(1);
       
        // Execution du travail - Algorithme Lecteur Ecrivain - Lecteur prioritaire
        // Création des mutex écriture / mutex_nb_lecteurs
        semctl(semap, requete.theme, SETVAL, 1);
        semctl(semap, 0, SETVAL, 1);

        Puisje(0, 1);
        nb_lecteur++;
        if (nb_lecteur == 1){
            Puisje(requete.theme, 1);
        }
        Vasy(0, 1);

        // Lire   
        switch(requete.nature){
            case 'c':
                printf("Demande de consultation émanant de %d\n", requete.expediteur);
                 if (tab_theme[requete.theme][requete.numero_article] == NULL){
                    strcpy(reponse.texte_erreur, "Consultation impossible - Article non existant");
                }else  
                    strcpy(reponse.texte, tab_theme[requete.theme][requete.numero_article]); // On stocke le texte dans la réponse qu'on enverra
                break;
            case 'p':
                printf("Demande de publication émanant de %d\n", requete.expediteur);
                for (i = 0; i < MAX_ARTICLE; i++){ // Parcours de la table des thèmes
                    if (tab_theme[requete.theme][i] == NULL){ // On cherche un emplacement d'article vide
                        strcpy(tab_theme[requete.theme][i], requete.texte_article); // C'est vide, on le met à cette position
                        publication = 1;
                    }
                }
                if (!publication){
                    strcpy(reponse.texte_erreur, "Publication impossible - Maximum d'article atteint");
                }
                break;
            case 'e':
                printf("Demande d'effacement émanant de %d\n", requete.expediteur);
                // On va effacer un article en le remplaçant par des espaces s'il existe
                if (tab_theme[requete.theme][requete.numero_article] == NULL){
                    strcpy(reponse.texte_erreur, "Effacement impossible - Article non existant");
                }else                         
                  strcpy(tab_theme[requete.theme][requete.numero_article], "    ");

                break;
            default:
                printf("Demande fellation\n");
                break;
        }
       
        // Simulation de travail
        sleep(3);
        
        Puisje(0, 1);
        nb_lecteur--;
        if (nb_lecteur == 0){
            Vasy(0, 1);
        }

        Vasy(requete.theme, 1);
        printf("AVANT ENVOIE\n");
        // Envoie de la réponse
        msgsnd(file_mess, &reponse, sizeof(reponse_t), 0);
        printf("APRES ENVOIE\n");
    }
    exit(0); // En principe on ne l'atteindra pas
}