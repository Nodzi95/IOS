#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include "h2o.h"

int main(int argc, char *argv[])
{
    int shmid;
    int semid;
    SHM *shm;
    SEM *sem;
    int N, GO, GH, B;
    int pid;
    char *endptr;
    FILE *h2o;

    //ošetření argumentů
    if(argc == 5){
        N = strtol(argv[1],&endptr,10);
        if(*endptr != '\0' || N <= 0){
            fprintf(stderr,"Spatne zadany prvni argument\n");
            return 1;
        }
        GH = strtol(argv[2],&endptr,10);
        if(*endptr != '\0' || GH < 0 || GH > 5000){
            fprintf(stderr,"Spatne zadany druhy argument\n");
            return 1;
        }
        GO = strtol(argv[3],&endptr,10);
        if(*endptr != '\0' || GO < 0 || GO > 5000){
            fprintf(stderr,"Spatne zadany treti argument\n");
            return 1;
        }
        B = strtol(argv[4],&endptr,10);
        if(*endptr != '\0' || B < 0 || B > 5000){
            fprintf(stderr,"Spatne zadany ctvrty argument\n");
            return 1;
        }
    }
    else{
        fprintf(stderr,"Chybny pocet argumentu\n");
        return 1;
    }

    if((h2o = fopen("h2o.txt","w")) == NULL){
        fprintf(stderr,"Soubor nelze otevrit\n");
        return 1;
    }

    //generování unikátního klíče pro paměť
    key_t klic1 = ftok("h2o",1);
    if(errno != 0){
        fprintf(stderr,"Nepodarilo se ziskat klic\n");
        fclose(h2o);
        return 2;
    }
    //naalokovní paměti
    if((shmid = shmget(klic1, sizeof(SHM), IPC_CREAT | 0666)) < 0){
        fprintf(stderr,"Nepodarilo se naalokovat pamet\n");
        fclose(h2o);
        return 2;
    }
    //připojení paměti
    if((shm = (SHM *) shmat(shmid, NULL, 0)) == (void *) -1){
        fprintf(stderr,"Nepodarilo se pripojit pamet\n");
        freeShm(shm,shmid,NULL,0);
        fclose(h2o);
        return 2;
    }
    //inicializace sdílených proměnných
    shm->hydrogen=0;
    shm->oxygen=0;
    shm->count=1;
    shm->h=1;
    shm->o=1;
    shm->bonded=0;
    shm->bonding=0;
    shm->sync=0;

    //generování unikátního klíče pro paměť
    key_t klic2 = ftok("h2o",2);
    if(errno != 0){
        fprintf(stderr,"Nepodarilo se ziskat klic\n");
        freeShm(shm, shmid,NULL,0);
        fclose(h2o);
        return 2;
    }
    //naalokovní paměti
    if((semid = shmget(klic2, sizeof(SEM), IPC_CREAT | 0666)) < 0){
        fprintf(stderr,"Nepodarilo se naalokovat pamet\n");
        freeShm(shm, shmid,NULL,0);
        fclose(h2o);
        return 2;
    }
    //připojení paměti
    if((sem = (SEM *) shmat(semid, NULL, 0)) == (void *) -1){
        fprintf(stderr,"Nepodarilo se pripojit pamet\n");
        freeShm(shm, shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    //inicializace semaforů
    if(sem_init(&sem->mutex, 1, 1)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->barrier, 1, 3)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->oxyQueue, 1, 0)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->hydroQueue, 1, 0)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->memory, 1, 1)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->finished, 1, 0)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->bonding, 1, 0)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->wait, 1, 0)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }
    if(sem_init(&sem->sync, 1, 1)){
        fprintf(stderr,"Nepodarilo se inicializovat semafor\n");
        freeShm(shm,shmid,sem,semid);
        fclose(h2o);
        return 2;
    }

    //vytvoření rodiče pro vodíky
    if ((pid = fork()) == 0) {
        if(make_children(2*N,GH,hydrogen,shm,sem,B,h2o) == 2){
            freeShm(shm,shmid,sem,semid);
            fclose(h2o);
            return 2;
        }
        return 0;
    }
    else if (pid < 0) {
        fprintf(stderr,"Failed to fork\n");
        freeShm(shm, shmid, sem, semid);
        fclose(h2o);
        return 2;
    }

    //vytvoření rodiče pro kyslíky
    if ((pid = fork()) == 0) {
        if(make_children(N,GO,oxygen,shm,sem,B,h2o) == 2){
            freeShm(shm,shmid,sem,semid);
            fclose(h2o);
            return 2;
        }
        return 0;
    }
    else if (pid < 0) {
        fprintf(stderr,"Failed to fork\n");
        freeShm(shm, shmid, sem, semid);
        fclose(h2o);
        return 2;
    }

    //úklid
    sem_wait(&sem->finished);
    freeShm(shm, shmid, sem, semid);
    fclose(h2o);
    return 0;
}
