#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmpstr(void* v1, void* v2)
{
    char *a1 = *(char**)v1;
    char *a2 = *(char**)v2;
    return strcmp(a1, a2);
}

int cmpnum(void* s1, void* s2)
{
    int *a = (int*)s1;
    int *b = (int*)s2;
    if ((*a) > (*b))
        return 1;
    else if ((*a) < (*b))
        return -1;
    else
        return 0;
}

void swap(void* v1, void* v2, int size)
{
    char buffer[size];
    memcpy(buffer, v1, size);
    memcpy(v1, v2, size);
    memcpy(v2, buffer, size);
}

// v is an array of elements to sort.
// size is the number of elements in array
// left and right is start and end of array
//(*comp)(void*, void*) is a pointer to a function
// which accepts two void* as its parameter
void _qsort(void* v, int size, int left, int right,
                    int (*comp)(void*, void*))
{
    void *vt, *v3;
    int i, last;
    int mid = (left + right) / 2;
    if (left >= right)
        return;

    void* vl = (char*)v + (left * size);
    void* vr = (char*)v + (mid * size);
    swap(vl, vr, size);
    last = left;
    for (i = left + 1; i <= right; i++) {

        // vl and vt will have the starting address
        // of the elements which will be passed to
        // comp function.
        vt = (char*)(v + (i * size));
        if (comp(vl, vt) > 0) {
            ++last;
            v3 = (char*)(v + (last * size));
            swap(vt, v3, size);
        }
    }
    v3 = (char*)(v + (last * size));
    swap(vl, v3, size);
    _qsort(v, size, left, last - 1, comp);
    _qsort(v, size, last + 1, right, comp);
}

typedef int (*FPTR)(void*, void*);

double* f(void *pv) {
	return (double*)pv;
}

int g() {
	int i = 1;
	int *pi = &i;
	void *pv = f(pi);
	int *pi2 = (int*)pv;
}

int main() {
    char* words[] = {"bbc", "xcd", "ede", "def"};
    FPTR pf = cmpstr; // [pf = cmpstr]
    _qsort(words, sizeof(char*), 0, 8, pf); // [.$0 = words; .. ; $2 = pf] + ;

    int numbers[] = { 45, 78, 89, 65, 70, 23, 44 };
    pf = cmpnum;
    _qsort(numbers, sizeof(int), 0, 6, cmpnum); // [.$0 = words; .. ; $2 = pf] + [pf = cmpnum];

    // What happens to history of words/numbers? Do they each get extended with history of cmpnum?
    // Or does words get extended by cmpstr and numbers by cmpnum?
    // Or something else?
    g();
}

