#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

typedef int (*cmd_function_t)(int argc, char **argv);

struct cli_command_t {
  const char *name;    /* the name of system call */
  const char *desc;    /* description of system call */
  cmd_function_t func; /* the function address of command entry */
};

void cmd_auto_complete(char *prefix);
bool check_exit_state(void);
int cmd_exec(char *cmd, int length);

#endif /*COMMAND_H */
