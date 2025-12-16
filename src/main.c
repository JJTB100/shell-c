#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while (1){
    // TODO: Uncomment the code below to pass the first stage
    printf("$ ");

    char command[1024];
    fgets(command, sizeof(command), stdin);

    // Remove trailing new line
    command[strcspn(command, "\n")] = '\0';

    printf("%s: command not found\n", command);
  }

  return 0;
}
