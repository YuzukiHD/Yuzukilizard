
/*
 ******************************************************************************
 *
 * isp_v4l2_helper.h
 *
 * Hawkview ISP - isp_v4l2_helper.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/17	VIDEO INPUT
 *
 *****************************************************************************
 */

#ifndef _ISP_V4L2_HELPER_H_
#define _ISP_V4L2_HELPER_H_

#include <linux/v4l2-subdev.h>

struct media_entity;

/*
 * v4l2_subdev_open - Open a sub-device
 * @entity: Sub-device media entity
 *
 * Open the V4L2 subdev device node associated with @entity. The file descriptor
 * is stored in the media_entity structure.
 *
 * Return 0 on success, or a negative error code on failure.
 */
int v4l2_subdev_open(struct media_entity *entity);

/*
 * v4l2_subdev_close - Close a sub-device
 * @entity: Sub-device media entity
 *
 * Close the V4L2 subdev device node associated with the @entity and opened by
 * a previous call to v4l2_subdev_open() (either explicit or implicit).
 */
void v4l2_subdev_close(struct media_entity *entity);

/*
 * v4l2_get_control - Read the value of a control
 * @entity: device media entity
 * @id: Control ID
 * @value: Control value to be filled
 *
 * Retrieve the current value of control @id and store it in @value.
 *
 * Return 0 on success or a negative error code on failure.
 */
int v4l2_get_control(struct media_entity *entity, unsigned int id,
			    int32_t *value);

/*
 * v4l2_set_control - Write the value of a control
 * @entity: device media entity
 * @id: Control ID
 * @value: Control value
 *
 * Set control @id to @value. The device is allowed to modify the requested
 * value, in which case @value is updated to the modified value.
 *
 * Return 0 on success or a negative error code on failure.
 */
int v4l2_set_control(struct media_entity *entity, unsigned int id,
			    int32_t *value);

/*
 * v4l2_get_controls - Read the value of multiple controls
 * @entity: device media entity
 * @count: Number of controls
 * @ctrls: Controls to be read
 *
 * Retrieve the current value of controls identified by @ctrls.
 *
 * Return 0 on success or a negative error code on failure.
 */
int v4l2_get_controls(struct media_entity *entity, unsigned int count,
			     struct v4l2_ext_control *ctrls);

/*
 * v4l2_set_controls - Write the value of multiple controls
 * @entity: device media entity
 * @count: Number of controls
 * @ctrls: Controls to be written
 *
 * Set controls identified by @ctrls. The device is allowed to modify the
 * requested values, in which case @ctrls is updated to the modified value.
 *
 * Return 0 on success or a negative error code on failure.
 */
int v4l2_set_controls(struct media_entity *entity, unsigned int count,
			     struct v4l2_ext_control *ctrls);

#endif /*_ISP_V4L2_HELPER_H_*/


