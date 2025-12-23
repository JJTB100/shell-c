#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "builtins.h"


// --- GLOBAL REGISTRY ---
Builtin builtins[] = {
  {"echo", do_echo},
  {"exit", do_exit},
  {"type", do_type},
  {"pwd",  do_pwd},
  {"cd", do_cd},
  {"history", do_history},
  {NULL, NULL} // Marks end
};

// --- IMPLEMENTATIONS ---
int do_history(char **argv){
  return 0;
}
int do_echo(char **argv) {
  for (int i = 1; argv[i] != NULL; i++) {
    printf("%s", argv[i]);
    if (argv[i + 1] != NULL) printf(" ");
  }
  printf("\n");
  return 0;
}

int do_exit(char **argv) {
  (void) argv;
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
  (void) argv;
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

int handle_builtin(char *command, char **argv, char *out_file, char *err_file, int append_out, int append_err) {
  // Check Builtins
  for (int i = 0; builtins[i].name != NULL; i++) {
    if (strcmp(command, builtins[i].name) == 0) {
      
      int saved_stdout = -1;
      int saved_stderr = -1;
      if (out_file != NULL) {
        saved_stdout = dup(STDOUT_FILENO);
        int flags = O_WRONLY | O_CREAT | (append_out ? O_APPEND : O_TRUNC);
        int fd = open(out_file, flags, 0644);
        if (fd == -1) { perror("open"); return 1; }
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }
      if (err_file != NULL) {
        saved_stderr = dup(STDERR_FILENO);
        int flags = O_WRONLY | O_CREAT | (append_err ? O_APPEND : O_TRUNC);
        int fd = open(err_file, flags, 0644);
        if (fd == -1) { perror("open"); return 1; }
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
      
      // Pass the entire argv array to the handler
      int result = builtins[i].handler(argv);
      
      if (out_file != NULL) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
      if (err_file != NULL) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
      
      if (result == -1) return -1; // Exit shell
      return 1; // Handled
    }
  }
  return 0; // Not a builtin
}