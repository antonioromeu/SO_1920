#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"

int obtainNewInumber(tecnicofs* fs) {
	int newInumber = ++(fs->nextINumber);
	return newInumber;
}

tecnicofs* new_tecnicofs() {
	tecnicofs* fs = (tecnicofs*) malloc(numberBuckets*sizeof(tecnicofs));
    if (!fs) {
        perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}
    int nextINumber = 0;
	for (int i = 0; i < numberBuckets; i++) {
        fs[i] = create;
        fs[i]->bstRoot = NULL;
    }
	return fs;
}

void free_tecnicofs(tecnicofs* fs, int numberBuckets) {
    for (int i = 0; i < numberBuckets; i++) {
        free_tree(fs[i]->bstRoot);
    }
	free(fs);
}

void create(tecnicofs* fs, char* namen int inumber) {
	fs->bstRoot = insert(fs->bstRoot, name, inumber);
}

void delete(tecnicofs* fs, char* name) {
	fs->bstRoot = remove_item(fs->bstRoot, name);
}

int lookup(tecnicofs* fs, char* name) {
	node* searchNode = search(fs->bstRoot, name);
	if (searchNode)
        return searchNode->inumber;
	return 0;
}

void print_tecnicofs_tree(FILE* fp, tecnicofs* fs) {
	print_tree(fp, fs->bstRoot);
}
