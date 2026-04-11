int* f(void *pi) {
    return (int*)pi;
}

int* g(int *pa, int *pb) {
    return pa;
}

int h(void *pa, int *pb) {
    return *(int*)pa + *pb;
}

int* f2(void *pi) {
    return (int*)pi;
}
int* hof(int *pa, int*(*pf)(void*)) {
    return pf(pa);
}

int main() {
    int i = 0;
    int *j = f(&i);
    int *k = g(&i, j);
    int l = h(&i, j);

    int *m = hof(j, f2);

    return 0;
}

