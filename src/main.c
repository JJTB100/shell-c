#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "builtins.h"
#include "input.h"

void parse_redirections(char **argv, char **out_file, char **err_file, int *append_out, int *append_err) {
  for (int i=0; argv[i] != NULL; i++){
    if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
      if (argv[i+1] == NULL) {
        printf("Syntax error: expected file after >\n");
        continue;
      }
      *out_file = argv[i+1];
      argv[i] = NULL; // Cut the command off here
    } else if (strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) {
      if (argv[i+1] == NULL) continue;
      *out_file = argv[i+1];
      *append_out = 1; // Set append flag
      argv[i] = NULL;
    }
    // Check for Standard Error Redirection
    else if (strcmp(argv[i], "2>") == 0) {
      if (argv[i+1] == NULL) continue;
      *err_file = argv[i+1];
      *append_err = 0;
      argv[i] = NULL;
    }
    else if (strcmp(argv[i], "2>>") == 0) {
      if (argv[i+1] == NULL) continue;
      *err_file = argv[i+1];
      *append_err = 1;
      argv[i] = NULL;
    }
  }
}

void handle_external(char *command, char **argv, char *out_file, char *err_file, int append_out, int append_err) {
  // Run External Program
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
    _exit(1);
  } else if (pid > 0) {
    // Parent Process
    int status;
    wait(&status);
  } else {
    perror("fork");
  }
}

// --- MAIN LOOP ---
int main(int argc, char *main_argv[]) {
  (void) argc;
  (void) main_argv;
  setbuf(stdout, NULL);
  enableRawMode();
  char inp[1024];

  while (1) {
    printf("$ ");
    
    // Using the custom read function instead of fgets
    if (read_input_with_autocomplete(inp, 1024) != 0) break;

    char *argv[100];
    int parsed_argc = parse_input(inp, argv);

    if (parsed_argc == 0) {
      continue;
    }

    char *command = argv[0];

    char *out_file = NULL;
    char *err_file = NULL;
    int append_out = 0; // Flag for >>
    int append_err = 0; // Flag for 2>>

    parse_redirections(argv, &out_file, &err_file, &append_out, &append_err);

    int builtin_result = handle_builtin(command, argv, out_file, err_file, append_out, append_err);
    if (builtin_result == -1) return 0;
    if (builtin_result == 1) continue;

    handle_external(command, argv, out_file, err_file, append_out, append_err);
  }

  return 0;
}