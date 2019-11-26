#ifndef FS_H
#define FS_H
#include "lib/bst.h"
#include "lib/hash.h"
#include "lib/inodes.h"

typedef struct tecnicofs {
    node** vector;
    int nextINumber;
} tecnicofs;

int obtainNewInumber(tecnicofs* fs);
tecnicofs* new_tecnicofs(int numberBuckets);
void free_tecnicofs(tecnicofs* fs, int numberBuckets);
void createFS(tecnicofs* fs, char* name, int inumber, int numberBuckets);
void delete(tecnicofs* fs, char* name, int numberBuckets);
int lookup(tecnicofs* fs, char* name, int numberBuckets);
void print_tecnicofs_tree(FILE* fp, tecnicofs* fs, int numberBuckets);

#endif
