#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  
  printf("$ ");

  ssize_t max_size = 1024;
  char command[max_size];

  while (fgets(command, max_size, stdin) != NULL){
    // Remove trailing new line
    command[strcspn(command, "\n")] = '\0';

    if (strcmp(command, "exit") == 0){
      return 0;
    }
    printf("%s: command not found\n", command);
    printf("$ ");
  }

  return 0;
}
