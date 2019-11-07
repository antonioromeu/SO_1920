#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
int numberCommands = 0;
int numberThreads = 0;
int numberBuckets = 1;
int headQueue = 0;
int flag = 1;
sem_t sem_prod;
sem_t sem_cons;

#ifdef MUTEX
pthread_mutex_t commandsLocker;
pthread_mutex_t* vecLock;
#define INIT(A) { \
    vecLock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * A); \
    if (pthread_mutex_init(&commandsLocker, NULL)) \
        exit(EXIT_FAILURE); \
    for (int i = 0; i < A; i++) \
        if(pthread_mutex_init(&vecLock[i], NULL)) \
            exit(EXIT_FAILURE); }
#define LOCK(A) pthread_mutex_lock(&A)
#define UNLOCK(A) pthread_mutex_unlock(&A)
#define DESTROY(A) { \
    if (pthread_mutex_destroy(&commandsLocker)) \
        exit(EXIT_FAILURE); \
    for (int i = 0; i < A; i++) \
        if (pthread_mutex_destroy(&vecLock[i])); }
#elif RWLOCK
pthread_rwlock_t commandsLocker;
pthread_rwlock_t* vecLock;
#define INIT(A) { \
    vecLock = (pthread_rwlock_t*) malloc(sizeof(pthread_rwlock_t) * A); \
    if (pthread_rwlock_init(&commandsLocker, NULL)) \
        exit(EXIT_FAILURE); \
    for (int i = 0; i < A; i++) \
        if (pthread_rwlock_init(&vecLock[i], NULL)) \
            exit(EXIT_FAILURE); }
#define LOCK(A) pthread_rwlock_wrlock(&A)
#define UNLOCK(A) pthread_rwlock_unlock(&A)
#define DESTROY(A) { \
    if (pthread_rwlock_destroy(&commandsLocker)) \
        exit(EXIT_FAILURE); \
    for (int i = 0; i < A; i++) \
        if (pthread_rwlock_destroy(&vecLock[i])); }
#else
#define INIT(A) {}
#define LOCK(A) {}
#define UNLOCK(A) {}
#define DESTROY(A) {}
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
    sem_wait(&sem_prod);   
    LOCK(commandsLocker);
    if (numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[(numberCommands + headQueue) % MAX_COMMANDS], data);
        numberCommands++;
        UNLOCK(commandsLocker);
        sem_post(&sem_cons);
        return 1;
    }
    UNLOCK(commandsLocker);
    return 0;
}

char* removeCommand() {
    if (numberCommands > 0) {
        numberCommands--;
        char* command = inputCommands[headQueue % MAX_COMMANDS];
        headQueue = (headQueue + 1) % MAX_COMMANDS;
        return command;
    }
    return NULL;
}

void errorParse() {
    fprintf(stderr, "Error: command invalid\n");
}

void* processInput() {
    FILE* fptr  = fopen(fileInput, "r");
    char line[MAX_INPUT_SIZE];
    while (fgets(line, sizeof(line)/sizeof(char), fptr)) {
        char token;
        char name[MAX_INPUT_SIZE]; 
        char rname[MAX_INPUT_SIZE];
        int numTokens = sscanf(line, "%c %s", &token, name);
        if (numTokens < 1)
            continue;
        switch (token) {
            case 'r':
                numTokens = sscanf(line, "%c %s %s", &token, name, rname);
                if (numTokens == 3) {
                    insertCommand(line);
                    break;
                }
            case 'c':
            case 'l':
            case 'd':
                if (numTokens != 2)
                    errorParse();
                else if (insertCommand(line))
                    break;
                return NULL;
            case '#':
                break;
            default:
                errorParse();
        }
    }
    fclose(fptr);
    insertCommand("x");
    return NULL;
}

void* applyCommands() {
    while (1) {
        sem_wait(&sem_cons);
        LOCK(commandsLocker);
        if (!numberCommands) {
            fprintf(stderr, "Error: invalid number of commands\n");
            exit(EXIT_FAILURE);
        }
        const char* command = removeCommand();
        if (!strcmp(command, "x")) {
            headQueue--;
            numberCommands++;
            UNLOCK(commandsLocker);
            sem_post(&sem_cons);
            break;
        }
        int iNumber;
        if (command && command[0] == 'c')
            iNumber = obtainNewInumber(fs);
        UNLOCK(commandsLocker);
        sem_post(&sem_prod);
        if (!command)
            continue;
        char token;
        char name[MAX_INPUT_SIZE];
        char rname[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s", &token, name);
        if (numTokens != 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        int searchResult1;
        int searchResult2;
        switch (token) {
            case 'c':
                LOCK(vecLock[hash(name, numberBuckets)]);
                create(fs, name, iNumber, numberBuckets);
                UNLOCK(vecLock[hash(name, numberBuckets)]);
                break;
            case 'l':
                LOCK(vecLock[hash(name, numberBuckets)]);
                searchResult1 = lookup(fs, name, numberBuckets);
                UNLOCK(vecLock[hash(name, numberBuckets)]);
                if (!searchResult1)
                    printf("%s not found\n", name);
                else
                    printf("%s found with inumber %d\n", name, searchResult1);
                break;
            case 'd':
                LOCK(vecLock[hash(name, numberBuckets)]);
                delete(fs, name, numberBuckets);
                UNLOCK(vecLock[hash(name, numberBuckets)]);
                break;
            case 'r':
                numTokens = sscanf(command, "%c %s %s", &token, name, rname);
                if (hash(name, numberBuckets) <= hash(rname, numberBuckets)) {
                    LOCK(vecLock[hash(name, numberBuckets)]);
                    searchResult1 = lookup(fs, name, numberBuckets);
                    UNLOCK(vecLock[hash(name, numberBuckets)]);
                    if (!searchResult1) {
                        printf("%s file not found\n", name);
                        break;
                    }
                    LOCK(vecLock[hash(rname, numberBuckets)]);
                    searchResult2 = lookup(fs, rname, numberBuckets);
                    UNLOCK(vecLock[hash(rname, numberBuckets)]);
                    if (searchResult2) {
                        printf("%s file already exists\n", rname);
                        break;
                    }
                    LOCK(vecLock[hash(name, numberBuckets)]);
                    delete(fs, name, numberBuckets);
                    create(fs, rname, searchResult1, numberBuckets);
                    UNLOCK(vecLock[hash(name, numberBuckets)]);
                    break;
                }
                else {
                    LOCK(vecLock[hash(rname, numberBuckets)]);
                    searchResult2 = lookup(fs, rname, numberBuckets);
                    UNLOCK(vecLock[hash(rname, numberBuckets)]);
                    if (searchResult2) {
                        printf("%s file already exists\n", rname);
                        break;
                    }
                    LOCK(vecLock[hash(name, numberBuckets)]);
                    searchResult1 = lookup(fs, name, numberBuckets);
                    UNLOCK(vecLock[hash(name, numberBuckets)]);
                    if (!searchResult1) {
                        printf("%s file not found\n", name);
                        break;
                    }
                    LOCK(vecLock[hash(name, numberBuckets)]);
                    delete(fs, name, numberBuckets);
                    create(fs, rname, searchResult1, numberBuckets);
                    UNLOCK(vecLock[hash(name, numberBuckets)]);
                    break;
                }
            default: {
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}

void applyThread() {
    INIT(numberBuckets);
    pthread_t processor;
    pthread_t workers[numberThreads];
    sem_init(&sem_prod, 0, MAX_COMMANDS);
    sem_init(&sem_cons, 0, 0);
    int err1 = pthread_create(&processor, NULL, processInput, NULL);
    if (err1 != 0) {
      perror("Can't create thread\n");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numberThreads; i++) {
        int err2 = pthread_create(&workers[i], NULL, applyCommands, NULL);
        if (err2 != 0) {
            perror("Can't create thread\n");
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_join(processor, NULL)) perror("Can't join thread\n");
    for (int i = 0; i < numberThreads; i++) {
        if (pthread_join(workers[i], NULL)) {
            perror("Can't join thread\n");
        }
    }
    sem_destroy(&sem_prod);
    sem_destroy(&sem_cons);
    DESTROY(numberBuckets);
}

int main(int argc, char* argv[]) {
    struct timeval start, end;
    double seconds, micros;
    parseArgs(argc, argv);
    gettimeofday(&start, NULL);
    fs = new_tecnicofs(numberBuckets);
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
