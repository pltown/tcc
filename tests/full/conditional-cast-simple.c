int flag = 0;

void flaggedOp(void *pv) {
    int *i = 0;
    char *c = 0;
    switch(flag) {
        case 0: {
            i = (int*) pv;
            *i = *i + 1;
            break;
        }
        case 1: {
            c = (char*) pv;
            *c = 'a';
            //printf("%c", *c);
            break;
        }
    }
}

int main() {
    int x = 1;
    flaggedOp(&x);
    return 0;
}

