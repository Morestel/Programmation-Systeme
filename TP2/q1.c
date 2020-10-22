#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


void usage(char *s){
  printf("Usage %s: <nombre> [<faces> [<limite>]]\n", s);
  printf("\t0 < <nombre> < 1000\n");
  printf("\tfaces par défaut : 12\n");
  printf("\tlimite par défaut : 10\n");
  exit(-1);
}

int main (int argc, char *argv[],char *envp[]){
    int n;
    int i;
    pid_t pid;

    if (argc < 2) 
      usage(argv[0]);
    
    n = atoi(argv[1]);
    
    for(i = 0; i < n; i++){
      pid = fork();
      if (pid == -1)// Echec, on sort
	break;
      if (pid == 0){// lancement du fils numero i 
	execlp("filsq3","filsq3",NULL);
	fprintf(stderr,"Pb exec fils %d\n",i);
	exit(-1);
      }
      fprintf(stdout,"Lancement du fils %d\n",pid);
    }
    
}
