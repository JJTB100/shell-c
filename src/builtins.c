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
int next_line_to_save = 0;
int session_start_line = 0;
void load_session_start(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) { 
    session_start_line = 0;
    next_line_to_save = 0;
    return;
  } 
  int count = 0;
  char c;
  // Efficiently count newlines
  for (c = getc(f); c != EOF; c = getc(f)) { 
        if (c == '\n') count++;
    }
  fclose(f);
  next_line_to_save = count;
  fprintf(stderr, "[DEBUG] Session start. Existing lines: %d\n", next_line_to_save);
}
// --- IMPLEMENTATIONS ---
int do_history(char **argv) {
  char * filename = getenv("HISTFILE");
  if (!argv[1]){
    FILE *fp = fopen(filename, "r");
    if (!fp) return 1;
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
      printf("    %d  %s", ++count, line);
    }
  } else if(strcmp(argv[1], "-r")==0){
    // READ FROM FILE
    if (!argv[2]) {
      printf("Error: Missing filename argument.\n");
      return 1;
    }
    FILE *readFrom = fopen(argv[2], "r");
    if(!readFrom){
      printf("Could not find file %s\n", argv[2]);
      return 1;
    }
    FILE *appendTo = fopen(filename, "a");

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), readFrom)) {
      fputs(buffer, appendTo);
    }
    fclose(readFrom);
    fclose(appendTo); 
    return 0;
  } else if (strcmp(argv[1], "-w") == 0) {
    if (!argv[2]) {
      printf("Error: Missing destination filename.\n");
      return 1;
    }

    FILE *src = fopen(filename, "r");
    // If no history exists yet, there is nothing to copy.
    if (!src) return 0; 

    FILE *dest = fopen(argv[2], "w");
    if (!dest) {
      printf("Error: Could not open %s for writing.\n", argv[2]);
      fclose(src);
      return 1;
    }

    char buffer[256];
    // Efficient copy loop
    while (fgets(buffer, sizeof(buffer), src)) {
      fputs(buffer, dest);
    }

    fclose(src);
    fclose(dest);
    return 0;
  } else if (strcmp(argv[1], "-a") == 0) {
    char *target_file;
    if (argv[2]) {
      target_file = argv[2];
    } else {
      target_file = filename;
    }
    FILE *fp = fopen(target_file, "a");
    if (!fp) {
      printf("Error: Could not open %s for appending.\n", target_file);
      return 1;
    }
    FILE *fp_session = fopen(filename, "r");
    if(strcmp(filename, target_file)==0){
      fclose(fp);
      fclose(fp_session);
      printf("Can't write to that filename.");
      return 1;
    }
    
    char buffer[1024];
    int line_num = 0;
    int lines_written = 0;
    while (fgets(buffer, sizeof(buffer), fp_session)) {
      //printf("%d, %d: ", last_line_saved, line_num);
      if(line_num >= next_line_to_save){
        //printf("Wrote\n");
        fputs(buffer, fp);
        lines_written++;
      }
      line_num++;
    }
    fprintf(stderr, "[DEBUG] history -a: Scanned %d lines, wrote %d lines to %s\n", 
                 line_num, lines_written, target_file);
    next_line_to_save = line_num;
    fclose(fp);
    fclose(fp_session);
    return 0;
  }else{
    FILE *fp = fopen(filename, "r");
    if (!fp) return 1;
    
    if (argv[1] && atoi(argv[1]) > 0) {
      int limit = atoi(argv[1]);
      char lines[limit][256]; // VLA on stack
      int total = 0;

      while (fgets(lines[total % limit], sizeof(lines[0]), fp)) {
          total++;
      }

      int count = (total < limit) ? total : limit;
      int start = (total < limit) ? 0 : (total % limit);

      for (int i = 0; i < count; i++) {
          printf("    %d  %s", total - count + i + 1, lines[(start + i) % limit]);
      }
    }
    fclose(fp);
  }
  
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