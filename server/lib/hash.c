#include "hash.h"

int hash(char* name, int n) {
	if (!name) return -1;
	return (int) name[0] % n;
}
