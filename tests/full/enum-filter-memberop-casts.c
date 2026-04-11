// Generated with Copilot GPT-4.1
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Minimal BOOL type for demo
typedef int BOOL;

// Type codes
#define BINN_INT      0x01
#define BINN_STRING   0x02
#define BINN_BOOL     0x03

// Minimal JSON value type for output
typedef struct {
    int type;
    union data_t {
        int    int_val;
        char   str_val[128];
        BOOL   bool_val;
    } data;
} json_t;

// Minimal binn_struct definition with only three supported types
typedef struct {
    int    header;
    int    type;
    union binn_data_t {
        int   vint32;
        char  vstr[128];
        BOOL  vbool;
    } data;
} binn;

// Mimic json_string function from the repo (returns pointer to string in json_t)
const char* json_string(const json_t* j) {
    if (j->type == BINN_STRING) {
        return j->data.str_val;
    }
    return "";
}

// Mimic binn_get_str function from the repo
const char* binn_get_str(const binn* value) {
    static char buf[128]; // static for demo purpose only!
    switch (value->type) {
        case BINN_STRING:
            return value->data.vstr;
        case BINN_INT:
            snprintf(buf, sizeof(buf), "%d", value->data.vint32);
            return buf;
        case BINN_BOOL:
            return value->data.vbool ? "true" : "false";
        default:
            return "";
    }
}

// Converts binn_struct to json_t
json_t value_from_binn(const binn* b) {
    json_t result;
    result.type = b->type;
    switch (b->type) {
        case BINN_INT: {
            result.data.int_val = b->data.vint32;
            break;
                       }
        case BINN_STRING: {
            strncpy(result.data.str_val, b->data.vstr, sizeof(result.data.str_val) - 1);
            result.data.str_val[sizeof(result.data.str_val) - 1] = '\0'; // ensure null-termination
            break;
                          }
        case BINN_BOOL: {
            result.data.bool_val = b->data.vbool;
            break;
                        }
    }
    return result;
}

// Displays json_t info
void display_json(const json_t* j) {
    printf("{ ");
    int vint;
    const char * vstr;
    const char * vboolstr;
    switch (j->type) {
        case BINN_INT: {
            vint = j->data.int_val;
            printf("\"type\": \"int\", \"value\": %d", vint);
            break;
                       }
        case BINN_STRING: {
            vstr = json_string(j);
            printf("\"type\": \"string\", \"value\": \"%s\"", vstr);
            break;
                          }
        case BINN_BOOL: {
            //vboolstr = j->data.bool_val ? "true" : "false";
            if(j->data.bool_val) {
                vboolstr = "true";
            }
            else {
                vboolstr = "false";
            }
            printf("\"type\": \"bool\", \"value\": %s", vboolstr);
            break;
                        }
        default: {
            printf("\"type\": \"unknown\"");
                 }
    }
    printf(" }\n");
}

int main() {
    binn bint = {0x1F22B11F, BINN_INT, .data.vint32 = -123};
    binn bstring = {0x1F22B11F, BINN_STRING, .data.vstr = "Hello, BINN"};
    binn bbool_true = {0x1F22B11F, BINN_BOOL, .data.vbool = 1};
    binn bbool_false = {0x1F22B11F, BINN_BOOL, .data.vbool = 0};

    json_t j1 = value_from_binn(&bint);
    json_t j2 = value_from_binn(&bstring);
    json_t j3 = value_from_binn(&bbool_true);
    json_t j4 = value_from_binn(&bbool_false);

    display_json(&j1);
    display_json(&j2);
    display_json(&j3);
    display_json(&j4);

    // demo binn_get_str usage
    printf("\nDemo binn_get_str:\n");
    printf("bint: %s\n", binn_get_str(&bint));
    printf("bstring: %s\n", binn_get_str(&bstring));
    printf("bbool_true: %s\n", binn_get_str(&bbool_true));
    printf("bbool_false: %s\n", binn_get_str(&bbool_false));

    return 0;
}

