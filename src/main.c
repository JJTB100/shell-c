#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>

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

struct termios orig_termios;

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

void disableRawMode(){
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(){
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// Returns a NULL-terminated array of matching strings. 
// Sets *match_count to the number of matches found.
char **get_all_matches(char *prefix, int *match_count) {
    int capacity = 10;
    int count = 0;
    char **matches = malloc(capacity * sizeof(char *));
    int prefix_len = strlen(prefix);

    // Search Builtins
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strncmp(prefix, builtins[i].name, prefix_len) == 0) {
            if (count >= capacity) {
                capacity *= 2;
                matches = realloc(matches, capacity * sizeof(char *));
            }
            matches[count++] = strdup(builtins[i].name);
        }
    }

    // Search PATH
    char *path_env = getenv("PATH");
    if (path_env != NULL) {
        char *path_copy = strdup(path_env);
        char *dir = strtok(path_copy, ":");

        while (dir != NULL) {
            DIR *d = opendir(dir);
            if (d != NULL) {
                struct dirent *entry;
                while ((entry = readdir(d)) != NULL) {
                    // Check prefix match
                    if (strncmp(prefix, entry->d_name, prefix_len) == 0 &&
                        strcmp(entry->d_name, ".") != 0 &&
                        strcmp(entry->d_name, "..") != 0) {
                        
                        // Check for duplicates before adding
                        int is_duplicate = 0;
                        for(int k=0; k<count; k++) {
                            if(strcmp(matches[k], entry->d_name) == 0) {
                                is_duplicate = 1;
                                break;
                            }
                        }
                        if(is_duplicate) continue;

                        if (count >= capacity) {
                            capacity *= 2;
                            matches = realloc(matches, capacity * sizeof(char *));
                        }
                        matches[count++] = strdup(entry->d_name);
                    }
                }
                closedir(d);
            }
            dir = strtok(NULL, ":");
        }
        free(path_copy);
    }

    // Sort matches alphabetically
    qsort(matches, count, sizeof(char *), compare_strings);

    matches[count] = NULL; // Null-terminate the array
    *match_count = count;
    return matches;
}
int read_input_with_autocomplete(char *buffer, size_t size) {
  int pos = 0;
  char c;
  int tab_count = 0;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\n') {
      buffer[pos] = '\0';
      printf("\n");
      return 0;
    } 
    else if (c == '\t') {
      tab_count++;
      int match_count;
      char **matches = get_all_matches(buffer, &match_count);
      if (match_count == 0){
        printf("\a");
      } else if (match_count == 1){
        // Auto complete
        const char *to_add = &matches[0][pos];
          
        strcat(buffer, to_add);
        printf("%s", to_add);
        
        // Add trailing space
        strcat(buffer, " ");
        printf(" ");
        
        pos += strlen(to_add) + 1;
      } else{ // higher match_count
        if (tab_count == 1){
          printf("\a");
        } else if (tab_count == 2){
          // Print the list
          printf("\n");
          for(int match_no=0; match_no<match_count; match_no++){
            printf("%s  ", matches[match_no]);
          }
          printf("\n$ %s", buffer);
          // Restore the cursor -> printf the prompt "$ " and the buffer contents
        }
      }
      
    } 
    else if (c == 127) { // Backspace
      if (pos > 0) {
        pos--;
        printf("\b \b"); // Move back, print space, move back again
      }
    } 
    else {
      if (size > 0 && (size_t)pos < size - 1) {
        buffer[pos++] = c;
        buffer[pos] = '\0';
        printf("%c", c);
      }
    }
  }
  return -1;
}

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