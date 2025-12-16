#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  
  printf("$ ");

  ssize_t max_size = 1024;
  char inp[max_size];

  while (fgets(inp, max_size, stdin) != NULL){
    // Remove trailing new line
    inp[strcspn(inp, "\n")] = '\0';
    char *space_pos = strchr(inp, ' ');
    char *command = NULL;
    char *args = NULL;

    if (space_pos != NULL){
      *space_pos = '\0';
      command = inp;
      args = space_pos +1;
      while(*args == ' ') args++; //Skip leading whitespace
    } else{
      command = inp;
      args = NULL;
    }

    if (strcmp(command, "exit") == 0){
      return 0;
    } else if (strcmp(command, "echo") == 0){
      printf("%s\n", args);
    } else{
      printf("%s: command not found\n", inp);
    }
    printf("$ ");
  }

  return 0;
}
