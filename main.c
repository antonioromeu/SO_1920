#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include "fs.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

tecnicofs* fs;
char* fileInput = NULL;
char* fileOutput = NULL;
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberThreads = 0;
int numberCommands = 0;
int headQueue = 0;

#ifdef MUTEX
pthread_mutex_t locker1;
pthread_mutex_t locker2;
#define INIT() { if(pthread_mutex_init(&locker1, NULL)) exit(EXIT_FAILURE); if(pthread_mutex_init(&locker2, NULL)) exit(EXIT_FAILURE); }
#define LOCK(A) pthread_mutex_lock(&A)
#define UNLOCK(A) pthread_mutex_unlock(&A)
#define DESTROY() { if(pthread_mutex_destroy(&locker1)) exit(EXIT_FAILURE); if(pthread_mutex_destroy(&locker2)) exit(EXIT_FAILURE); }
#elif RWLOCK
pthread_rwlock_t locker1;
pthread_rwlock_t locker2;
#define INIT() { if(pthread_rwlock_init(&locker1, NULL)) exit(EXIT_FAILURE); if(pthread_rwlock_init(&locker2, NULL)) exit(EXIT_FAILURE); }
#define LOCK(A) pthread_rwlock_wrlock(&A)
#define UNLOCK(A) pthread_rwlock_unlock(&A)
#define DESTROY() { if(pthread_rwlock_destroy(&locker1)) exit(EXIT_FAILURE); if(pthread_rwlock_destroy(&locker2)) exit(EXIT_FAILURE); }
#else
#define INIT() {}
#define LOCK(A) {}
#define UNLOCK(A) {}
#define DESTROY() {}
#endif

static void displayUsage (const char* appName) {
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    fileInput = argv[1];
    fileOutput = argv[2];
    numberThreads = atoi(argv[3]);
    if (numberThreads != 1) {
        #ifdef MUTEX
        return;
        #endif
        #ifdef RWLOCK
        return;
        #endif
        numberThreads = 1;
    }
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
    char line[MAX_INPUT_SIZE];
    while(fgets(line, sizeof(line)/sizeof(char), fptr)) {
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
    }
    fclose(fptr);
}

void* applyCommands() {
    while(numberCommands > 0) {
        LOCK(locker1);
        const char* command = removeCommand();
        if (command == NULL) {
            continue;
        }
        char token;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s", &token, name);
        if (numTokens != 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        int searchResult;
        int iNumber;
        UNLOCK(locker1);
        switch (token) {
            case 'c':
                LOCK(locker2);
                iNumber = obtainNewInumber(fs);
                create(fs, name, iNumber);
                UNLOCK(locker2);
                break;
            case 'l':
                LOCK(locker2);
                searchResult = lookup(fs, name);
                UNLOCK(locker2);
                if(!searchResult)
                    printf("%s not found\n", name);
                else
                    printf("%s found with inumber %d\n", name, searchResult);
                break;
            case 'd':
                LOCK(locker2);
                delete(fs, name);
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
    pthread_t workers[numberThreads];
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
    DESTROY();
}

int main(int argc, char* argv[]) {
    struct timeval start, end;
    double seconds, micros;
    parseArgs(argc, argv);
    processInput();
    fs = new_tecnicofs();
    gettimeofday(&start, NULL);
    applyThread();
    gettimeofday(&end, NULL);
    FILE* fptr = fopen(fileOutput, "a");
    print_tecnicofs_tree(fptr, fs);
    fclose(fptr);
    free_tecnicofs(fs);
    seconds = (double) (end.tv_sec - start.tv_sec);
    micros = (double) ((seconds + (double) (end.tv_usec - start.tv_usec)/1000000));
    printf("TecnicoFS completed in %.4f seconds.\n", micros);
    exit(EXIT_SUCCESS);
}