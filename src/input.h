#ifndef INPUT_H
#define INPUT_H

#include <stddef.h> // for size_t

void enableRawMode();
void disableRawMode();
int read_input_with_autocomplete(char *buffer, size_t size);
int tokenise(char *input, char **argv);


#endif