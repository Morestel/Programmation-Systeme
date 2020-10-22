#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include <unistd.h>


/*============= Debut des types ====================*/

#define LONGUEUR_CODE_MAX 100

#define COULEUR_MAX 100

typedef int vecteur[LONGUEUR_CODE_MAX];

struct reponse {
    int blancs,noirs;
    vecteur info;
    struct reponse *suiv;
};

typedef struct reponse *reponse;


/*============= Fin des types ======================*/

int nb_comparaisons ; /* Necessairement globale, pour etre visible de Calcule_Reponse */
int signal_sig = 0;

/*----------------------------------------------------------

    Enveloppe du malloc, pour tester si on a encore de la memoire

  ----------------------------------------------------------*/
void *alloc_memoire(size_t taille){
    void *p = malloc(taille);

    if (p == NULL){ // Plus de memoire, inutile de continuer...
	fprintf(stderr,"Plus de memoire, bye\n");
	exit(-1);
    }

    return p;
}

/*----------------------------------------------------------

    Choisit un point de depart au hasard dans ref;
    initialise le nombre de comparaisons (qui est global)

  ----------------------------------------------------------*/
void initialiser(int couleur, int lgr, vecteur ref){
    int i;
    
    srand(getpid());
    
    for (i=0;i<lgr;i++){
	ref[i]=(rand()%couleur)+1;
    }

    nb_comparaisons = 0;
}


/*----------------------------------------------------------

    La marque du pluriel....

  ----------------------------------------------------------*/
char pluriel(int x){

    if (x > 1)
	return 's';
    return ' ';
}


/*----------------------------------------------------------

    Compare la combinaison prop au code;
    ecrit dans noirs et blancs la reponse

  ----------------------------------------------------------*/
void Calcule_Reponse(int couleurs, int longueur, 
		    vecteur prop, vecteur code,
		    int *noirs, int *blancs){
    int i;
    vecteur c_prop,c_code;

    *noirs  = 0;
    *blancs = 0;
    
    memset(c_prop,0,sizeof(vecteur));
    memset(c_code,0,sizeof(vecteur));

    
    for (i=0;i<longueur;i++)
      if (prop[i] == code[i])
	(*noirs)++;
      else
	{
	  c_prop[prop[i]-1]++;
	  c_code[code[i]-1]++;
	}
    
    
    for (i=0;i<couleurs;i++)
      (*blancs) += (c_prop[i] < c_code[i] ? c_prop[i] : c_code[i]);
    
    nb_comparaisons++;
}

/*----------------------------------------------------------
    
    Vrai si x precede y, modulo n

  ----------------------------------------------------------*/
int precede(int x, int y, int n){
    
    if (y == 1)
	return (x == n);
    else
	return (x == y - 1);
}

/*----------------------------------------------------------

    Cherche la proposition suivante admissible;
    la proposition precedente est dans prop_precedente,
    qui contiendra au retour la suivante admissible;
    reference est le point de depart (pour savoir quand 
    passer a la colonne suivante)

  ----------------------------------------------------------*/
void Suivante_Admissible(vecteur prop_precedente, reponse historique, 
	      vecteur reference, 
	      int couleurs, int longueur){
    int pas_trouve=1;
    int i;
    int so_far_so_good,noirs,blancs;
    reponse curseur;
    
    while (pas_trouve){
	// On cherche le rang a incrementer (cf fonctionnement d'un compteur)
	for(i = 0; precede(prop_precedente[i], reference[i], couleurs); i++)
	    prop_precedente[i] = reference[i];

	// On l'incremente
	if (prop_precedente[i] == couleurs)
	    prop_precedente[i] = 1;
	else
	    prop_precedente[i] += 1;
	
      so_far_so_good = 1;
      curseur = historique;
      
      // Comparaison de la proposition a toutes les reponses de l'historique
      while ((so_far_so_good) && (curseur != NULL)){
	  Calcule_Reponse(couleurs, longueur, prop_precedente, curseur->info, &noirs, &blancs);
	  if ((blancs == curseur->blancs)&&
	      (noirs == curseur->noirs))
	      curseur = curseur->suiv;
	  else
	      so_far_so_good = 0;
      }

      // Fini?
      pas_trouve = (curseur != NULL);
    }
  
}


/*----------------------------------------------------------

    Le point de depart au hasard est dans ref;
    cf description dans l'enonce du tp

  ----------------------------------------------------------*/

void avancement(int signal){
  signal_sig = 1;
}


void affiche_sig(vecteur reference, vecteur proposition, vecteur code, int longueur){
  int i;
  printf("\tRéférence: ");
  for (i = 0; i < longueur; i++)
    printf("%d ", reference[i]);
  printf("\n");
  printf("\tje suis en train d'examiner ");
  for (i = 0; i < longueur; i++)
    printf("%d ", proposition[i]);
  printf("\n");
  printf("\tCode: ");
  for (i = 0; i < longueur; i++)
    printf("%d ", code[i]);
  printf("\n");
  signal_sig = 0;
}
int mastermind(int couleurs, int longueur, 
	       vecteur reference, vecteur code){

  // Variables principales :                                                  
  int pas_fini;               // Booleen de controle de la boucle principale
  int essais;                 // Nombre d'essais
  reponse historique;         // Historique des propositions
  vecteur proposition;        // La proposition courante

  // Et quelques variables d'administration :
  int i, noirs, blancs;
  reponse rep;

  struct sigaction act;
  memset(&act, '\0', sizeof(act));
  act.sa_handler = &avancement;
  act.sa_flags = 0;
  // Initialisations
  pas_fini = 1;
  essais   = 0;
  historique = NULL;
  
  // Point de depart de la recherche
  for (i=0; i < longueur; i++)
    proposition[i] = reference[i];
    
  while (pas_fini){// Tant qu'on n'a pas tout trouve
    // Passer a la proposition suivante
    Suivante_Admissible(proposition, historique, reference, couleurs, longueur);

    // Calcul de la reponse et remplissage des champs d'une reponse 
    Calcule_Reponse(couleurs, longueur, code, proposition, &noirs, &blancs);
    //signal(SIGINT, avancement);
    sigaction(SIGINT, &act, NULL);
    if (signal_sig == 1)
      affiche_sig(reference, proposition, code, longueur);
    rep = alloc_memoire(sizeof(struct reponse));	
    rep->noirs  = noirs;
    rep->blancs = blancs;
    for(i=0;i<longueur;i++)
      rep->info[i] = proposition[i];

    // Affichage....
    fprintf(stdout,"   Je propose ");
    for(i=0; i < longueur; i++)
      fprintf(stdout,"%2d ", proposition[i]);
    fprintf(stdout,"  Reponse : ");
    fprintf(stdout,"%d noir%c, ", rep->noirs, pluriel(rep->noirs));
    fprintf(stdout,"%d blanc%c\n", rep->blancs, pluriel(rep->blancs));

    // Ajout de rep dans l'historique
    rep->suiv = historique;
    historique = rep;

    // Un essai de plus :
    essais++;

    // Fini ?
    pas_fini = (rep->noirs != longueur);
  }
  return essais;
}

/*====================== Fin des fonctions ======================*/



void usage(char *a){
  fprintf(stderr,"Usage : %s <nb. couleurs> <code>\n",a);
  fprintf(stderr,"    avec 0 < <nb. couleurs> < %d,\n",COULEUR_MAX);
  exit(-1);
}


int main(int argc, char *argv[]){
  
  /*---------------------------------------------
  !                                             !
  !    DÃ©clarations de variables                !
  !                                             !
  !--------------------------------------------*/

  int i, on_continue = 1;  // Utilisees dans le decodage d'argv
  int essais;              // Nombre d'essais effectues pour trouver le code
  int nb_coul, long_code;  // Nombre de couleurs et longueur du code
  vecteur reference, code; // Point de depart de la recherche et code



  /*---------------------------------------------
  !                                             !
  !    DÃ©chiffrage de la ligne de commande,     !
  !    initialisations.                         !
  !                                             !
  !--------------------------------------------*/

  if (argc < 3)
      usage(argv[0]);
  
  nb_coul = atoi(argv[1]);
  if ((nb_coul > COULEUR_MAX)||(nb_coul < 1))
      usage(argv[0]);

  long_code = argc - 2;
  if (long_code > LONGUEUR_CODE_MAX)
      usage(argv[0]);

  for(i = 0; (i < long_code) && (on_continue); i++){
    code[i] = atoi(argv[i+2]);
      if((code[i] > nb_coul) || (code[i] < 1))
	on_continue = 0;
  }

  if (!on_continue)
      usage(argv[0]);

  initialiser(nb_coul, long_code, reference);

  /*---------------------------------------------
  !                                             !
  !                Et on y va !                 !
  !                                             !
  !--------------------------------------------*/

  essais = mastermind(nb_coul, long_code, reference, code);

  fprintf(stdout,"\n\n\n Nombre d'essais : %d\n", essais);
  fprintf(stdout," Nombre de comparaisons : %d\n", nb_comparaisons);
	       
  exit(0);
}
