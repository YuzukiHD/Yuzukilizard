#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/rpmsg.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../amp_device.h"

#define RPMSG_CTRL_DEV "/dev/rpmsg_ctrl0"
#define RPMSG_BIND_NAME "console"

static int fd_ctrl;
static int fd_ept;
static uint32_t ept_id;

int rpmsg_device_write(amp_device *device, char *data, int len) {
  int fd = device->amp_fd;
  return write(fd, data, len);
}

int rpmsg_device_read(amp_device *device, char *data, int len) {
  int ret = -1;
  int fd = device->amp_fd;

  struct pollfd poll_fds = {0};
  poll_fds.fd = fd;
  poll_fds.events = POLLIN;

  ret = poll(&poll_fds, 1, -1);
  if (ret < 0) {
    if (errno == EINTR) {
      printf("Signal occurred. Exit\n");
    } else {
      printf("Poll error (%s)\n", strerror(errno));
    }
  }
  return read(fd, data, len);
}

int rpmsg_device_connect(amp_device *device) { return 0; }

int rpmsg_device_init(amp_device *device) {
  int ret = -1;
  char *ctrl_dev = RPMSG_CTRL_DEV;
  struct rpmsg_ept_info info;
  char ept_dev_name[32];

  strcpy(info.name, RPMSG_BIND_NAME);
  info.id = 0xfffff;

  fd_ctrl = open(ctrl_dev, O_RDWR);
  if (fd_ctrl < 0) {
    printf("Failed to open \"%s\" (ret: %d)\n", ctrl_dev, fd_ctrl);
    ret = -1;
    goto out;
  }

  printf("Creating Endpoint...\r\n");
  ret = ioctl(fd_ctrl, RPMSG_CREATE_EPT_IOCTL, &info);
  if (ret < 0) {
    printf("Failed to create endpoint (ret: %d)\n", ret);
    ret = -1;
    goto close_fd_ctrl;
  }
  close(fd_ctrl);

  ept_id = info.id;
  printf("Success Create /dev/rpmsg%d\n", info.id);
  snprintf(ept_dev_name, 32, "/dev/rpmsg%d", info.id);

  fd_ept = open(ept_dev_name, O_RDWR);
  if (fd_ept < 0) {
    printf("Failed to open \"%s\" (ret: %d)\n", ept_dev_name, fd_ept);
    ret = -1;
    goto close_fd_ctrl;
  }

  device->amp_fd = fd_ept;

  return 0;

close_fd_ept:
  close(fd_ept);
close_fd_ctrl:
  close(fd_ctrl);
out:
  return ret;
}

int rpmsg_device_deinit(amp_device *device) {
  struct rpmsg_ept_info info;

  if (device->amp_fd) {
    close(device->amp_fd);
    device->amp_fd = 0;
  }

  info.id = ept_id;
  printf("destory rpmsg%d device\r\n", ept_id);
  fd_ctrl = open(RPMSG_CTRL_DEV, O_RDWR);
  if (fd_ctrl > 0) {
    ioctl(fd_ctrl, RPMSG_DESTROY_EPT_IOCTL, &info);
    close(fd_ctrl);
    fd_ctrl = 0;
  }
  return 0;
}

raw_device_ops rpmsg_ops = {
    .write = rpmsg_device_write,
    .read = rpmsg_device_read,
    .init = rpmsg_device_init,
    .deinit = rpmsg_device_deinit,
    .connect = rpmsg_device_connect,
};
