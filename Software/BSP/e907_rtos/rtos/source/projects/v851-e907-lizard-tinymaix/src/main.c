#include <openamp/sunxi_helper/openamp.h>
#include <stdio.h>

int app_entry(void *param) { return 0; }

int hello_cmd(int argc, const char **argv) {
  printf("Hello World\n");
  return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(hello_cmd, hello, Show Hello World)