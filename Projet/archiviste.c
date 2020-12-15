#include "include.h"

int mem_part; // ID memoire partagé
int semap; // ID semaphore
int file_mess; // ID file message
int nb_lecteur = 0; // Nombre de lecteurs (Algorithme Lecteur - Ecrivain)
int nb_ecrivain = 0; // Nombre d'écrivains (Algorithme Lecteur - Ecrivain)
int *file_attente; 
tab_article *tab_theme;
int *nb_article;

// Fonction usage
void usage(char *s){
    printf("Usage : %s <numéro archiviste> <nb_themes>\n", s);
    printf("nb_themes >=2\n");
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
    printf("(%d) Signal de fin reçu\n", getpid());
    couleur(REINIT);
    semctl(semap,1,IPC_RMID,NULL);
    shmctl(mem_part,IPC_RMID,NULL);
    msgctl(file_mess,IPC_RMID, NULL);
    shmdt(file_attente);
    shmdt(tab_theme);
    shmdt(nb_article);
    exit(EXIT_SUCCESS);
}

// P(sem)
int Puisje(int sem, int n){
    struct sembuf op = {sem, -n, SEM_UNDO};
    return semop(semap, &op, 1);
}

// V(sem)
int Vasy(int sem, int n){
    struct sembuf op = {sem, n, SEM_UNDO};
    return semop(semap, &op, 1);
}

int main(int argc, char *argv[]){
    int nom_de_code, nb_themes;
    pid_t pid = getpid();
    key_t cle;
    struct stat st;
    sigset_t  masque_attente;
    int msg_rcv;
    requete_t requete;
    reponse_t reponse;
    requete.type = 6;
    
    // Traitement arguments
    if (argc != 3)
        usage(argv[0]);

    
    nom_de_code = atoi(argv[1]);
    nb_themes = atoi(argv[2]);

    // Gestion signaux
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

    // Attachement du nombre d'articles du SMP
    nb_article = shmat(mem_part,NULL,0);
    if (nb_article==(int *)-1){
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
        perror("file de message ");
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

        // Attente des requêtes
        
        if ((msg_rcv = msgrcv(file_mess, &requete, sizeof(requete_t), requete.type, 0)) == -1){
            printf("Erreur de lecture, erreur numéro %d\n", errno);
            raise(SIGUSR1);
        }
       // printf("%s\n", requete.texte_article);
        // Traitement des données
        if (requete.theme > nb_themes){
            strcpy(reponse.texte_erreur, "Le thème choisi ne fait pas parti de la liste des thèmes");
            msgsnd(file_mess, &reponse, sizeof(reponse_t), 0);
        }

        if (requete.num_archiviste == nom_de_code){ // Si je suis l'archiviste choisi je travaille

            // Chaque archiviste à sa propre petite file d'attente à l'indice de son nom de code
            // Le nombre de personnes dans la file d'attente est incrémenté de 1 à chaque tour de bouche (A chaque fois qu'il reçoit une requête)
            // Toutefois on protegera la file d'attente avec un mutex
            semctl(semap, 0, SETVAL, 1);
            file_attente[nom_de_code]++;
    
            sleep(1);
       
            // Execution du travail - Algorithme Lecteur Ecrivain - Lecteur prioritaire
            semctl(semap, requete.theme, SETVAL, 1);
            semctl(semap, 1, SETVAL, 1);// mutex_nb_ecrivains
            semctl(semap, 2, SETVAL, 1);// mutex lecture
            semctl(semap, 3, SETVAL, 1); // mutex écriture
            semctl(semap, 4, SETVAL, 1); // mutex avant
            semctl(semap, 5, SETVAL, 1); // mutex_nb_lecteurs

            // Partie écrivain

            Puisje(1, 1);
            nb_ecrivain++;
            if (nb_lecteur == 1){
                Puisje(2, 1);
            }
            Vasy(1, 1);
            Puisje(3, 1);
        
             // Ecrire

            if (requete.nature == 'p'){
                printf("Demande de publication émanant de %d\n", requete.expediteur);
                printf("%s\n", requete.texte_article);
                if (nb_article[0] >= MAX_ARTICLE){// Si on a plus d'article que le maximum
                    strcpy(reponse.texte_erreur, "Publication impossible - Maximum d'article atteint"); // Message d'erreur
                }
                else{ // Sinon on copie ce qu'il y a dans l'article au bon endroit
                    strcpy(tab_theme[requete.theme][nb_article[0]], requete.texte_article); 
                    nb_article[0]++; // Et on incrémente le nombre d'articles
                }
            }

            if (requete.nature == 'e'){
                printf("Demande d'effacement émanant de %d\n", requete.expediteur);
                // Effacement d'un article à condition que cet article soit au moins dans l'ensemble
                if (requete.numero_article < nb_article[0]){
                    strcpy(reponse.texte_erreur, "Effacement impossible - Article non existant"); // Il ne l'est pas on ne peut pas
                }else{   // Il l'est, on le supprime
                    nb_article[0]--;
                }
            }  

            Vasy(3, 1);
            Puisje(1, 1);
            nb_ecrivain--;
            if (nb_ecrivain == 0){
                Vasy(2, 1);
            }
            Vasy(1, 1);

            // Partie lecteur

            Puisje(4, 1);
            Puisje(2, 1);
            Puisje(5, 1);
            nb_lecteur++;
            if (nb_lecteur == 1){
                Puisje(3, 1);
            }
            Vasy(5, 1);
            Vasy(2, 1);
            Vasy(4, 1);
        
            // Lire
            if (requete.nature == 'c'){
                printf("Demande de consultation émanant de %d\n", requete.expediteur);
                if (requete.numero_article < nb_article[0]){
                    strcpy(reponse.texte_erreur, "Consultation impossible - Article non existant");
                }else  
                    strcpy(reponse.texte, tab_theme[requete.theme][requete.numero_article]); // On stocke le texte dans la réponse qu'on enverra
            }

            Puisje(5, 1);
            nb_lecteur--;
            if (nb_lecteur == 0){
                Vasy(3, 1);
            }
            Vasy(5, 1);
       
            // Simulation de travail
            sleep(3);
       
            // Envoie de la réponse
            msgsnd(file_mess, &reponse, sizeof(reponse_t), 0);
       }
    }
    exit(0); // En principe on ne l'atteindra pas
}