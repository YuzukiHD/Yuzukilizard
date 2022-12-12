
/*
 ******************************************************************************
 *
 * subdev.c
 *
 * Hawkview ISP - subdev.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/17	VIDEO INPUT
 *
 *****************************************************************************
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/v4l2-subdev.h>

#include "media.h"
#include "tools.h"

int v4l2_subdev_open(struct media_entity *entity)
{
	if (entity->fd != -1)
		return 0;

	entity->fd = open(entity->devname, O_RDWR | O_NONBLOCK);
	if (entity->fd == -1) {
		ISP_ERR("%s: Failed to open subdev device node %s\n", __func__,
			entity->devname);
		return -errno;
	}
	return 0;
}

void v4l2_subdev_close(struct media_entity *entity)
{
	if (entity->fd == -1)
		return;

	close(entity->fd);
	entity->fd = -1;
}

/* -----------------------------------------------------------------------------
 * Controls
 */

int v4l2_get_control(struct media_entity *entity, unsigned int id,
			    int32_t *value)
{
	struct v4l2_control ctrl;
	int ret;

	if (entity->fd == -1)
		return -1;

	ctrl.id = id;

	ret = ioctl(entity->fd, VIDIOC_G_CTRL, &ctrl);
	if (ret < 0) {
		ISP_ERR("unable to get control: %s (%d).\n",
			strerror(errno), errno);
		return -errno;
	}

	*value = ctrl.value;
	return 0;
}

int v4l2_set_control(struct media_entity *entity, unsigned int id,
			    int32_t *value)
{
	struct v4l2_control ctrl;
	int ret;

	if (entity->fd == -1)
		return -1;

	ctrl.id = id;
	ctrl.value = *value;

	ret = ioctl(entity->fd, VIDIOC_S_CTRL, &ctrl);
	if (ret < 0) {
		ISP_ERR("unable to set control: %s (%d).\n",
			strerror(errno), errno);
		return -errno;
	}

	*value = ctrl.value;
	return 0;
}

int v4l2_get_controls(struct media_entity *entity, unsigned int count,
			     struct v4l2_ext_control *ctrls)
{
	struct v4l2_ext_controls controls;
	int ret;

	if (entity->fd == -1)
		return -1;

	memset(&controls, 0, sizeof controls);
	controls.count = count;
	controls.controls = ctrls;

	ret = ioctl(entity->fd, VIDIOC_G_EXT_CTRLS, &controls);
	if (ret < 0)
		ISP_ERR("unable to get multiple controls: %s (%d).\n",
			strerror(errno), errno);

	return ret;
}

int v4l2_set_controls(struct media_entity *entity, unsigned int count,
			     struct v4l2_ext_control *ctrls)
{
	struct v4l2_ext_controls controls;
	int ret;

	if (entity->fd == -1)
		return -1;

	memset(&controls, 0, sizeof controls);
	controls.count = count;
	controls.controls = ctrls;

	ret = ioctl(entity->fd, VIDIOC_S_EXT_CTRLS, &controls);
	if (ret < 0)
		ISP_ERR("unable to set multiple controls: %s (%d).\n",
			strerror(errno), errno);

	return ret;
}

