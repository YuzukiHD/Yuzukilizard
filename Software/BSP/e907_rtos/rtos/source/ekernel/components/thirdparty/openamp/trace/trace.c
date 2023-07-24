#include <stdio.h>
#include <sunxi_hal_common.h>
#include <hal_interrupt.h>
#include <hal_cache.h>
#include <hal_cmd.h>
#include <string.h>
#include <ktimer.h>
#include <inttypes.h>

#define WAIT_TIME					(1 * 1000 * 1000)  // 1ms

#define FIFO_RESERVED_SIZE              (32)
#define LOG_BUF_SIZE				(sizeof(amp_log_buffer) - FIFO_RESERVED_SIZE)

char amp_log_buffer[CONFIG_AMP_TRACE_BUF_SIZE] __attribute__((aligned(CACHELINE_LEN))) = {0};

#pragma pack(1)
struct _sh_fifo {
	uint32_t	start;
	uint32_t	end;
	uint32_t	len;

	uint16_t	exclusive;
	uint16_t	isfull;
	uint16_t	reading;
	uint16_t	pad;
};
#pragma pack()

/* return true if time a after time b */
#define time_after(a, b) \
	(((long)((unsigned long)(b) - (unsigned long)(a))) < 0)
/* return true if a before ktime */
#define time_is_before_ktime(a)  time_after(ktime_get(), a)

#define read_fifo(fifo, member) \
({ \
	hal_dcache_invalidate((unsigned long)(&fifo->member), sizeof(fifo->member)); \
	fifo->member; \
})

#define write_fifo(fifo, member, val) \
({ \
	fifo->member = val; \
	hal_dcache_clean((unsigned long)(&fifo->member), sizeof(fifo->member)); \
})

static inline int ringbuffer_get_unused(struct _sh_fifo *fifo)
{
	int isfull = read_fifo(fifo, isfull);
	uint32_t start, end;

	if (isfull)
		return 0;

	start = read_fifo(fifo, start);
	end = read_fifo(fifo, end);

	if (start > end)
		return start - end;
	else
		return LOG_BUF_SIZE - (end - start);
}

static int ringbuffer_put(struct _sh_fifo *fifo, void *from, int size)
{
	unsigned long flags;
	int remain, len;
	uint32_t start, end;
	void *data = &amp_log_buffer[FIFO_RESERVED_SIZE];
	int cross = 0;

	hal_dcache_invalidate((unsigned long)fifo, sizeof(*fifo));

	write_fifo(fifo, exclusive, read_fifo(fifo, exclusive) + 1);
	flags = hal_interrupt_save();

	/* input too long, we clear buffer */
	if (size > LOG_BUF_SIZE) {
		from += size - LOG_BUF_SIZE;
		size = LOG_BUF_SIZE;
		if (fifo->reading) {
			uint64_t timeout;
			timeout = (uint64_t)ktime_get() + WAIT_TIME;
			while (time_is_before_ktime(timeout) && read_fifo(fifo, reading));
			/* if failed we force to move read pointer */
		}
		write_fifo(fifo, start, 0);
		write_fifo(fifo, end, 0);
		write_fifo(fifo, isfull, 0);
		goto put;
	}

	remain = ringbuffer_get_unused(fifo);
	if (remain < size) {
		/* not enough space, we need to move read pointer */
		if (read_fifo(fifo, reading)) {
			uint64_t timeout;
			timeout = (uint64_t)ktime_get() + WAIT_TIME;
			while (time_is_before_ktime(timeout) && read_fifo(fifo, reading));
			/* if failed we force to move read pointer */
		}
		start = read_fifo(fifo, start);
		start += size - remain;
		start %= LOG_BUF_SIZE;
		write_fifo(fifo, start, start);
		write_fifo(fifo, isfull, 0);
	}

put:
	len = ringbuffer_get_unused(fifo);
	len = len > size ? size : len;
	end = read_fifo(fifo, end);

	if (end + len > LOG_BUF_SIZE)
		cross = 1;

	if (cross) {
		int first = LOG_BUF_SIZE - end;

		memcpy(data + end, from, first);
		hal_dcache_clean((unsigned long)(data + end), first);
		memcpy(data, from + first, len - first);
		hal_dcache_clean((unsigned long)(data), len - first);
		end = len - first;
	} else {
		memcpy(data + end, from, len);
		hal_dcache_clean((unsigned long)(data + end), len);
		end += len;
		len %= LOG_BUF_SIZE;
	}

	write_fifo(fifo, end, end);
	start = read_fifo(fifo, start);

	if (end == start && len != 0)
		write_fifo(fifo, isfull, 1);

	hal_interrupt_restore(flags);
	write_fifo(fifo, exclusive, read_fifo(fifo, exclusive) - 1);

	return len;
}

void amp_log_put(char ch)
{
	struct _sh_fifo *fifo = (struct _sh_fifo *)amp_log_buffer;

	ringbuffer_put(fifo, &ch, 1);
}

int amp_log_put_str(char *buf)
{
	struct _sh_fifo *fifo = (struct _sh_fifo *)amp_log_buffer;

	return ringbuffer_put(fifo, buf, strlen(buf));
}

int amp_log_put_buf(char *buf, int len)
{
	struct _sh_fifo *fifo = (struct _sh_fifo *)amp_log_buffer;

	return ringbuffer_put(fifo, buf, len);
}

static int amp_log_stat(int argc, const char **argv)
{
	struct _sh_fifo fifo;

	hal_dcache_invalidate((unsigned long)amp_log_buffer, sizeof(fifo));
	memcpy(&fifo, amp_log_buffer, sizeof(fifo));

	printf("amp fifo stat:\r\n");
	printf("\texclusize: %" PRId16 "\r\n", fifo.exclusive);
	printf("\tstart: %" PRId32 "\r\n", fifo.start);
	printf("\tend : %" PRId32 "\r\n", fifo.end);
	printf("\treading : %" PRId16 "\r\n", fifo.reading);
	printf("\tisfull : %" PRId16 "\r\n", fifo.isfull);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(amp_log_stat, amp_log_stat, dump amp log stat);
