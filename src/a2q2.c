#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define P 8
#define R 4

#define NOT_START 0
#define START 1
#define RUNNING 2
#define FINISH 3

sem_t mutex;
sem_t S[R];

int k;

int state[P];
int process_num[P];
int counter[P];     //counter for no of times of execution

int process_res[P][R] = 
{   {1, 1, 1, 0}, 
    {0, 1, 1, 1},
    {1, 0, 1, 1}, 
    {1, 1, 0, 1},
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1}
};

//allocation of resources
void allocate_resources(int x) {
    for(int i=0;i<R;i++) {
        if(process_res[x][i] == 1) {
            sem_wait(&S[i]);
        //    printf("resource %d allocated for process %d\n",i, x);
        }
    }
}

//deallocate resources
void d_alloc(int res_num )
{
    double ran = (double) rand()/(double) RAND_MAX;
    double prob[4] = {2.0/3.0, 3.0/4.0, 3.0/5.0, 2.0/3.0};
    if(ran < prob[res_num]) {
        sem_post(&S[res_num]);
        printf("Resource %c collected\n", (char)(res_num+65));
    } else {
        printf("Resource %c not collected\n",(char)(res_num+65));
    }
    return;
}

void deallocate_resources(int x) {
    for(int i=0;i<R;i++) {
        if(process_res[x][i]==1) {
            d_alloc(i);
        }
    }
}

void *funct(void *ptr) {
    while(1) { 
        int x;
        x = *((int *) ptr);
        int i;

            sem_wait(&mutex);
            int res_avail[4];
            sem_getvalue(&S[0], &res_avail[0]);
            sem_getvalue(&S[1], &res_avail[1]);
            sem_getvalue(&S[2], &res_avail[2]);
            sem_getvalue(&S[3], &res_avail[3]);
          //  printf("Process-%d, res-%d %d %d %d\n", x, res_avail[0], res_avail[1], res_avail[2], res_avail[3]);
            //int i;
            for(i=0;i<4;i++) {
                if(res_avail[i] < process_res[x][i]) {
                    sem_post(&mutex);
                    break;
                }
            }
            if(i<4) {
                printf("Process %d: Finished, waiting upon resource- %d\n", x, i);
                pthread_exit(0);
                //continue;
            }
            allocate_resources(x);
            if(counter[x]==k) {
                state[x]=START;
                printf("Process %d: Started\n", x);
            } 
            state[x]=RUNNING;
            printf("Process %d: Running\n", x);
          //  printf("Process %d: counter=%d before\n", x, counter[x]);
            counter[x]--;
          //  printf("Process %d: counter=%d after\n", x, counter[x]);
            deallocate_resources(x);
            sem_post(&mutex);           
            if(counter[x]==0) {
                state[x]=FINISH;
                printf("Process %d: Finished\n", x);
                pthread_exit(0);
            } else {
                state[x]=START;
            }
            sleep(1);
 
    }
} 


int main() {
    for(int i=0;i<P;i++)
        state[i]=NOT_START;
    
    for(int i=0;i<P;i++)
        process_num[i]=i;
    
    scanf("%d", &k);
    for(int i=0;i<P;i++)
        counter[i] = k;

    int temp;
    for(int i=0;i<R;i++) {
        scanf("%d", &temp);
        sem_init(&S[i], 0, temp);
    }

    pthread_t thread_id[P];
    
    sem_init(&mutex, 0, 1);
    
    for(int i=0;i<P;i++) {
        if(pthread_create (&thread_id[i], NULL, funct, (void *)&process_num[i])) {
            printf("Error creating thread\n");
            return 0;
        }
    }
    
    for(int i=0;i<2;i++)
        pthread_join(thread_id[i], NULL);
    sem_destroy(&mutex);
    sem_destroy(S);
   /* printf("Process counters\n");
    for(int i=0;i<8;i++)
        printf("Process %d - counter = %d\n", i, counter[i]); */   
    return 0;    
}