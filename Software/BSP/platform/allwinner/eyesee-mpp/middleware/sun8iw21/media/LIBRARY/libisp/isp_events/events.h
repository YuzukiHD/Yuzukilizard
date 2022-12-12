
/*
 ******************************************************************************
 *
 * events.h
 *
 * Hawkview ISP - events.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/21	VIDEO INPUT
 *
 *****************************************************************************
 */

#ifndef _EVENTS_H_
#define _EVENTS_H_

#include <stdbool.h>
#include <sys/select.h>

#include "../isp_dev/tools.h"

struct event_list {
	struct list_entry events;
	bool done;
	int isp_id;

	int maxfd;
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
};

void events_monitor_fd(struct event_list *events, int fd,
				     enum hw_isp_event_type type,
				     void(*callback)(void *), void *priv);
void events_unmonitor_fd(struct event_list *events, int fd);
bool events_loop(struct event_list *events);
void events_star(struct event_list *events);
void events_stop(struct event_list *events);
void events_init(struct event_list *events);

#endif /*_EVENTS_H_*/

