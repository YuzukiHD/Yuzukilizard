#ifndef _AMP_DEVICE_H
#define _AMP_DEVICE_H

#include "shell.h"

#define AMP_DEVICE_NAME_MAX (32)

typedef struct _amp_device amp_device;

typedef struct _raw_device_ops {
  int (*write)(amp_device *device, char *data, int len);
  int (*read)(amp_device *device, char *data, int len);
  int (*init)(amp_device *device);
  int (*deinit)(amp_device *device);
  int (*connect)(amp_device *device);
} raw_device_ops;

typedef struct _amp_device {
  char name[AMP_DEVICE_NAME_MAX];
  int amp_fd;
  raw_device_ops ops;
} amp_device;

int amp_device_init(void);
int amp_device_deinit(void);
int amp_device_read(char *data, int len);
int amp_device_write(char *data, int len);

#endif
