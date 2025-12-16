#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>

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
int do_pwd(char *args);
int do_cd(char * args);

// --- GLOBAL REGISTRY ---
Builtin builtins[] = {
  {"echo", do_echo},
  {"exit", do_exit},
  {"type", do_type},
  {"pwd",  do_pwd},
  {"cd", do_cd},
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
int do_cd(char * args){
  char *path = args;
  if (path == NULL || strcmp(path, "~") == 0){
    path = getenv("HOME");
    if (path == NULL){
      printf("cd: HOME not set\n");
      return -1;
    }
  }
  if (chdir(path) !=0){
    printf("cd: %s: No such file or directory\n", path);
    return -1;
  }
  return 0;
}
int do_pwd(char *args){
  char cwd[1024]; 
    // getcwd(buffer, size) fills the buffer with the current directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd error");
        return 1;
    }
    return 0;
}
int do_type(char *args) {
  if (args == NULL) return 1;

  for (int i = 0; builtins[i].name != NULL; i++) {
    if (strcmp(args, builtins[i].name) == 0) {
      printf("%s is a shell builtin\n", args);
      return 0;
    }
  }

  char *path_env = getenv("PATH");
  if (path_env == NULL) return 1;

  char *path_copy = strdup(path_env);
  char *dir = strtok(path_copy, ":");
  char full_path[1024];
  int found = 0;

  while (dir != NULL) {
    // Concatenate 'dir' + '/' + 'args' 
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, args);
    // Check if that file exists using access()
    if (access(full_path, X_OK) == 0) {
      printf("%s is %s\n", args, full_path);
      found = 1;
      break;
    }
    dir = strtok(NULL, ":");
  }
  if(!found){
    printf("%s: not found\n", args);
    return 0;
  }
  free(path_copy);
  return 1;
}

// --- MAIN LOOP ---
int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);
    
    char inp[1024];
    
    printf("$ ");

    while (fgets(inp, 1024, stdin) != NULL) {
        // Clean up input (remove newline)
        inp[strcspn(inp, "\n")] = '\0';
        
        // Parse Command vs Args
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

        // Check Builtins
        int found_builtin = 0; 
        for (int i = 0; builtins[i].name != NULL; i++) {
            if (strcmp(command, builtins[i].name) == 0) {
                int result = builtins[i].handler(args);
                if (result == -1) return 0; // Exit shell
                found_builtin = 1;
                break;
            }
        }

        // Run External Program (if not a builtin)
        if (!found_builtin) {
            pid_t pid = fork();

            if (pid == 0) {
                // --- CHILD PROCESS ---
                char *cmd_args[100];
                cmd_args[0] = command;
                int idx = 1;

                // Split args for execvp
                if (args != NULL) {
                    char *token = strtok(args, " ");
                    while (token != NULL) {
                        cmd_args[idx++] = token;
                        token = strtok(NULL, " ");
                    }
                }
                cmd_args[idx] = NULL; // Null terminate the list

                // execvp automatically searches PATH
                execvp(command, cmd_args);

                // If we get here, execvp failed (command not found)
                printf("%s: command not found\n", command);
                exit(1); 
            } else if (pid > 0) {
                // --- PARENT PROCESS ---
                int status;
                wait(&status);
            } else {
                perror("fork");
            }
        }
        
        printf("$ ");
    }

    return 0;
}