#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amp_device.h"

#define RAW_DATA_LINE_SIZE (512 - 16)

static amp_device *rdevice;
static amp_device *wdevice;

extern raw_device_ops rpmsg_ops;

int amp_device_read(char *data, int len) {
  if (!rdevice)
    return -1;
  if (rdevice->ops.read)
    return rdevice->ops.read(rdevice, data, len);
  return -1;
}

int amp_device_write(char *data, int len) {
  if (!wdevice)
    return -1;
  if (wdevice->ops.write)
    return wdevice->ops.write(wdevice, data, len);
  return -1;
}

int amp_device_init(void) {
  int err = -1;

  rdevice = malloc(sizeof(amp_device));
  if (!rdevice) {
    printf("alloc read device failed!\n");
    return -ENOMEM;
  }
  memset(rdevice, 0, sizeof(amp_device));
  rdevice->ops = rpmsg_ops;
  strncpy(rdevice->name, "rpmsg", sizeof(rdevice->name));
  wdevice = rdevice;
  if (rdevice->ops.init) {
    err = rdevice->ops.init(rdevice);
    if (err) {
      printf("raw device init failed!\n");
      goto error;
    }
  }

  return 0;
error:
  free(rdevice);
  rdevice = NULL;
  wdevice = NULL;
  return err;
}

int amp_device_deinit(void) {
  /* The read task maybe use the device as the same time. */
  if (rdevice) {
    rdevice->ops.deinit(rdevice);
    free(rdevice);
    rdevice = NULL;
    wdevice = NULL;
  }

  return 0;
}
