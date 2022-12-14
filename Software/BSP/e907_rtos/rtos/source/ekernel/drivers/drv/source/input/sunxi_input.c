/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <hal_sem.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <sunxi_input.h>
#include <arch.h>

#define input_err(fmt, args...)  printf("%s()%d - "fmt, __func__, __LINE__, ##args)

#define INPUT_SEM_MAX_COUNT 0xFFFFFFFFUL

static INPUT_LIST_HEAD(input_dev_list);
static INPUT_LIST_HEAD(evdev_list);

static unsigned evdev_cnt = 0;

static inline int is_event_support(unsigned int code,
                                   unsigned long *bm, unsigned int max)
{
    return code <= max && input_test_bit(code, bm);
}

static struct sunxi_input_dev *find_input_dev_by_name(const char *name)
{
    struct sunxi_input_dev *dev;

    list_for_each_entry(dev, &input_dev_list, node)
    {
        if (!strcmp(dev->name, name))
        {
            return dev;
        }
    }

    return NULL;
}

#if 0
static struct sunxi_evdev *find_evdev_by_name(const char *name)
{
    struct sunxi_evdev *evdev = NULL;

    list_for_each_entry(evdev, &evdev_list, node)
    {
        if (!strcmp(evdev->name, name))
        {
            return evdev;
        }
    }

    return NULL;
}
#endif

static struct sunxi_evdev *find_evdev_by_fd(int fd)
{
    struct sunxi_evdev *evdev = NULL;

    list_for_each_entry(evdev, &evdev_list, node)
    {
        if (evdev->fd == fd)
        {
            return evdev;
        }
    }

    return NULL;
}

static void evdev_pass_event(struct sunxi_evdev *evdev, struct sunxi_input_event *event)
{
    int ret;

    evdev->buffer[evdev->head++] = *event;
    evdev->head &= EVENT_BUFFER_SIZE - 1;

    if (evdev->head == evdev->tail)
    {
        evdev->tail = (evdev->head - 1) & (EVENT_BUFFER_SIZE - 1);
        evdev->packet_head = evdev->tail;
    }

    if (event->type == EV_SYN && event->code == SYN_REPORT)
    {
        if ((evdev->tail == (evdev->head - 1)) & (EVENT_BUFFER_SIZE - 1))
        {
            evdev->tail = evdev->head;
        }
        else
        {
            evdev->packet_head = evdev->head;
            ret = hal_sem_post(evdev->sem);
            if (ret < 0)
            {
                input_err(" evdev give semaphore err\n");
            }
        }
    }
}

static void input_pass_event(struct sunxi_input_dev *dev, struct sunxi_input_event *event)
{
    struct sunxi_evdev *evdev = NULL;

    //report input event to all evdev (all task that read).
    list_for_each_entry(evdev, &evdev_list, node)
    {
        if (!strcmp(evdev->name, dev->name))
        {
            evdev_pass_event(evdev, event);
        }
    }

}

static void input_handle_event(struct sunxi_input_dev *dev,
                               unsigned int type, unsigned int code, unsigned int value)
{
    int report = 0;
    struct sunxi_input_event event;

    switch (type)
    {
        case EV_SYN :
            report = 1;
            break;
        case EV_KEY :
            /*no repeat procedure like timer check when press down, comment by swb*/
            if (is_event_support(code, dev->keybit, KEY_MAX) && !!__test_bit(code, dev->key) != value)
            {
                if (value)
                {
                    __change_bit(code, &dev->key);
                }
                else
                {
                    __clear_bit(&dev->key);
                }
                report = 1;
            }
            break;
        case EV_ABS :
            if (is_event_support(code, dev->absbit, ABS_MAX))
            {
                report = 1;
            }
            break;
        case EV_REL :
            if (is_event_support(code, dev->relbit, REL_MAX))
            {
                report = 1;
            }
            break;
        case EV_MSC :
            if (is_event_support(code, dev->mscbit, MSC_MAX))
            {
                report = 1;
            }
            break;
        default :
            break;
    }

    if (report)
    {
        event.type = type;
        event.code = code;
        event.value = value;
        input_pass_event(dev, &event);
    }

}

void sunxi_input_event(struct sunxi_input_dev *dev,
                       unsigned int type, unsigned int code, unsigned int value)
{
    uint32_t level;
    if (is_event_support(type, dev->evbit, EV_MAX))
    {
        level = awos_arch_lock_irq();
        input_handle_event(dev, type, code, value);
        awos_arch_unlock_irq(level);
    }

}

static int sunxi_fetch_next_event(struct sunxi_evdev *evdev, struct sunxi_input_event *event)
{
    int have_event;
    uint32_t level;

    level = awos_arch_lock_irq();

    have_event = evdev->packet_head != evdev->tail;
    if (have_event)
    {
        *event = evdev->buffer[evdev->tail++];
        evdev->tail &= EVENT_BUFFER_SIZE - 1;
    }

    awos_arch_unlock_irq(level);

    return have_event;
}


/*------------------------------DRIVER API-------------------------*/
void input_set_capability(struct sunxi_input_dev *dev, unsigned int type, unsigned int code)
{
    switch (type)
    {
        case EV_KEY:
            input_set_bit(code, dev->keybit);
            break;

        case EV_REL:
            input_set_bit(code, dev->relbit);
            break;

        case EV_ABS:
            input_set_bit(code, dev->absbit);
            break;

        case EV_MSC:
            input_set_bit(code, dev->mscbit);
            break;

        default:
            input_err("input_set_capability: unknown type %u (code %u)\n", type, code);
            return;
    }

    input_set_bit(type, dev->evbit);
}

struct sunxi_input_dev *sunxi_input_allocate_device()
{
    struct sunxi_input_dev *dev;

    dev = malloc(sizeof(struct sunxi_input_dev));
    if (dev)
    {
        memset(dev, 0, sizeof(struct sunxi_input_dev));
    }

    return dev;

}

int sunxi_input_register_device(struct sunxi_input_dev *dev)
{
    if (NULL == dev->name)
    {
        input_err("err : input device must have a name\n");
        return -1;
    }

    /* Every input device generates EV_SYN/SYN_REPORT events. */
    input_set_bit(EV_SYN, dev->evbit);

    input_list_add_tail(&dev->node, &input_dev_list);

    return 0;
}

/*---------------------------USER API-------------------------*/
int sunxi_input_open(const char *name)
{
    struct sunxi_input_dev *dev = NULL;
    struct sunxi_evdev *evdev;

    dev = find_input_dev_by_name(name);
    if (NULL == dev)
    {
        input_err("input dev %s is not exist\n", name);
        return -1;
    }

    evdev = (struct sunxi_evdev *)malloc(sizeof(struct sunxi_evdev));
    if (evdev)
    {
        memset(evdev, 0, sizeof(struct sunxi_evdev));
        evdev->name = dev->name;
        evdev->fd = evdev_cnt++;
        evdev->sem = hal_sem_create(0);
        if (evdev->sem == NULL)
        {
            input_err("init Semaphore fail\n");
        }
        input_list_add_tail(&evdev->node, &evdev_list);

        return evdev->fd;
    }

    return -1;
}

int sunxi_input_read(int fd, void *buffer, unsigned int size)
{
    unsigned int count = 0;
    int ret;
    struct sunxi_evdev *evdev = NULL;
    struct sunxi_input_event event;

    evdev = find_evdev_by_fd(fd);
    if (NULL == evdev)
    {
        input_err("input read fd err\n");
        return -1;
    }

    if (evdev->packet_head == evdev->tail)
    {
        ret = hal_sem_timedwait(evdev->sem, 500);
        if (ret < 0)
        {
            return -1;
        }
    }

    while (count + sizeof(struct sunxi_input_event) <= size
           && sunxi_fetch_next_event(evdev, &event))
    {
        memcpy(buffer + count, &event, sizeof(struct sunxi_input_event));
        count += sizeof(struct sunxi_input_event);
    }

    return count;
}
