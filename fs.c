#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"

int obtainNewInumber(tecnicofs* fs) {
	int i = ++fs->nextINumber;
    return i;
}

tecnicofs* new_tecnicofs(int numberBuckets) {
	tecnicofs* fs = (tecnicofs*) malloc(sizeof(struct tecnicofs));
    fs->vector = (node**) malloc(numberBuckets*sizeof(struct node));
    if (!fs) {
        perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}
    fs->nextINumber = 0;
	for (int i = 0; i < numberBuckets; i++) {
        fs->vector[i] = NULL;
    }
	return fs;
}

void free_tecnicofs(tecnicofs* fs, int numberBuckets) {
    for (int i = 0; i < numberBuckets; i++) {
        free_tree(fs->vector[i]);
    }
    free(fs->vector);
	free(fs);
}

void create(tecnicofs* fs, char* name, int inumber, int numberBuckets) {
    int i = hash(name, numberBuckets);
	fs->vector[i] = insert(fs->vector[i], name, inumber);
}

void delete(tecnicofs* fs, char* name, int numberBuckets) {
    int i = hash(name, numberBuckets);
	fs->vector[i] = remove_item(fs->vector[i], name);
}

int lookup(tecnicofs* fs, char* name, int numberBuckets) {
    int i = hash(name, numberBuckets);
	node* searchNode = search(fs->vector[i], name);
	if (searchNode)
        return searchNode->inumber;
	return 0;
}

void print_tecnicofs_tree(FILE* fp, tecnicofs* fs, int numberBuckets) {
    for (int i = 0; i < numberBuckets; i++) {
	    print_tree(fp, fs->vector[i]);
    }
}
