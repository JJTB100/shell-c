#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include "input.h"
#include "builtins.h"

struct termios orig_termios;

int tokenise(char *input, char **argv){
  int argc = 0;
  int in_single_quotes = 0;
  int in_double_quotes = 0;
  char * current_token = input;

  // Loop through every character
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
    } else if(input[i]=='|'&& !in_single_quotes && !in_double_quotes){
      input[i] = '\0';
      if (*current_token != '\0') {
        argv[argc++] = current_token;
      }
      argv[argc++] = "|";
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
char* longest_common_prefix(char** names, int names_size) {
  if (names_size == 0) return strdup("");
  if (names_size == 1) return strdup(names[0]);

  // Because the list is sorted, we only need to compare 
  // the first and the last string to find the common prefix 
  // for the entire set.
  char* first = names[0];
  char* last = names[names_size - 1];
  int i = 0;

  while (first[i] && last[i] && first[i] == last[i]) {
    i++;
  }

  char* prefix = malloc(i + 1);
  strncpy(prefix, first, i);
  prefix[i] = '\0';

  return prefix;
}
int read_input_with_autocomplete(char *buffer, size_t size) {
  int pos = 0;
  char c;
  int tab_count = 0;

  int echo_enabled = (orig_termios.c_lflag & ECHO);
  
  // IMPORTANT: Initialize buffer to empty string to ensure clean start

  buffer[0] = '\0'; 

  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\n'|| c == '\r') {
      buffer[pos] = '\0';
      if (echo_enabled) printf("\n");
      return 0;
    } else if (c == '\033') { // Escape sequence detected
      char seq[3];
      // Read the next two bytes into a temporary buffer
      // This strips them from the input stream so they don't get printed
      if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
      if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;

      if (seq[0] == '[') {
        switch (seq[1]) {
          case 'A': // Up Arrow
            FILE *fp = fopen("term_history.txt", "r");
            char last_line[256] = {0};

            if (fp != NULL) {
                char temp[256];
                // Read through the file; the last successful read is the last line
                while (fgets(temp, sizeof(temp), fp) != NULL) {
                    // Ignore empty lines (just newlines) if necessary
                    if (strlen(temp) > 1) {
                        strcpy(last_line, temp);
                    }
                }
                fclose(fp);
            }

            // Only update if we found a history line
            if (strlen(last_line) > 0) {
                // 1. Strip the trailing newline from the file
                size_t len = strlen(last_line);
                if (len > 0 && last_line[len - 1] == '\n') {
                    last_line[len - 1] = '\0';
                }

                // 2. Replace the current buffer
                strncpy(buffer, last_line, size - 1);
                buffer[size - 1] = '\0'; // Safety null-term
                pos = strlen(buffer);

                // 3. Overwrite the visual output
                // \r moves to start, \033[K clears the rest of the line
                printf("\r$ %s\033[K", buffer);
            }
            break;
          case 'B': // Down Arrow
            // TODO: Implement History (Next command)
            break;
          case 'C': // Right Arrow
            // TODO: Move cursor right (requires tracking pos < length)
            break;
          case 'D': // Left Arrow
            // TODO: Move cursor left (requires tracking pos > 0)
            break;
        }
      }
    }
    else if (c == '\t') {
      tab_count++;
      int match_count;
      char **matches = get_all_matches(buffer, &match_count);

      if (match_count == 0){
        printf("\a"); // Bell
      } 
      else if (match_count == 1){
        // SINGLE MATCH
        // This block was mostly correct, but let's make it robust
        const char *to_add = &matches[0][pos];
          
        if (strlen(to_add) > 0) {
            strcat(buffer, to_add);
            printf("%s", to_add);
            pos += strlen(to_add);
        }
        
        // Only add space if we actually completed a word
        // (Optional preference, but standard for shells)
        strcat(buffer, " ");
        printf(" ");
        pos++;
      } 
      else { // MULTIPLE MATCHES
        char *prefix = longest_common_prefix(matches, match_count);
        
        // Attempt to extend the current word
        if (strlen(prefix) > (size_t)pos) {
          char *suffix = prefix + pos; 
          strcat(buffer, suffix);
          printf("%s", suffix);
          pos += strlen(suffix);
        } else {
            // If we couldn't extend (e.g. "e" -> "e"), ring the bell
            // only on the first tab press.
            if (tab_count == 1) printf("\a");
        }
        
        //  If this is the second tab press, show the list
        if (tab_count == 2){
            printf("\n");
            for(int match_no=0; match_no<match_count; match_no++){
              printf("%s  ", matches[match_no]);
            }
            printf("\n$ %s", buffer);
            tab_count = 0; // Reset so next tab starts fresh
        }
        
        free(prefix);
      }
      
    } 
    else if (c == 127) { // Backspace
      tab_count = 0; // Reset tab count on edit
      if (pos > 0) {
        pos--;
        buffer[pos] = '\0'; // Ensure buffer is null-terminated on backspace
        if (echo_enabled) printf("\b \b");
      }
    } 
    else {
      tab_count = 0; // Reset tab count on normal typing
      if (size > 0 && (size_t)pos < size - 1) {
        buffer[pos++] = c;
        buffer[pos] = '\0';
        if (echo_enabled) printf("%c", c);
      }
    }
  }
  return -1;
}