CC = gcc
CFLAGS = -Wall -Wextra -g

# The target executable depends on the object files
myshell: main.o builtins.o input.o
	$(CC) $(CFLAGS) -o myshell main.o builtins.o input.o

# Rules to compile source files into object files
main.o: main.c builtins.h input.h
	$(CC) $(CFLAGS) -c main.c

builtins.o: builtins.c builtins.h
	$(CC) $(CFLAGS) -c builtins.c

input.o: input.c input.h builtins.h
	$(CC) $(CFLAGS) -c input.c

clean:
	rm -f *.o myshell