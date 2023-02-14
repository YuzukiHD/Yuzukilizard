#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amp_device.h"
#include "command.h"
#include "shell.h"

#include <pthread.h>

struct rpmsg_message {
  unsigned int command;
  unsigned int data_length;
};

struct rpmsg_console_packet {
  struct rpmsg_message msg;
  char data[512 - 16 - sizeof(struct rpmsg_message)];
};

enum {
  A_OPEN = 0,
  A_CLSE,
  A_WRTE,
};

#define CLI_ARG_MAX 10
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static struct rpmsg_console_packet packet;
static pthread_mutex_t packet_mutex = PTHREAD_MUTEX_INITIALIZER;

static int cmd_exit(int argc, char **argv);
static int cmd_help(int argc, char **argv);

static struct cli_command_t default_commands[] = {
    {"amp_exit", "exit amp shell", cmd_exit},
    {"amp_help", "show help information", cmd_help},
};

static bool _exit_state = false;

int cmd_amp_exec_send(char *cmd, int length) {
  pthread_mutex_lock(&packet_mutex);
  memset(&packet, 0, sizeof(struct rpmsg_console_packet));
  packet.msg.command = A_WRTE;
  packet.msg.data_length = length + 1;
  memcpy(packet.data, cmd, length);
  packet.data[length] = '\n';
  amp_device_write((char *)&packet, sizeof(struct rpmsg_message) + length + 1);
  pthread_mutex_unlock(&packet_mutex);
  return 0;
}

int cmd_amp_open_send(char *cmd, int length) {
  int ret = 0;
  pthread_mutex_lock(&packet_mutex);
  memset(&packet, 0, sizeof(struct rpmsg_console_packet));
  packet.msg.command = A_OPEN;

  if (cmd) {
    ret = snprintf(packet.data, sizeof(packet.data), "%s", cmd);
    packet.data[ret] = 0;
    packet.msg.data_length = ret + 1;
  } else
    packet.msg.data_length = 0;

  amp_device_write((char *)&packet,
                   sizeof(struct rpmsg_message) + packet.msg.data_length);
  pthread_mutex_unlock(&packet_mutex);
  return 0;
}

int cmd_amp_close_send(char *cmd, int length) {
  int ret = 0;
  pthread_mutex_lock(&packet_mutex);
  memset(&packet, 0, sizeof(struct rpmsg_console_packet));
  packet.msg.command = A_CLSE;

  if (cmd) {
    ret = snprintf(packet.data, sizeof(packet.data), "%s", cmd);
    packet.data[ret] = 0;
    packet.msg.data_length = ret + 1;
  } else
    packet.msg.data_length = 0;

  amp_device_write((char *)&packet,
                   sizeof(struct rpmsg_message) + packet.msg.data_length);
  pthread_mutex_unlock(&packet_mutex);
  return 0;
}

static int cmd_exit(int argc, char **argv) {
  char *exit_cmd = "exit";

  cmd_amp_exec_send(exit_cmd, strlen(exit_cmd));
  _exit_state = true;

  return 0;
}

static int cmd_help(int argc, char **argv) {
  printf("Amp shell commands:\n");
  int i;

  for (i = 0; i < ARRAY_SIZE(default_commands); i++) {
    printf("%-16s - %s\n", default_commands[i].name, default_commands[i].desc);
  }

  return 0;
}

static int cmd_split(char *cmd, int length, char *argv[CLI_ARG_MAX]) {
  char *ptr;
  int position;
  int argc;

  ptr = cmd;
  position = 0;
  argc = 0;

  while (position < length) {
    /* strip bank and tab */
    while ((*ptr == ' ' || *ptr == '\t') && position < length) {
      *ptr = '\0';
      ptr++;
      position++;
    }
    if (position >= length)
      break;

    /* handle string */
    if (*ptr == '"') {
      ptr++;
      position++;
      argv[argc] = ptr;
      argc++;

      /* skip this string */
      while (*ptr != '"' && position < length) {
        if (*ptr == '\\') {
          if (*(ptr + 1) == '"') {
            ptr++;
            position++;
          }
        }
        ptr++;
        position++;
      }
      if (position >= length)
        break;

      /* skip '"' */
      *ptr = '\0';
      ptr++;
      position++;
    } else {
      argv[argc] = ptr;
      argc++;
      while ((*ptr != ' ' && *ptr != '\t') && position < length) {
        ptr++;
        position++;
      }
      if (position >= length)
        break;
    }
  }

  return argc;
}

static cmd_function_t cmd_get_cmd(char *cmd, int size) {
  int i;
  cmd_function_t cmd_func = NULL;

  for (i = 0; i < ARRAY_SIZE(default_commands); i++) {
    if (strncmp(default_commands[i].name, cmd, size) == 0) {
      cmd_func = (cmd_function_t)default_commands[i].func;
      break;
    }
  }

  return cmd_func;
}

static int _cmd_exec_cmd(char *cmd, int length, int *retp) {
  int argc;
  int cmd0_size = 0;
  cmd_function_t cmd_func;
  char *argv[CLI_ARG_MAX];

  /* find the size of first command */
  while ((cmd[cmd0_size] != ' ' && cmd[cmd0_size] != '\t') &&
         cmd0_size < length)
    cmd0_size++;
  if (cmd0_size == 0)
    return -1;

  cmd_func = cmd_get_cmd(cmd, cmd0_size);
  if (cmd_func == NULL)
    return -1;

  /* split arguments */
  memset(argv, 0x00, sizeof(argv));
  argc = cmd_split(cmd, length, argv);
  if (argc == 0)
    return -1;

  /* exec this command */
  *retp = cmd_func(argc, argv);
  return 0;
}

int cmd_exec(char *cmd, int length) {
  int cmd_ret;

  /* strim the beginning of command */
  while (*cmd == ' ' || *cmd == '\t') {
    cmd++;
    length--;
  }

  if (length == 0)
    return 0;

  if (!strncmp(cmd, "amp_", strlen("amp_"))) {
    if (_cmd_exec_cmd(cmd, length, &cmd_ret) == 0) {
      return cmd_ret;
    }
  }

  return cmd_amp_exec_send(cmd, length);
}

bool check_exit_state(void) { return _exit_state; }
