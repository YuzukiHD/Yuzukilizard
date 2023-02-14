#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amp_device.h"
#include "command.h"
#include "shell.h"

#define CLI_CONSOLEBUF_SIZE 128

static struct cli_shell *shell;
static char read_line[CLI_CONSOLEBUF_SIZE];
static pthread_t ampr_pid;
static int ampr_stat;

static bool shell_handle_history(struct cli_shell *shell) {
  printf("\033[2K\r");
#ifdef CONFIG_PRINT_CLI_PROMPT
  printf("%s%s", CLI_PROMPT, shell->line);
#else
  printf("%s", shell->line);
#endif
  return false;
}

static void shell_push_history(struct cli_shell *shell) {
  if (shell->line_position != 0) {
    /* push history */
    if (shell->history_count >= CLI_HISTORY_LINES) {
      /* move history */
      int index;
      for (index = 0; index < CLI_HISTORY_LINES - 1; index++) {
        memcpy(&shell->cmd_history[index][0], &shell->cmd_history[index + 1][0],
               CLI_CMD_SIZE);
      }
      memset(&shell->cmd_history[index][0], 0, CLI_CMD_SIZE);
      memcpy(&shell->cmd_history[index][0], shell->line, shell->line_position);

      /* it's the maximum history */
      shell->history_count = CLI_HISTORY_LINES;
    } else {
      memset(&shell->cmd_history[shell->history_count][0], 0, CLI_CMD_SIZE);
      memcpy(&shell->cmd_history[shell->history_count][0], shell->line,
             shell->line_position);

      /* increase count and set current history position */
      shell->history_count++;
    }
  }
  shell->current_history = shell->history_count;
}

static void cli_work_entry(void) {
  char ch;

  setbuf(stdout, NULL);
  setbuf(stdin, NULL);
#ifdef CONFIG_PRINT_CLI_PROMPT
  printf(CLI_PROMPT);
#endif

  shell->echo_mode = 1;

  while (1) {
    while ((ch = getchar()) != 0) {
      /*
       * handle control key
       * up key  : 0x1b 0x5b 0x41
       * down key: 0x1b 0x5b 0x42
       * right key:0x1b 0x5b 0x43
       * left key: 0x1b 0x5b 0x44
       */
      if (ch == 0x1b) {
        shell->stat = WAIT_SPEC_KEY;
        continue;
      } else if (shell->stat == WAIT_SPEC_KEY) {
        if (ch == 0x5b) {
          shell->stat = WAIT_FUNC_KEY;
          continue;
        }

        shell->stat = WAIT_NORMAL;
      } else if (shell->stat == WAIT_FUNC_KEY) {
        shell->stat = WAIT_NORMAL;

        if (ch == 0x41) { /* up key */
          /* prev history */
          if (shell->current_history > 0)
            shell->current_history--;
          else {
            shell->current_history = 0;
            continue;
          }

          /* copy the history command */
          memcpy(shell->line, &shell->cmd_history[shell->current_history][0],
                 CLI_CMD_SIZE);
          shell->line_curpos = shell->line_position = strlen(shell->line);
          shell_handle_history(shell);
          continue;
        } else if (ch == 0x42) { /* down key */
          /* next history */
          if (shell->current_history < shell->history_count - 1)
            shell->current_history++;
          else {
            /* set to the end of history */
            if (shell->history_count != 0)
              shell->current_history = shell->history_count - 1;
            else
              continue;
          }

          memcpy(shell->line, &shell->cmd_history[shell->current_history][0],
                 CLI_CMD_SIZE);
          shell->line_curpos = shell->line_position = strlen(shell->line);
          shell_handle_history(shell);
          continue;
        } else if (ch == 0x44) { /* left key */
          if (shell->line_curpos) {
            printf("\b");
            shell->line_curpos--;
          }

          continue;
        } else if (ch == 0x43) { /* right key */
          if (shell->line_curpos < shell->line_position) {
            printf("%c", shell->line[shell->line_curpos]);
            shell->line_curpos++;
          }

          continue;
        }
      }

      /* handle CR key */
      if (ch == '\r') {
        char next = getchar();

        if (next) {
          if (next == '\0')
            ch = '\r'; /* linux telnet will issue '\0' */
          else
            ch = next;
        } else
          ch = '\r';
      }
      /* handle tab key */
      else if (ch == '\t') {
        continue;
      }
      /* handle backspace key */
      else if (ch == 0x7f || ch == 0x08) {
        /* note that shell->line_curpos >= 0 */
        if (shell->line_curpos == 0)
          continue;

        shell->line_position--;
        shell->line_curpos--;

        if (shell->line_position > shell->line_curpos) {
          int i;

          memmove(&shell->line[shell->line_curpos],
                  &shell->line[shell->line_curpos + 1],
                  shell->line_position - shell->line_curpos);
          shell->line[shell->line_position] = 0;

          printf("\b%s  \b", &shell->line[shell->line_curpos]);

          /* move the cursor to the origin position */
          for (i = shell->line_curpos; i <= shell->line_position; i++)
            printf("\b");
        } else {
          printf("\b \b");
          shell->line[shell->line_position] = 0;
        }

        continue;
      }

      /* handle end of line, break */
      if (ch == '\r' || ch == '\n') {
        shell_push_history(shell);

        if (shell->echo_mode)
          printf("\n");
        cmd_exec(shell->line, shell->line_position);
        if (check_exit_state() == true) {
          return;
        }
#ifdef CONFIG_PRINT_CLI_PROMPT
        printf(CLI_PROMPT);
#endif
        memset(shell->line, 0, sizeof(shell->line));
        shell->line_curpos = shell->line_position = 0;
        break;
      }

      /* it's a large line, discard it */
      if (shell->line_position >= CLI_CMD_SIZE)
        shell->line_position = 0;

      /* normal character */
      if (shell->line_curpos < shell->line_position) {
        int i;

        memmove(&shell->line[shell->line_curpos + 1],
                &shell->line[shell->line_curpos],
                shell->line_position - shell->line_curpos);
        shell->line[shell->line_curpos] = ch;
        if (shell->echo_mode)
          printf("%s", &shell->line[shell->line_curpos]);

        /* move the cursor to new position */
        for (i = shell->line_curpos; i < shell->line_position; i++)
          printf("\b");
      } else {
        shell->line[shell->line_position] = ch;
        if (shell->echo_mode)
          printf("%c", ch);
      }

      ch = 0;
      shell->line_position++;
      shell->line_curpos++;
      if (shell->line_position >= CLI_CMD_SIZE) {
        /* clear command line */
        shell->line_position = 0;
        shell->line_curpos = 0;
      }
    }
  }
}

static void *shell_read_task_entry(void *param) {
  int i = 0;
  int ret = -1;

  memset(read_line, 0, sizeof(read_line));

  while (1) {
    if (ampr_stat)
      break;

    ret = amp_device_read(read_line, sizeof(read_line));
    if (ret <= 0)
      continue;

    read_line[ret] = '\0';
    printf("%s", read_line);
  }
out:
  pthread_exit(NULL);
  return NULL;
}

static void print_usage(void) { printf("Usage: amp_shell [cmd] \n"); }

static void say_hello(void) {
  printf("=============================\n");
  printf("          AMP Shell         \n");
  printf("============================\n");
}

int cli_system_init(int argc, char **argv) {
  int ret;
  char *cmd = NULL;

  if (argc == 2) {
    cmd = argv[1];
    printf("cmd=%s\r\n", cmd);
  }

  shell = (struct cli_shell *)malloc(sizeof(struct cli_shell));
  if (shell == NULL) {
    printf("no memory for shell\n");
    return -1;
  }
  memset(shell, 0, sizeof(struct cli_shell));

  if (amp_device_init()) {
    printf("amp device init failed!\n");
    ret = -ENOMEM;
    goto free_shell;
  }

  ampr_stat = 0;
  ret = pthread_create(&ampr_pid, NULL, shell_read_task_entry, NULL);
  if (ret) {
    printf("create shell read task failed!\n");
    ret = -ENOMEM;
    goto amp_deinit;
  }

  if (!cmd) {
    say_hello();
    cmd_amp_open_send(NULL, 0);
    cli_work_entry();
  } else {
    cmd_amp_open_send(NULL, 0);
    cmd_amp_exec_send(cmd, strlen(cmd));
    sleep(1);
  }

  ret = 0;
amp_deinit:
  amp_device_deinit();
free_shell:
  free(shell);
  shell = NULL;

  return ret;
}

int cli_system_deinit(void) {
  ampr_stat = 1;
  amp_device_deinit();
  if (shell) {
    free(shell);
    shell = NULL;
  }
}
