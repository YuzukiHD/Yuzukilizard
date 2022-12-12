
/*
 ******************************************************************************
 *
 * media.c
 *
 * Hawkview ISP - media.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/29	VIDEO INPUT
 *
 *****************************************************************************
 */

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/sysmacros.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include "media.h"
#include "tools.h"

struct media_entity_pad *media_entity_remote_source(struct media_entity_pad *pad)
{
	unsigned int i;

	for (i = 0; i < pad->entity->num_links; ++i) {
		struct media_entity_link *link = &pad->entity->links[i];

		if (!(link->flags & MEDIA_LNK_FL_ENABLED))
			continue;

		if (link->source == pad)
			return link->sink;

		if (link->sink == pad)
			return link->source;
	}

	return NULL;
}

struct media_entity *media_get_entity_by_name
				(struct media_device *media, const char *name)
{
	unsigned int i;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		if (strcmp(entity->info.name, name) == 0)
			return entity;
	}

	return NULL;
}

struct media_entity *media_get_entity_by_id(struct media_device *media,
					    __u32 id)
{
	unsigned int i;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		if (entity->info.id == id)
			return entity;
	}

	return NULL;
}

struct media_entity *media_pipeline_get_head(struct media_entity *me)
{
	struct media_entity_pad *pad = &me->pads[0];

	while (!(pad->flags & MEDIA_PAD_FL_SOURCE)) {
		pad = media_entity_remote_source(pad);
		if (!pad)
			break;
		me = pad->entity;
		pad = &me->pads[0];
	}
	if (me->info.type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)
		return me;
	else
		return NULL;
}

int media_video_to_isp_id(struct media_entity *me)
{
	struct media_entity_pad *pad = &me->pads[0];
	int id = -1;

	while (!(pad->flags & MEDIA_PAD_FL_SOURCE)) {
		pad = media_entity_remote_source(pad);
		if (!pad)
			return id;
		me = pad->entity;
		if (!strncmp(me->info.name, "sunxi_isp.", 9)) {
			sscanf(me->info.name, "sunxi_isp.%d", &id);
			return id;
		}
		pad = &me->pads[0];
	}
	return id;
}

int media_video_to_csi_id(struct media_entity *me)
{
	struct media_entity_pad *pad = &me->pads[0];
	int id = -1;

	while (!(pad->flags & MEDIA_PAD_FL_SOURCE)) {
		pad = media_entity_remote_source(pad);
		if (!pad)
			return id;
		me = pad->entity;
		if (!strncmp(me->info.name, "sunxi_csi.", 9)) {
			sscanf(me->info.name, "sunxi_csi.%d", &id);
			return id;
		}
		pad = &me->pads[0];
	}
	return id;
}

int media_setup_link(struct media_device *media,
		     struct media_entity_pad *source,
		     struct media_entity_pad *sink,
		     __u32 flags)
{
	struct media_entity_link *link;
	struct media_link_desc ulink;
	unsigned int i;
	int ret;

	for (i = 0; i < source->entity->num_links; i++) {
		link = &source->entity->links[i];

		if (link->source->entity == source->entity &&
		    link->source->index == source->index &&
		    link->sink->entity == sink->entity &&
		    link->sink->index == sink->index)
			break;
	}

	if (i == source->entity->num_links) {
		ISP_ERR("%s: Link not found\n", __func__);
		return -EINVAL;
	}

	/* source pad */
	ulink.source.entity = source->entity->info.id;
	ulink.source.index = source->index;
	ulink.source.flags = MEDIA_PAD_FL_SOURCE;

	/* sink pad */
	ulink.sink.entity = sink->entity->info.id;
	ulink.sink.index = sink->index;
	ulink.sink.flags = MEDIA_PAD_FL_SINK;

	ulink.flags = flags | (link->flags & MEDIA_LNK_FL_IMMUTABLE);

	ret = ioctl(media->fd, MEDIA_IOC_SETUP_LINK, &ulink);
	if (ret < 0) {
		ISP_ERR("%s: Unable to setup link (%s)\n", __func__,
			strerror(errno));
		return ret;
	}

	link->flags = flags;
	return 0;
}

int media_reset_links(struct media_device *media)
{
	unsigned int i, j;
	int ret;

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		for (j = 0; j < entity->num_links; j++) {
			struct media_entity_link *link = &entity->links[j];

			if (link->flags & MEDIA_LNK_FL_IMMUTABLE ||
			    link->source->entity != entity)
				continue;

			ret = media_setup_link(media, link->source, link->sink,
					link->flags & ~MEDIA_LNK_FL_ENABLED);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static struct media_entity_link *media_entity_add_link
						(struct media_entity *entity)
{
	if (entity->num_links >= entity->max_links) {
		struct media_entity_link *links = entity->links;
		unsigned int max_links = entity->max_links * 2;
		unsigned int i;

		links = realloc(entity->links, max_links * sizeof *links);
		if (links == NULL)
			return NULL;

		for (i = 0; i < entity->num_links; ++i)
			links[i].reverse->reverse = &links[i];

		entity->max_links = max_links;
		entity->links = links;
	}

	return &entity->links[entity->num_links++];
}

static int media_enum_links(struct media_device *media, int verbose)
{
	__u32 id;
	int ret = 0;

	for (id = 1; id <= media->entities_count; id++) {
		struct media_entity *entity = &media->entities[id - 1];
		struct media_links_enum links;
		unsigned int i;

		links.entity = entity->info.id;
		links.pads = calloc(entity->info.pads,
						sizeof(struct media_pad_desc));
		links.links = calloc(entity->info.links,
						sizeof(struct media_link_desc));

		if (ioctl(media->fd, MEDIA_IOC_ENUM_LINKS, &links) < 0) {
			ISP_ERR("%s: Unable to enumerate pads and links (%s).\n",
				__func__, strerror(errno));
			free(links.pads);
			free(links.links);
			return -errno;
		}

		for (i = 0; i < entity->info.pads; ++i) {
			entity->pads[i].entity = entity;
			entity->pads[i].index = links.pads[i].index;
			entity->pads[i].flags = links.pads[i].flags;
		}

		for (i = 0; i < entity->info.links; ++i) {
			struct media_link_desc *link = &links.links[i];
			struct media_entity_link *fwdlink;
			struct media_entity_link *backlink;
			struct media_entity *source;
			struct media_entity *sink;

			source = media_get_entity_by_id(media,
							link->source.entity);
			sink = media_get_entity_by_id(media, link->sink.entity);



			if (source == NULL || sink == NULL) {
				ISP_WARN("WARNING entity %u link %u from %u/%u "
					"to %u/%u is invalid!\n", id, i,
					link->source.entity, link->source.index,
					link->sink.entity, link->sink.index);
				ret = -EINVAL;
			}
			if (verbose)
				ISP_PRINT("entity link from %s to %s!\n",
					source->info.name, sink->info.name);

			fwdlink = media_entity_add_link(source);
			fwdlink->source = &source->pads[link->source.index];
			fwdlink->sink = &sink->pads[link->sink.index];
			fwdlink->flags = links.links[i].flags;

			backlink = media_entity_add_link(sink);
			backlink->source = &source->pads[link->source.index];
			backlink->sink = &sink->pads[link->sink.index];
			backlink->flags = links.links[i].flags;

			fwdlink->reverse = backlink;
			backlink->reverse = fwdlink;
		}

		free(links.pads);
		free(links.links);
	}

	return ret;
}

static int media_enum_entities(struct media_device *media)
{
	struct media_entity *entity;
	struct stat devstat;
	unsigned int size;
	char devname[32];
	char sysname[32];
	char target[1024];
	char *p;
	__u32 id;
	int ret;

	for (id = 0; ; id = entity->info.id) {
		size = (media->entities_count + 1) * sizeof(*media->entities);
		media->entities = realloc(media->entities, size);

		entity = &media->entities[media->entities_count];
		memset(entity, 0, sizeof(*entity));
		entity->fd = -1;
		entity->info.id = id | MEDIA_ENT_ID_FLAG_NEXT;

		ret = ioctl(media->fd, MEDIA_IOC_ENUM_ENTITIES, &entity->info);
		if (ret < 0) {
			if (errno == EINVAL)
				break;
			return -errno;
		}

		/* Number of links (for outbound links) plus number of pads (for
		 * inbound links) is a good safe initial estimate of the total
		 * number of links.
		 */
		entity->max_links = entity->info.pads + entity->info.links;

		entity->pads = malloc(entity->info.pads
						* sizeof(*entity->pads));
		entity->links = malloc(entity->max_links
						* sizeof(*entity->links));
		if (entity->pads == NULL || entity->links == NULL)
			return -ENOMEM;

		media->entities_count++;

		/* Find the corresponding device name. */
		if (media_entity_type(entity) != MEDIA_ENT_T_DEVNODE &&
		    media_entity_type(entity) != MEDIA_ENT_T_V4L2_SUBDEV)
			continue;

		sprintf(sysname, "/sys/dev/char/%u:%u", entity->info.v4l.major,
			entity->info.v4l.minor);
		ret = readlink(sysname, target, sizeof(target));
		if (ret < 0)
			continue;

		target[ret] = '\0';
		p = strrchr(target, '/');
		if (p == NULL)
			continue;

		sprintf(devname, "/dev/%s", p + 1);
		ret = stat(devname, &devstat);
		if (ret < 0)
			continue;

		/* Sanity check: udev might have reordered the device nodes.
		 * Make sure the major/minor match. We should really use
		 * libudev.
		 */
		if (major(devstat.st_rdev) == entity->info.v4l.major &&
		    minor(devstat.st_rdev) == entity->info.v4l.minor)
			strcpy(entity->devname, devname);
	}

	return 0;
}

struct media_device *media_open(const char *name, int verbose)
{
	struct media_device *media;
	int ret;
	unsigned int i;

	media = malloc(sizeof(*media));
	if (media == NULL) {
		ISP_ERR("%s: unable to allocate memory\n", __func__);
		return NULL;
	}
	memset(media, 0, sizeof(*media));

	if (verbose)
		ISP_PRINT("Opening media device %s\n", name);
	media->fd = open(name, O_RDWR);
	if (media->fd < 0) {
		media_close(media);
		ISP_ERR("%s: Can't open media device %s\n", __func__, name);
		return NULL;
	}

	ret = ioctl(media->fd, MEDIA_IOC_DEVICE_INFO, &media->info);
	if (ret < 0) {
		ISP_ERR("%s: Unable to retrieve media device information for "
		       "device %s (%s)\n", __func__, name, strerror(errno));
		media_close(media);
		return NULL;
	}

	if (verbose)
		ISP_PRINT("Enumerating entities\n");

	ret = media_enum_entities(media);
	if (ret < 0) {
		ISP_ERR("%s: Unable to enumerate entities for device %s (%s)\n",
			__func__, name, strerror(-ret));
		media_close(media);
		return NULL;
	}

	if (verbose) {
		ISP_PRINT("Found %u entities\n", media->entities_count);
		for(i = 0; i < media->entities_count; i++)
			ISP_PRINT("Entities %d is %s, type is %d!\n", i+1,
				media->entities[i].info.name, media->entities[i].info.type);

		ISP_PRINT("Enumerating pads and links\n");
	}

	ret = media_enum_links(media, verbose);
	if (ret < 0) {
		ISP_ERR("%s: Unable to enumerate pads and linksfor device %s\n",
			__func__, name);
		media_close(media);
		return NULL;
	}

	return media;
}

void media_close(struct media_device *media)
{
	unsigned int i;

	if (media->fd != -1)
		close(media->fd);

	for (i = 0; i < media->entities_count; ++i) {
		struct media_entity *entity = &media->entities[i];

		free(entity->pads);
		free(entity->links);
		if (entity->fd != -1)
			close(entity->fd);
	}

	free(media->entities);
	free(media);
}
