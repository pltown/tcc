//https://stackoverflow.com/questions/252552/why-do-we-need-c-unions 

enum Type { INTS, FLOATS, DOUBLE };

struct S
{
    enum Type s_type;
    union
    {
        int s_ints[2];
        float s_floats[2];
        double s_double;
    };
};

void do_something(struct S *s)
{
    switch(s->s_type)
    {
        case INTS:    // do something with s->s_ints
            s->s_ints[0]++; // union_cast
            s->s_ints[1]++;
            break;

        case FLOATS:    // do something with s->s_floats
            s->s_floats[0]--;   // union_cast
            s->s_floats[1]--;
            break;

        case DOUBLE:    // do something with s->s_double
            s->s_double += 2;   // union_cast
            break;
    }
}

union {
    struct {
        unsigned char byte1;
        unsigned char byte2;
        unsigned char byte3;
        unsigned char byte4;
    } bytes;
    unsigned int dword;
} HW_Register;

struct HW_Register reg;

int main() {
    struct S s1 = {INTS, {{-1, -2}}};
    struct S s3 = {DOUBLE, {4.0}};
    struct S s4 = {INTS, {5.0}};
    do_something(&s1);
    do_something(&s3);
    do_something(&s4);

    reg.dword = 0x12345678;
    reg.bytes.byte3 = 9;

    return 0;
}
