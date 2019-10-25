#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include "fs.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

tecnicofs* fs;
char* fileInput = NULL;
char* fileOutput = NULL;
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberThreads = 0;
int numberBuckets = 1;
int numberCommands = 0;
int headQueue = 0;
int CommandNumber = 0;
int lastIndex = 0;
sem_t sem1;
sem_t sem2;

#ifdef MUTEX
pthread_mutex_t locker1;
pthread_mutex_t locker2;
#define INIT() { \
    if(pthread_mutex_init(&locker1, NULL)) exit(EXIT_FAILURE); \
    if(pthread_mutex_init(&locker2, NULL)) exit(EXIT_FAILURE); \
    if(sem_init(&sem1, 0, MAX_COMMANDS)) exit(EXIT_FAILURE); \
    if(sem_init(&sem2, 0, MAX_COMMANDS)) exit(EXIT_FAILURE); }
#define LOCK(A) pthread_mutex_lock(&A)
#define UNLOCK(A) pthread_mutex_unlock(&A)
#define DESTROY() { \
    if(pthread_mutex_destroy(&locker1)) exit(EXIT_FAILURE); \
    if(pthread_mutex_destroy(&locker2)) exit(EXIT_FAILURE); \
    if(sem_destroy(&sem1)) exit(EXIT_FAILURE); \
    if(sem_destroy(&sem2)) exit(EXIT_FAILURE); }
#elif RWLOCK
pthread_rwlock_t locker1;
pthread_rwlock_t locker2;
#define INIT() { \
    if(pthread_rwlock_init(&locker1, NULL)) exit(EXIT_FAILURE); \
    if(pthread_rwlock_init(&locker2, NULL)) exit(EXIT_FAILURE); \
    if(sem_init(&sem1, 0, MAX_COMMANDS)) exit(EXIT_FAILURE); \
    if(sem_init(&sem2, 0, MAX_COMMANDS)) exit(EXIT_FAILURE); }
#define LOCK(A) pthread_rwlock_wrlock(&A)
#define UNLOCK(A) pthread_rwlock_unlock(&A)
#define DESTROY() { \
    if(pthread_rwlock_destroy(&locker1)) exit(EXIT_FAILURE); \
    if(pthread_rwlock_destroy(&locker2)) exit(EXIT_FAILURE); \
    if(sem_destroy(&sem1)) exit(EXIT_FAILURE); \
    if(sem_destroy(&sem2)) exit(EXIT_FAILURE); }
#else
#define INIT() { \
    if(sem_init(&sem1, 0, MAX_COMMANDS)) exit(EXIT_FAILURE); \
    if(sem_init(&sem2, 0, MAX_COMMANDS)) exit(EXIT_FAILURE); }
#define LOCK(A) {}
#define UNLOCK(A) {}
#define DESTROY() { \
    if(sem_destroy(&sem1)) exit(EXIT_FAILURE); \
    if(sem_destroy(&sem2)) exit(EXIT_FAILURE); }
#endif

static void displayUsage (const char* appName) {
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    fileInput = argv[1];
    fileOutput = argv[2];
    numberThreads = atoi(argv[3]);
    numberBuckets = atoi(argv[4]);
    if (numberThreads < 1)
        exit(EXIT_FAILURE);
    else if (numberThreads > 1) {
        #ifdef MUTEX
        return;
        #endif
        #ifdef RWLOCK
        return;
        #endif
    }
    numberThreads = 1;
}

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {    
    if(numberCommands > 0) {
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    return NULL;
}

void errorParse() {
    fprintf(stderr, "Error: command invalid\n");
}

void processInput() {
    FILE* fptr  = fopen(fileInput, "r");
    if (!fptr)
        exit(EXIT_FAILURE);
    char line[MAX_INPUT_SIZE];
    //while(fgets(line, sizeof(line)/sizeof(char), fptr)) {
    while(1) {
        LOCK();
        sem_wait(&sem1);
        char token;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(line, "%c %s", &token, name);
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
            case 'l':
            case 'd':
                if (numTokens != 2)
                    errorParse();
                if (insertCommand(line))
                    break;
                return;
            case '#':
                break;
            default: {
                errorParse();
            }
        }
        sem_post(&sem2);
    }
    fclose(fptr);
}

void* applyCommands() {
    while(1) {
        LOCK(locker1);
        sem_wait(&sem2);
        if (numberCommands <= 0) {
            UNLOCK(LOCKER1);
            break;
        }
        const char* command = removeCommand();
        if (command == NULL) {
            UNLOCK(locker1);
            continue;
        }
        sem_post(&sem1);
        UNLOCK(locker1);
        char token;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s", &token, name);
        if (numTokens != 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        int searchResult;
        int iNumber;
        switch (token) {
            case 'c':
                LOCK(locker2);
                iNumber = obtainNewInumber(fs);
                create(fs, name, iNumber, numberBuckets);
                UNLOCK(locker2);
                break;
            case 'l':
                LOCK(locker2);
                searchResult = lookup(fs, name, numberBuckets);
                UNLOCK(locker2);
                if(!searchResult)
                    printf("%s not found\n", name);
                else
                    printf("%s found with inumber %d\n", name, searchResult);
                break;
            case 'd':
                LOCK(locker2);
                delete(fs, name, numberBuckets);
                UNLOCK(locker2);
                break;
            default: {
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
       
    }
    return NULL;
}

void applyThread() {
    INIT();
    pthread_t processor;
    pthread_t workers[numberThreads];
    int err = pthread_create(&processor, NULL, processInput, NULL);
    if (err != 0) {
      perror("Can't create thread\n");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numberThreads; i++) {
        int err = pthread_create(&workers[i], NULL, applyCommands, NULL);
        if (err != 0) {
            perror("Can't create thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < numberThreads; i++) {
        if (pthread_join(workers[i], NULL)) {
            perror("Can't join thread\n");
        }
    }
    pthread_join(processor, NULL);
    DESTROY();
}

int main(int argc, char* argv[]) {
    struct timeval start, end;
    double seconds, micros;
    parseArgs(argc, argv);
    //gettimeofday(&start, NULL);
    //applyThread();
    processInput();
    fs = new_tecnicofs(numberBuckets);
    gettimeofday(&start, NULL);
    applyThread();
    gettimeofday(&end, NULL);
    FILE* fptr = fopen(fileOutput, "w");
    print_tecnicofs_tree(fptr, fs, numberBuckets);
    fclose(fptr);
    free_tecnicofs(fs, numberBuckets);
    seconds = (double) (end.tv_sec - start.tv_sec);
    micros = (double) ((seconds + (double) (end.tv_usec - start.tv_usec)/1000000));
    printf("TecnicoFS completed in %.4f seconds.\n", micros);
    exit(EXIT_SUCCESS);
}