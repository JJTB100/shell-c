#ifndef BUILTINS_H
#define BUILTINS_H

// Type Definitions
typedef int (*builtin_handler)(char **argv);

typedef struct {
  const char *name;
  builtin_handler handler;
} Builtin;

// Expose the registry so other files (like autocomplete) can see it
extern Builtin builtins[];

// Prototypes
int do_echo(char **argv);
int do_exit(char **argv);
int do_type(char **argv);
int do_pwd(char **argv);
int do_cd(char **argv);
int do_history(char **argv);

// Helper to execute builtins with redirection
int handle_builtin(char *command, char **argv, char *out_file, char *err_file, int append_out, int append_err);

#endif