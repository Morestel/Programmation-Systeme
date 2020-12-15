#include "include.h"

int nb_lecteur;
int semap;
// Fonction usage
void usage(char *s){
    printf("Usage : %s <nb_archiviste> <Consultation> <p1> <p2>\n", s);
    printf("nb_archiviste >= 2\nConsultation = e, p ou c\n");
    printf("p1 / p2 dépende de Consultation");
    exit(EXIT_FAILURE);
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
    int file_mess;
    int mem_part;
    int nb_archivistes;
    char consultation;
    tab_article *tab_theme;
    int *file_attente; // File d'attente
    key_t cle;
    pid_t pid = getpid();
    struct stat st;
    int res_rcv;
    requete_t requete;
    reponse_t reponse;
    reponse.type = 3;
    // Test validité arguments
    if (argc != 5) // 2 paramètres de bases + 2 paramètres suivant le type de requête
        usage(argv[0]);

    if ( (nb_archivistes = atoi(argv[1])) < 2)
        usage(argv[0]);

    consultation = argv[2][0]; // Le test sera effectué après

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
    // Commun à tous les cas possibles
   
    switch(consultation){
        case 'c': // Consultation
            requete.nature = 'c';
            requete.numero_article = atoi(argv[4]);
            requete.theme = atoi(argv[3]);
            break;

        case 'p': // Création
            requete.nature = 'p';
            requete.theme = atoi(argv[3]);
            if (strlen(argv[4]) > 4)
                usage(argv[0]);
            requete.texte_article = argv[4];
            break;

        case 'e': // Effacement
            requete.nature = 'p';
            requete.theme = atoi(argv[3]);
            requete.numero_article = atoi(argv[4]);
            break;

        default: // Aucun des trois c'est donc une erreur
            usage(argv[0]);
            break;
    }
    
    requete.expediteur = pid;
    requete.type = 5;
    msgsnd(file_mess, &requete, sizeof(requete_t), 0);

    // Mutex écriture
    semctl(semap, 0, SETVAL, 1);

    Puisje(0, 1);

    switch(consultation){
        case 'c': // Consultation
            requete.nature = 'c';
            requete.numero_article = atoi(argv[4]);
            requete.theme = atoi(argv[3]);
            break;

        case 'p': // Création
            requete.nature = 'p';
            requete.theme = atoi(argv[3]);
            if (strlen(argv[4]) > 4)
                usage(argv[0]);
            requete.texte_article = argv[4];
            break;

        case 'e': // Effacement
            requete.nature = 'p';
            requete.theme = atoi(argv[3]);
            requete.numero_article = atoi(argv[4]);
            break;

        default: // Aucun des trois c'est donc une erreur
            usage(argv[0]);
            break;
    }
    // Qui est commun à tous les cas
    requete.expediteur = pid;
    requete.type = 5; // Type ne bouge pas

    // Envoie du message
    msgsnd(file_mess, &requete, sizeof(requete_t), 0);

    
    Vasy(0, 1);

    // Réception de la réponse
    res_rcv = msgrcv(file_mess, &reponse, sizeof(reponse_t), reponse.type, 0);
    if (res_rcv ==-1){
	    printf("Erreur, numero %d\n",errno);
	    exit(-1);
    }

    // Traitement des données
    printf("Journaliste %d\n", pid);
    printf("Requête envoyée: ");
    switch(requete.nature){
        case 'c':
            printf("Consultation\n");
            printf("Thème numéro : %d\n", requete.theme);
            printf("Article numéro : %d\n", requete.numero_article);
            break;
        case 'p':  
            printf("Publication\n");
            printf("Thème : %d\n", requete.theme);
            printf("Contenu : %s\n", requete.texte_article);
            break;
        case 'e':
            printf("Effacement\n");
            printf("Thème numéro : %d\n", requete.theme);
            printf("Article numéro : %d\n", requete.numero_article);
            break;
        default:
            printf("Erreur de requête\n");
            break;
    }
    printf("Réponse reçue: %s\n", reponse.texte);
    printf("Problème rencontré: %s\n", reponse.texte_erreur);
    exit(0);
}