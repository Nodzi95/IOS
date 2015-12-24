//struktura sdélených proměnných
typedef struct{
    int hydrogen;                                                       //udává počet vodíků, které je nutno obsloužit
    int oxygen;                                                         //udává počet kyslíků, které je nutno obsloužit
    int o;                                                              //indexy kyslíků
    int h;                                                              //indexy vodíků
    int count;                                                          //počítadlo akcí
    int bonded;                                                         //počítá, kolik atomů bylo dokončeno
    int bonding;                                                        //počítá, kolik atomů se právě spojuje
    int sync;                                                           //zajišťuje vytváření jen jedné molekuly
} SHM;

//struktura sdílených semaforů
typedef struct{
    sem_t mutex;                                                        //zabrání příchodu 2 atomů zároveň
    sem_t barrier;                                                      //propouští po 3 procesech
    sem_t oxyQueue;                                                     //fronta kyslíků
    sem_t hydroQueue;                                                   //fronta vodíků
    sem_t memory;                                                       //zajistí výlučný přístup do paměti
    sem_t finished;                                                     //signál pro hlavní proces, že je vše hotovo
    sem_t bonding;                                                      //zařídí, že se všechny procesy ukončí zároveň
    sem_t wait;                                                         //zajišťuje vypsání begin bonding před bonded
    sem_t sync;                                                         //zajišťuje vytváření jen jedné molekuly
} SEM;


int make_children(int N, int max,void (*function)(),SHM * shm, SEM * sem, int B, FILE * h2o);
void freeShm (SHM * shm, int shmid, SEM * sem, int semid);
void bond(char NAME, SHM * shm, SEM * sem, int x, int B, FILE * h2o, int N);
void oxygen(SHM * shm, SEM * sem, int B, FILE * h2o, int N);
void hydrogen(SHM * shm, SEM * sem, int B, FILE * h2o, int N);

//funkce vytvoří N atomů(procesů)
int make_children(int N, int max,void (*function)(),SHM * shm, SEM * sem, int B, FILE * h2o){
    int k;
    int pid;
    srand (time (NULL));
    for(k = 0; k < N; k++) {
        usleep (rand () % ((max * 1000) + 1));
        pid = fork();
        //waitpid (-1, 0, WNOHANG);
        if(pid < 0) {

            fprintf(stderr,"Failed to fork\n");
            //fclose(h2o);
            return 2;
            break;
        }
        else if (pid == 0) {
            function(shm, sem, B, h2o, N);
            return 0;
        }
    }
    return 0;
}

//uvolnění sdílených zdrojů
void freeShm(SHM * shm, int shmid, SEM * sem, int semid){
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    shmdt(sem);
    shmctl(semid, IPC_RMID, NULL);
}

//funkce na vytváření molekuly
void bond(char NAME, SHM * shm, SEM * sem, int x, int B, FILE * h2o, int N){
    //srand(time(NULL));
    int i;
    sem_wait(&sem->memory);
    fprintf(h2o,"%d    : %c %d : begin bonding\n", shm->count++, NAME,x);
    fflush(h2o);
    sem_post(&sem->memory);
    sem_wait(&sem->memory);
    shm->bonding++;
    sem_post(&sem->memory);
    if(shm->bonding == 3){                                          //zajišťuje vypsání begin bonding za sebou
        sem_post(&sem->wait);
        sem_post(&sem->wait);
        sem_post(&sem->wait);
        shm->bonding=0;
    }
    usleep (rand () % ((B * 1000) + 1));
    sem_wait(&sem->memory);
    shm->bonded++;
    sem_post(&sem->memory);
    sem_post(&sem->barrier);
    sem_wait(&sem->wait);
    sem_wait(&sem->memory);
    fprintf(h2o,"%d    : %c %d : bonded\n", shm->count++, NAME,x);
    fflush(h2o);
    sem_post(&sem->memory);

    sem_wait(&sem->memory);
    shm->sync++;
    sem_post(&sem->memory);
    if(shm->sync == 3){                                             //zajišťuje vytváří jen jedné molelkuly
        shm->sync = 0;
        sem_post(&sem->sync);

    }

    if(shm->bonded == 3*N){                                         //po příchodu posledního procesu, umožní všechny procesy ukončit
        for(i = 0; i < 3*N; i++){
            sem_post(&sem->bonding);
        }
        sem_post(&sem->finished);
    }
    //sem_post(&sem->barrier);
}

//proces kyslíku
void oxygen(SHM * shm, SEM * sem, int B, FILE * h2o, int N){
    int ox;
    //srand (time (NULL));
    sem_wait(&sem->mutex);
    sem_wait(&sem->memory);
    ox=shm->o++;
    fflush(h2o);
    fprintf(h2o,"%d    : O %d : started\n", shm->count++,ox);
    fflush(h2o);
    sem_post(&sem->memory);
    sem_wait(&sem->memory);
    shm->oxygen++;
    sem_post(&sem->memory);
    if(shm->hydrogen >=2){
        sem_wait(&sem->sync);
        sem_wait(&sem->memory);
        fprintf(h2o,"%d    : O %d : ready\n", shm->count++,ox);
        fflush(h2o);
        sem_post(&sem->memory);
        sem_post(&sem->hydroQueue);
        sem_post(&sem->hydroQueue);
        shm->hydrogen -= 2;
        sem_post(&sem->oxyQueue);
        shm->oxygen--;

    }
    else{
        sem_wait(&sem->memory);
        fprintf(h2o,"%d    : O %d : waiting\n", shm->count++,ox);
        fflush(h2o);
        sem_post(&sem->memory);
        sem_post(&sem->mutex);
    }
    sem_wait(&sem->oxyQueue);
    bond('O', shm, sem, ox, B, h2o, N);

    sem_wait(&sem->barrier);
    sem_post(&sem->mutex);

    sem_wait(&sem->bonding);
    sem_wait(&sem->memory);
    fprintf(h2o,"%d    : O %d : finished\n", shm->count++, ox);
    fflush(h2o);
    sem_post(&sem->memory);
}

//proces vodíku
void hydrogen(SHM * shm, SEM * sem, int B, FILE * h2o, int N){
    int hy;
    //srand (time (NULL));
    sem_wait(&sem->mutex);
    sem_wait(&sem->memory);
    hy=shm->h++;
    fflush(h2o);
    fprintf(h2o,"%d    : H %d : started\n", shm->count++,hy);
    fflush(h2o);
    sem_post(&sem->memory);
    sem_wait(&sem->memory);
    shm->hydrogen++;                                                    //přijde vodík
    sem_post(&sem->memory);
    if(shm->hydrogen >= 2 && shm->oxygen >= 1){                         //pokud máme dostatek správných atomů, tak proces požádá o slovo
        sem_wait(&sem->sync);
        sem_wait(&sem->memory);
        fprintf(h2o,"%d    : H %d : ready\n", shm->count++,hy);
        fflush(h2o);
        sem_post(&sem->memory);
        sem_post(&sem->hydroQueue);
        sem_post(&sem->hydroQueue);
        shm->hydrogen -= 2;                                             //odebereme použité vodíky
        sem_post(&sem->oxyQueue);
        shm->oxygen--;                                                  //odebereme použitý kyslík
    }
    else{
        sem_wait(&sem->memory);
        fprintf(h2o,"%d    : H %d : waiting\n", shm->count++,hy);       //atom čeká ve frontě na další atomy
        fflush(h2o);
        sem_post(&sem->memory);
        sem_post(&sem->mutex);
    }
    sem_wait(&sem->hydroQueue);
    bond('H', shm, sem, hy, B, h2o, N/2);                                //když máme 1x kyslík a 2x vodík provede se funkce bond

    sem_wait(&sem->barrier);

    sem_wait(&sem->bonding);                                            //čeká až přijde poslední proces
    sem_wait(&sem->memory);
    fprintf(h2o,"%d    : H %d : finished\n", shm->count++, hy);
    fflush(h2o);
    sem_post(&sem->memory);
}
