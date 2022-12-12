
/*
 ******************************************************************************
 *
 * events.c
 *
 * Hawkview ISP - events.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/28	VIDEO INPUT
 *
 *****************************************************************************
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/select.h>

#include "../isp_dev/list.h"
#include "device/isp_dev.h"
#include "../isp_dev/tools.h"

#include "events.h"

#define SELECT_TIMEOUT		3000		/* milliseconds */

struct event_fd {
	struct list_entry list;

	int fd;
	enum hw_isp_event_type type;
	void (*callback)(void *priv);
	void *priv;
};

void events_monitor_fd(struct event_list *events, int fd,
		     enum hw_isp_event_type type,
		     void(*callback)(void *), void *priv)
{
	struct event_fd *event;

	event = malloc(sizeof *event);
	if (event == NULL)
		return;

	event->fd = fd;
	event->type = type;
	event->callback = callback;
	event->priv = priv;

	switch (event->type) {
	case HW_ISP_EVENT_READ:
		FD_SET(fd, &events->rfds);
		break;
	case HW_ISP_EVENT_WRITE:
		FD_SET(fd, &events->wfds);
		break;
	case HW_ISP_EVENT_EXCEPTION:
		FD_SET(fd, &events->efds);
		break;
	}

	events->maxfd = max(events->maxfd, fd);

	list_add_tail(&event->list, &events->events);
}

void events_unmonitor_fd(struct event_list *events, int fd)
{
	struct event_fd *event = NULL;
	struct event_fd *entry;
	int maxfd = 0;

	list_for_each_entry(entry, &events->events, list) {
		if (entry->fd == fd)
			event = entry;
		else
			maxfd = max(maxfd, entry->fd);
	}

	if (event == NULL)
		return;

	switch (event->type) {
	case HW_ISP_EVENT_READ:
		FD_CLR(fd, &events->rfds);
		break;
	case HW_ISP_EVENT_WRITE:
		FD_CLR(fd, &events->wfds);
		break;
	case HW_ISP_EVENT_EXCEPTION:
		FD_CLR(fd, &events->efds);
		break;
	}

	events->maxfd = maxfd;
	list_del(&event->list);
	free(event);
}

static void events_dispatch(struct event_list *events, const fd_set *rfds,
			    const fd_set *wfds, const fd_set *efds)
{
	struct event_fd *event;
	struct event_fd *next;

	list_for_each_entry_safe(event, next, &events->events, list) {
		if (event->type == HW_ISP_EVENT_READ &&
		    FD_ISSET(event->fd, rfds))
			event->callback(event->priv);
		else if (event->type == HW_ISP_EVENT_WRITE &&
			 FD_ISSET(event->fd, wfds))
			event->callback(event->priv);
		else if (event->type == HW_ISP_EVENT_EXCEPTION &&
			 FD_ISSET(event->fd, efds))
			event->callback(event->priv);

		/* If the callback stopped events processing, we're done. */
		if (events->done)
			break;
	}
}

bool events_loop(struct event_list *events)
{
	while (!events->done) {
		struct timeval timeout;
		fd_set rfds;
		fd_set wfds;
		fd_set efds;
		int ret;

		timeout.tv_sec = SELECT_TIMEOUT / 1000;
		timeout.tv_usec = (SELECT_TIMEOUT % 1000) * 1000;
		rfds = events->rfds;
		wfds = events->wfds;
		efds = events->efds;

		ret = select(events->maxfd + 1, &rfds, &wfds, &efds, &timeout);
		if (ret < 0) {
			/* EINTR means that a signal has been received, continue
			 * to the next iteration in that case.
			 */
			if (errno == EINTR)
				continue;

			ISP_ERR("isp%d event select failed with %s [%d]\n", \
					events->isp_id, strerror(errno), errno);
			break;
		}
		if (ret == 0) {
			/* select() should never time out as the ISP is supposed
			 * to capture images continuously. A timeout is thus
			 * considered as a fatal error.
			 */
			ISP_ERR("isp%d event select timeout\n", events->isp_id);
			continue;
		}

		events_dispatch(events, &rfds, &wfds, &efds);
	}

	return !events->done;
}

void events_star(struct event_list *events)
{
	events->done = false;
}
void events_stop(struct event_list *events)
{
	events->done = true;
}

void events_init(struct event_list *events)
{
	memset(events, 0, sizeof *events);

	FD_ZERO(&events->rfds);
	FD_ZERO(&events->wfds);
	FD_ZERO(&events->efds);
	events->maxfd = 0;
	INIT_LIST_HEAD(&events->events);
}

