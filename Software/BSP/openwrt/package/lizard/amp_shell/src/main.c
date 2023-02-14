#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "shell.h"

static struct termios save, current;

void signalHandler(int signum) {
  printf("amp_shell intrrupted\r\n");
  cli_system_deinit();
  tcsetattr(0, TCSANOW, &save);
  exit(signum);
}

int main(int argc, char **argv) {
  tcgetattr(0, &save);
  current = save;
  current.c_lflag &= ~ICANON;
  current.c_lflag &= ~ECHO;
  current.c_cc[VMIN] = 1;
  current.c_cc[VTIME] = 0;

  signal(SIGQUIT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGSTOP, signalHandler);

  tcsetattr(0, TCSANOW, &current);

  cli_system_init(argc, argv);

  tcsetattr(0, TCSANOW, &save);

  cli_system_deinit();

  return 0;
}
