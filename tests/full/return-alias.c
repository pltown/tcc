int* f(void *pi) {
    return (int*)pi;
}

int* g(int *pa, int *pb) {
    return pa;
}

int h(void *pa, int *pb) {
    return *(int*)pa + *pb;
}

int main() {
    int i = 0;
    int *j = f(&i);
    int *k = g(&i, j);
    int l = h(&i, j);

    return 0;
}

