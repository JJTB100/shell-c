#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "builtins.h"

// --- GLOBAL STATE ---
int next_line_to_save = 0; 

void load_session_start(const char *filename) {
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
    fprintf(stderr, "[DEBUG] load_session_start: %s has %d lines. next_line_to_save = %d\n", filename, count, next_line_to_save);
}

// --- IMPLEMENTATIONS ---

int do_history(char **argv) {
    char *filename = getenv("HISTFILE");
    if (!filename) {
        fprintf(stderr, "[DEBUG] do_history: HISTFILE env var is not set!\n");
        return 1;
    }

    if (!argv[1]) {
        // "history" command
        FILE *fp = fopen(filename, "r");
        if (!fp) return 0;
        char line[1024]; 
        int count = 0;
        while (fgets(line, sizeof(line), fp)) {
            printf("    %d  %s", ++count, line);
        }
        fclose(fp);
        return 0;
    } 
    else if (strcmp(argv[1], "-a") == 0) {
        char *target_file = argv[2] ? argv[2] : filename;
        
        fprintf(stderr, "[DEBUG] history -a: Source='%s', Target='%s', next_line_to_save=%d\n", 
                filename, target_file, next_line_to_save);

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
        
        fprintf(stderr, "[DEBUG] history -a: Scanned %d total lines. Wrote %d new lines.\n", current_line_idx, written_count);

        next_line_to_save = current_line_idx;
        fclose(fp_session);
        fclose(fp_dest);
        return 0;
    }
    
    return 0;
}

// ... Paste your standard do_echo, do_cd, etc. below here ...
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
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("pwd error");
    return 1;
  }
  return 0;
}

int do_type(char **argv) {
  if (argv[1] == NULL) return 1;
  char *target = argv[1];

  for (int i = 0; builtins[i].name != NULL; i++) {
    if (strcmp(target, builtins[i].name) == 0) {
      printf("%s is a shell builtin\n", target);
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