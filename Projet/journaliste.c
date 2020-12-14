#include "include.h"

// Fonction usage
void usage(char *s){
    printf("Usage : %s <nb_archiviste> <Consultation> <p1> <p2>\n", s);
    printf("nb_archiviste >= 2\nConsultation = e, p ou c\n");
    printf("p1 / p2 dépende de Consultation");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
    int file_mess;
    int mem_part;
    int semap;
    int nb_archivistes;
    char consultation;
    int *tab; // Entier du SMP
    key_t cle;
    pid_t pid = getpid();
    struct stat st;
    requete_t requete;
    reponse_t reponse;

    int res_rcv;

    struct sembuf P = {0,-1,SEM_UNDO};
    struct sembuf V = {0,1,SEM_UNDO};
    
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

    // Commun à tous les cas possibles
    requete.expediteur = pid;
    printf("JOURNALISTE: CONSULTATION %c\n", consultation);
    switch(consultation){
        case 'c': // Consultation
            requete.nature = 'c';
            requete.numero_article = atoi(argv[4]);
            requete.theme = atoi(argv[3]);
            // Envoie de la requête
            msgsnd(file_mess, &requete, sizeof(requete_t), 0);
            break;

        case 'p': // Création
            requete.nature = 'p';
            requete.theme = atoi(argv[3]);
            if (strlen(argv[4]) > 4)
                usage(argv[0]);
            requete.texte_article = argv[4];
            // Envoie de la requête
            msgsnd(file_mess, &requete, sizeof(requete_t), 0);
            break;

        case 'e': // Effacement
            requete.nature = 'p';
            requete.theme = atoi(argv[3]);
            requete.numero_article = atoi(argv[4]);
            // Envoie de la requête
            msgsnd(file_mess, &requete, sizeof(requete_t), 0);
            break;

        default: // Aucun des trois c'est donc une erreur
            usage(argv[0]);
            break;
    }
    
    // Réception de la réponse
    res_rcv = msgrcv(file_mess,&reponse,sizeof(reponse_t),pid,0);
    if (res_rcv ==-1){
	    printf("Erreur, numero %d\n",errno);
	    exit(-1);
    }
}