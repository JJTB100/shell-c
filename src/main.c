#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- TYPE DEFINITIONS ---
typedef int (*builtin_handler)(char *args);
typedef struct {
    const char *name;
    builtin_handler handler;
} Builtin;

// --- FUNCTION PROTOTYPES ---
int do_echo(char *args);
int do_exit(char *args);
int do_type(char *args);

// --- GLOBAL REGISTRY ---
Builtin builtins[] = {
    {"echo", do_echo},
    {"exit", do_exit},
    {"type", do_type},
    {NULL, NULL} // Marks end
};

// --- IMPLEMENTATIONS ---
int do_echo(char *args){
    if(args) printf("%s\n", args);
    else printf("\n");
    return 0;
}

int do_exit(char *args) {
    return -1; 
}

int do_type(char *args) {
    if (args == NULL) return 1;

    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(args, builtins[i].name) == 0) {
            printf("%s is a shell builtin\n", args);
            return 0;
        }
    }

    // Placeholder for PATH search later
    printf("%s: not found\n", args);
    return 1;
}

// --- MAIN LOOP ---
int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    printf("$ ");

    char inp[1024];

    while (fgets(inp, 1024, stdin) != NULL) {
        // Remove trailing new line
        inp[strcspn(inp, "\n")] = '\0';
        
        char *space_pos = strchr(inp, ' ');
        char *command = NULL;
        char *args = NULL;

        if (space_pos != NULL){
            *space_pos = '\0';
            command = inp;
            args = space_pos + 1;
            while(*args == ' ') args++; 
        } else {
            command = inp;
            args = NULL;
        }

        int found = 0; 

        for (int i = 0; builtins[i].name != NULL; i++) {
            if (strcmp(command, builtins[i].name) == 0) {
                int result = builtins[i].handler(args);
                
                if (result == -1) return 0; // Exit successfully
                
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("%s: command not found\n", command);
        }
        
        printf("$ ");
    }

    return 0;
}