#include "include.h"


// Fonction usage
void usage(char *s){
    printf("Usage : %s <nb_archiviste> <Consultation> <p1> <p2>\n", s);
    printf("nb_archiviste >= 2\nConsultation = e, p ou c\n");
    printf("p1 / p2 dépende de Consultation");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
    key_t cle;
    pid_t pid = getpid();
    struct stat st;

    int i;
    int semap;
    int moins_occupe;
    int nb_archivistes;
    int numero_ordre;
    int file_mess;
    int mem_part;
    char consultation;
    tab_article *tab_theme;
    int *file_attente; // File d'attente
    int res_rcv;
    requete_t requete;
    reponse_t reponse;
    reponse.type = 0;
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

    // Récupération de l'archiviste le moins occupé
    moins_occupe = file_attente[0];
    numero_ordre = 0;
    for (i = 1; i < nb_archivistes; i++){
        if (moins_occupe > file_attente[i]){ // Si le moins occupé est plus occupé que suivant
            moins_occupe = file_attente[i]; // Alors on les échange
            numero_ordre = i; // Et on préserve le numéro d'ordre
        }
    }

    // Les arguments seront traités en fonction de s'il veut consulter, publier ou effacer
    switch(consultation){
        case 'c': // Consultation
            requete.nature = 'c';
            requete.numero_article = atoi(argv[4]);
            requete.theme = atoi(argv[3]);
            break;

        case 'p': // Publication
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
   
    requete.expediteur = pid; // Journalite défini par son PID
    requete.num_archiviste = numero_ordre; // On demande l'archiviste avec son numéro d'ordre
    requete.type = 6; // 6 champs hors champ type

    // Envoie du message
    msgsnd(file_mess, &requete, sizeof(requete_t), 0);


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
        case 'c': // Consultation
            printf("Consultation\n");
            printf("Thème numéro : %d\n", requete.theme);
            printf("Article numéro : %d\n", requete.numero_article);
            break;
        case 'p':  // Publication
            printf("Publication\n");
            printf("Thème : %d\n", requete.theme);
            printf("Contenu : %s\n", requete.texte_article);
            break;
        case 'e': // Effacement
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
    printf("\n");
    exit(0);
}