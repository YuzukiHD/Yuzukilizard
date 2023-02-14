#ifndef __SHELL_H__
#define __SHELL_H__

#include <stdint.h>

#define CLI_HISTORY_LINES 10
#define CLI_CMD_SIZE 256

#define CLI_PROMPT "msh >"

enum input_stat {
  WAIT_NORMAL,
  WAIT_SPEC_KEY,
  WAIT_FUNC_KEY,
};

struct cli_shell {
  enum input_stat stat;

  uint8_t echo_mode : 1;

  uint16_t current_history;
  uint16_t history_count;

  char cmd_history[CLI_HISTORY_LINES][CLI_CMD_SIZE];

  char line[CLI_CMD_SIZE];
  uint8_t line_position;
  uint8_t line_curpos;
};

int cli_system_init(int argc, char **argv);
int cli_system_deinit(void);
int cmd_amp_open_send(char *cmd, int length);
int cmd_amp_exec_send(char *cmd, int length);
int cmd_amp_close_send(char *cmd, int length);

#endif
