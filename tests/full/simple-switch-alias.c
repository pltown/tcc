int main() {
    int i = 1;
    int *pi = 0;
    char *pc = 0;
    switch(i) {
        case 1: {pi = &i;
                break;}
        case 2: {pc = (char*)&i;
                break;}
    }

    return 0;
}
