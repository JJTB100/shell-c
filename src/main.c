#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "builtins.h"
#include "input.h"
typedef struct Command {
    char **argv;           // Array of strings for execvp (e.g., {"ls", "-l", NULL})
    int argc;              // Number of arguments
    
    // -- Redirection Support (Per-Command) --
    char *input_file;      // For '<' (mostly used by the first command)
    char *output_file;     // For '>' or '>>'
    char *error_file;      // For '2>'
    int append_out;        // Flag: 1 if '>>', 0 if '>'
    int append_err;
    // -- Pipeline Link --
    struct Command *next;  // Pointer to the next command in the pipeline (or NULL)
} Command;
int is_builtin(char *name) {
    if (name == NULL) return 0;
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(name, builtins[i].name) == 0) return 1;
    }
    return 0;
}
Command * create_command() {
    Command *cmd = malloc(sizeof(Command));
    if (!cmd) return NULL;
    
    cmd->argv = malloc(10 * sizeof(char*)); // Start with space for 10 args
    cmd->argv[0] = NULL;
    cmd->argc = 0;
    
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->error_file = NULL;
    cmd->append_out = 0;
    cmd->append_err = 0;
    
    cmd->next = NULL;
    return cmd;
}
Command * parse(int num_token, char **argv){
  
  Command *current_cmd = create_command();
  Command *first_cmd = current_cmd;
  for (int i=0; i<num_token; i++){
    if(strcmp(argv[i], "|")==0){
      Command *next_cmd = create_command();
      current_cmd->next = next_cmd;
      current_cmd = next_cmd;

    } else if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
      if (argv[i+1] == NULL) continue;      
      current_cmd->output_file = argv[i+1];
      i++;
    } else if (strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) {
      if (argv[i+1] == NULL) continue;
      current_cmd->output_file = argv[i+1];
      current_cmd->append_out = 1; // Set append flag
      i++;
    } else if (strcmp(argv[i], "2>") == 0) {
      if (argv[i+1] == NULL) continue;
      current_cmd->error_file = argv[i+1];
      current_cmd->append_err = 0;
      i++;
    } else if (strcmp(argv[i], "2>>") == 0) {
      if (argv[i+1] == NULL) continue;
      current_cmd->error_file = argv[i+1];
      current_cmd->append_err = 1;
      i++;
    } else if (strcmp(argv[i], "<") == 0) {
      if (argv[i+1] == NULL) {
          printf("Syntax error: expected file after <\n");
          continue;
      }
      current_cmd->input_file = argv[i+1];
      i++;
    } else{
      current_cmd->argv[current_cmd->argc] = argv[i];
      current_cmd->argc++;
      current_cmd->argv[current_cmd->argc] = NULL;
    }
  }
  return first_cmd;
}

void free_commands(Command *head) {
    Command *current = head;
    while (current != NULL) {
        Command *next = current->next;
        
        // We only malloc'd 'argv' and the struct itself.
        // The strings inside argv point to 'inp', so we don't free them here.
        if (current->argv) free(current->argv);
        free(current);
        
        current = next;
    }
}
void execute_pipeline(Command *head){
  int prev_read_fd = -1;
  int pipe_fd[2];

  Command *cmd = head;
  while (cmd != NULL){
    // Create a pipe
    if (cmd->argv[0] == NULL) {
        cmd = cmd->next;
        continue;
    }
    if (cmd->next != NULL){
      if (pipe(pipe_fd)==-1){perror("pipe");return;}
    }
    pid_t pid = fork();

    if(pid==0){
      // --- Child Process ---
      
      // Connect Input
      if(cmd->input_file != NULL){
        int fd = open(cmd->input_file, O_RDONLY);
        if(fd == -1){ perror("open input"); _exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
      } 
      else if(prev_read_fd != -1){
        dup2(prev_read_fd, STDIN_FILENO);
        close(prev_read_fd);
      }


      // Connect Output
      if (cmd->next != NULL) close(pipe_fd[0]);

      if(cmd->output_file != NULL){
        int flags = O_WRONLY | O_CREAT | (cmd->append_out ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if(fd == -1){ perror("open output"); _exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);

        if (cmd->next != NULL) close(pipe_fd[1]);
      } else if (cmd->next != NULL){
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
      }

      if(cmd->error_file != NULL){
        int flags = O_WRONLY | O_CREAT | (cmd->append_err ? O_APPEND : O_TRUNC);
        int fd = open(cmd->error_file, flags, 0644);
        if(fd == -1){ perror("open error"); _exit(1); }
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
      // Execute
      if (handle_builtin(cmd->argv[0], cmd->argv, NULL, NULL, 0, 0)) {
         _exit(0); // Success
      }
      execvp(cmd->argv[0], cmd->argv);
      fprintf(stderr, "%s: command not found\n", cmd->argv[0]);
      _exit(1);
    } else{
      // --- PARENT PROCESS ---
      // cleanup
      if (prev_read_fd != -1){
        close(prev_read_fd);
      }

      // Set up input for the NEXT command
      if (cmd->next != NULL){
        close(pipe_fd[1]);
        prev_read_fd = pipe_fd[0];
      }
    }

    cmd = cmd->next;
  }
  while(wait(NULL)>0);
}
// --- MAIN LOOP ---
int main(int argc, char *main_argv[]) {
  load_last_line_saved(getenv("HISTFILE"));
  (void) argc;
  (void) main_argv;
  setbuf(stdout, NULL);
  int interactive = isatty(STDIN_FILENO);
  if(interactive)  enableRawMode();

  char inp[1024];

  while (1) {
    if(interactive){
      printf("$ ");
      fflush(stdout);
      if (read_input_with_autocomplete(inp, 1024) != 0) break;
    } else{
      if (fgets(inp, sizeof(inp), stdin) == NULL) break;
      size_t len = strlen(inp);
      if (len > 0 && inp[len - 1] == '\n') {
          inp[len - 1] = '\0';
      }
    }

    FILE *fptr = fopen(getenv("HISTFILE"), "a");
    fprintf(fptr, "%s\n", inp);
    fclose(fptr);
    
    char *argv[100];
    int num_token = tokenise(inp, argv);

    if (num_token == 0) {
      continue;
    }

    Command *first_cmd = parse(num_token, argv);    
    if (!first_cmd || !first_cmd->argv[0]) {
        free_commands(first_cmd);
        continue;
    }

    if (first_cmd->next == NULL && is_builtin(first_cmd->argv[0])) {
        int status = handle_builtin(first_cmd->argv[0], first_cmd->argv, 
                       first_cmd->output_file, first_cmd->error_file, 
                       first_cmd->append_out, first_cmd->append_err);
    
        if (status == -1) {
            free_commands(first_cmd);
            exit(0); // Actually exit the shell
        }
    } else {
        // Run external commands OR pipelines (forking involved)
        execute_pipeline(first_cmd);
    }
    
    free_commands(first_cmd);
  }

  return 0;
}
