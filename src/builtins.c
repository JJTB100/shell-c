#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "builtins.h"

const char *get_history_filename() {
    const char *env = getenv("HISTFILE");
    if(env) {
        printf("[DEBUG] get_history_filename: HISTFILE='%s'\n", env);
    }
    return env ? env : "hist_file.txt";

}
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
void load_session_start() {
  const char *filename = get_history_filename();
  next_line_to_save = 0;

  if (!filename) {
      fprintf(stderr, "[DEBUG] load_session_start: HISTFILE is NULL\n");
      return;
  }
  
  FILE *f = fopen(filename, "r");
  if (!f) {
      fprintf(stderr, "[DEBUG] load_session_start: Could not open %s (assuming new session)\n", filename);
      return;
  }

  int count = 0;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), f)) {
      count++;
  }
  fclose(f);
  
  next_line_to_save = count;
  //fprintf(stderr, "[DEBUG] load_session_start: %s has %d lines. next_line_to_save = %d\n", filename, count, next_line_to_save);
}
// --- IMPLEMENTATIONS ---
int do_history(char **argv) {
  char * filename = (char*) get_history_filename();
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
    char *target_file = argv[2] ? argv[2] : filename;
    //fprintf(stderr, "[DEBUG] history -a: Source='%s', Target='%s', next_line_to_save=%d\n", 
            //    filename, target_file, next_line_to_save);
    FILE *fp_session = fopen(filename, "r");
    if (!fp_session) {
        fprintf(stderr, "[DEBUG] history -a: Could not read source file %s\n", filename);
        return 0; 
    }

    FILE *fp_dest = fopen(target_file, "a");
    if (!fp_dest) {
        printf("Error: Could not open %s for appending.\n", target_file);
        fclose(fp_session);
        return 1;
    }
    
    char buffer[1024];
    int current_line_idx = 0;
    int written_count = 0;

    while (fgets(buffer, sizeof(buffer), fp_session)) {
        // Check if this line is "new"
        if (current_line_idx >= next_line_to_save) {
            fputs(buffer, fp_dest);
            written_count++;
        }
        current_line_idx++;
    }
    
    //fprintf(stderr, "[DEBUG] history -a: Scanned %d total lines. Wrote %d new lines.\n", current_line_idx, written_count);

    next_line_to_save = current_line_idx;
    fclose(fp_session);
    fclose(fp_dest);
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