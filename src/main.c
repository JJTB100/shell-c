#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

// --- TYPE DEFINITIONS ---
typedef int (*builtin_handler)(char **argv);
typedef struct {
  const char *name;
  builtin_handler handler;
} Builtin;

// --- FUNCTION PROTOTYPES ---
int do_echo(char **argv);
int do_exit(char **argv);
int do_type(char **argv);
int do_pwd(char **argv);
int do_cd(char **argv);

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
int do_echo(char **argv) {
  for (int i = 1; argv[i] != NULL; i++) {
    printf("%s", argv[i]);
    if (argv[i + 1] != NULL) printf(" ");
  }
  printf("\n");
  return 0;
}

int do_exit(char **argv) {
  return -1;
}

int do_cd(char **argv) {
  // argv[0] is "cd", argv[1] is the path
  char *path = argv[1];
  
  if (path == NULL || strcmp(path, "~") == 0) {
    path = getenv("HOME");
    if (path == NULL) {
      printf("cd: HOME not set\n");
      return 1;
    }
  }
  
  if (chdir(path) != 0) {
    printf("cd: %s: No such file or directory\n", path);
    return 1;
  }
  return 0;
}

int do_pwd(char **argv) {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("pwd error");
    return 1;
  }
  return 0;
}

int do_type(char **argv) {
  // argv[0] is "type", argv[1] is the command to check
  if (argv[1] == NULL) return 1;
  char *target = argv[1];

  // Check Builtins
  for (int i = 0; builtins[i].name != NULL; i++) {
    if (strcmp(target, builtins[i].name) == 0) {
      printf("%s is a shell builtin\n", target);
      return 0;
    }
  }

  // Check PATH
  char *path_env = getenv("PATH");
  if (path_env == NULL) return 1;

  char *path_copy = strdup(path_env);
  char *dir = strtok(path_copy, ":");
  char full_path[1024];
  int found = 0;

  while (dir != NULL) {
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, target);
    if (access(full_path, X_OK) == 0) {
      printf("%s is %s\n", target, full_path);
      found = 1;
      break;
    }
    dir = strtok(NULL, ":");
  }
  
  if (!found) {
    printf("%s: not found\n", target);
  }
  
  free(path_copy);
  return 0;
}

int parse_input(char *input, char **argv){
  int argc = 0;
  int in_single_quotes = 0;
  int in_double_quotes = 0;
  char * current_token = input;

  // Loop through every character checking for a single quote
  for (int i = 0; input[i] != '\0'; i++) {
    if(input[i] == '\'' && !in_double_quotes){
      in_single_quotes = !in_single_quotes;
      // In place move all the characters left one
      memmove(&input[i], &input[i+1], strlen(&input[i+1]) + 1);
      i--;
      continue;
    }
    else if(input[i] == '"' && !in_single_quotes){
      in_double_quotes = !in_double_quotes;
      // In place move all the characters left one
      memmove(&input[i], &input[i+1], strlen(&input[i+1]) + 1);
      i--;
      continue;
    } else if(input[i] == '\\'){
      if(in_single_quotes){
      } else if (in_double_quotes){
        if(input[i+1] == '\\' || input[i+1] =='"' || input[i+1] =='$' || input[i+1] =='\n'){
          memmove(&input[i], &input[i+1], strlen(&input[i+1]) + 1);
        } 
      } else{
        if (input[i + 1] != '\0') {
          memmove(&input[i], &input[i + 1], strlen(&input[i + 1]) + 1);
        }
      }
    }
    else if(input[i]==' ' && !in_single_quotes && !in_double_quotes){
      input[i] = '\0';
      if (*current_token != '\0') {
        argv[argc++] = current_token;
      }
      current_token = &input[i + 1];
    }
  }
  if (*current_token != '\0') {
    argv[argc++] = current_token;
  }
  
  argv[argc] = NULL; // Null-terminate the list
  return argc;
}

// --- MAIN LOOP ---
int main(int argc, char *main_argv[]) {
  setbuf(stdout, NULL);
  printf("$ ");

  char inp[1024];

  while (fgets(inp, 1024, stdin) != NULL) {
    inp[strcspn(inp, "\n")] = '\0';

    char *argv[100];
    int argc = parse_input(inp, argv);

    if (argc == 0) {
      printf("$ ");
      continue;
    }

    char *command = argv[0];

    char *out_file = NULL;
    char *err_file = NULL;
    for (int i=0; argv[i] != NULL; i++){
      if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
        if (argv[i+1] == NULL) {
          printf("Syntax error: expected file after >\n");
          continue;
        }
        out_file = argv[i+1];
        argv[i] = NULL; // Cut the command off here
        break;
      } else if (strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) {
        if (argv[i+1] == NULL) continue;
        out_file = argv[i+1];
        append_out = 1; // Set append flag
        argv[i] = NULL;
      }// Check for Standard Error Redirection
      else if (strcmp(argv[i], "2>") == 0) {
          if (argv[i+1] == NULL) continue;
          err_file = argv[i+1];
          append_err = 0;
          argv[i] = NULL;
      }
      else if (strcmp(argv[i], "2>>") == 0) {
          if (argv[i+1] == NULL) continue;
          err_file = argv[i+1];
          append_err = 1;
          argv[i] = NULL;
      }
    }

    int found_builtin = 0;
    // Check Builtins
    for (int i = 0; builtins[i].name != NULL; i++) {
      if (strcmp(command, builtins[i].name) == 0) {
        found_builtin = 1;

        int saved_stdout = -1;
        int out_fd = -1;
        if (out_file != NULL) {
          saved_stdout = dup(STDOUT_FILENO);
          int flags = O_WRONLY | O_CREAT | (append_out ? O_APPEND : O_TRUNC);
          int fd = open(out_file, flags, 0644);
          if (fd == -1) { perror("open"); break; }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        if (err_file != NULL) {
          saved_stderr = dup(STDERR_FILENO);
          int flags = O_WRONLY | O_CREAT | (append_err ? O_APPEND : O_TRUNC);
          int fd = open(err_file, flags, 0644);
          if (fd == -1) { perror("open"); break; }
          dup2(fd, STDERR_FILENO);
          close(fd);
        }
        // Pass the entire argv array to the handler
        int result = builtins[i].handler(argv);
        if (out_file != NULL) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        if (err_file != NULL) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
        if (result == -1) return 0; // Exit shell
        
        break;
      }
    }

    // Run External Program
    if (!found_builtin) {

      pid_t pid = fork();

      if (pid == 0) {
        // Child Process
        // Redirect Stdout
        if (out_file != NULL) {
          int flags = O_WRONLY | O_CREAT | (append_out ? O_APPEND : O_TRUNC);
          int fd = open(out_file, flags, 0644);
          if (fd == -1) { perror("open out"); exit(1); }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }

        // Redirect Stderr
        if (err_file != NULL) {
          int flags = O_WRONLY | O_CREAT | (append_err ? O_APPEND : O_TRUNC);
          int fd = open(err_file, flags, 0644);
          if (fd == -1) { perror("open err"); exit(1); }
          dup2(fd, STDERR_FILENO);
          close(fd);
        }

        // execvp accepts the argv array directly
        execvp(command, argv);
        
        printf("%s: command not found\n", command);
        exit(1);
      } else if (pid > 0) {
        // Parent Process
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