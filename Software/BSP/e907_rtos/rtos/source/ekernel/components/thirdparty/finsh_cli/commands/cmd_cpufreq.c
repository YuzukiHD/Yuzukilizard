#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include <sunxi_hal_common.h>
#include <rtthread.h>
#include <hal_osal.h>
#include <hal_cmd.h>
#include <hal_cpufreq.h>

#include <finsh.h>
#include <finsh_api.h>
#include <mod_defs.h>
#include <errno.h>
#include <sys/time.h>

#define TEST_NUMBER	(10000)

void cmd_cpufreq_help(void)
{
	char buff[] = \
	"-l: list support freqtable.\n"
	"-s: set freq, A parameter is required.\n"
	"-g: get current freq.\n"
	"-t: test demo.\n"
	"-h: print the message.\n"
	"\n";
	printf("%s", buff);
}

int cmd_cpufreq_test_seq(int size)
{
	int ret, index;
	int testcnt = 0, okcnt = 0;
	int sfreq, gfreq;

	/* sequence test */
	printf("==== cpufreq test start: sequence ====\n");
	while (1) {
		index = (testcnt % size);
		testcnt++;

		sfreq = hal_cpufreq_get_freq_table_freq(index);
		if (gfreq <= 0) {
			printf("Error: get freq table freq \
			failed, index %d, return %d",
					index, sfreq);
			break;
		}
		printf("%d, Set frequency: %d Hz.\n", testcnt, sfreq);
		ret = hal_cpufreq_set_freq(sfreq);

		gfreq = hal_cpufreq_get_freq();
		if (gfreq <= 0) {
			printf("Error: get freq failed, return %d\n", gfreq);
			break;
		}
		printf("%d, Get frequency: %d Hz.\n", testcnt, gfreq);
		if (gfreq == sfreq)
			okcnt++;

		rt_thread_mdelay(10);

		if (testcnt == (int)TEST_NUMBER) {
			if (okcnt == testcnt) {
				printf("==== seq test complete, success ====\n");
			} else
				printf("==== seq test complete, fail ====\n");
			break;
		}
	}

	return 0;
}

int cmd_cpufreq_test_minmax(int size)
{
	int ret, index;
	int testcnt = 0, okcnt = 0;
	int sfreq, gfreq;
	bool flag = false;

	/* minmax test */
	printf("==== cpufreq test start: minmax ====\n");
	while (1) {
		if (!flag)
			index = size - 1;
		else
			index = 0;
		flag = !flag;
		testcnt++;

		sfreq = hal_cpufreq_get_freq_table_freq(index);
		if (gfreq <= 0) {
			printf("Error: get freq table freq "
				"failed, index %d, return %d",
					index, sfreq);
			break;
		}
		printf("%d, Set frequency: %d Hz.\n", testcnt, sfreq);
		ret = hal_cpufreq_set_freq(sfreq);

		gfreq = hal_cpufreq_get_freq();
		if (gfreq <= 0) {
			printf("Error: get freq failed, return %d\n", gfreq);
			break;
		}
		printf("%d, Get frequency: %d Hz.\n", testcnt, gfreq);
		if (gfreq == sfreq)
			okcnt++;

		rt_thread_mdelay(10);

		if (testcnt == 2 * (int)TEST_NUMBER) {
			if (okcnt == testcnt) {
				printf("==== minmax test complete, success "
					"====\n");
			} else
				printf("==== minmax test complete, fail ====\n");
			break;
		}
	}

	return 0;
}

int cmd_cpufreq_test_random(int size)
{
	int ret, index;
	int testcnt = 0, okcnt = 0;
	int sfreq, gfreq;
	struct timeval time;

	/* random test */
	printf("==== cpufreq test start: random ====\n");
	gettimeofday(&time, NULL);
	srand((unsigned int)time.tv_sec);
	while (1) {
		index = (rand() % size);
		testcnt++;

		sfreq = hal_cpufreq_get_freq_table_freq(index);
		if (gfreq <= 0) {
			printf("Error: get freq table freq "
				"failed, index %d, return %d",
						index, sfreq);
			break;
		}
		printf("%d, Set frequency: %d Hz.\n", testcnt, sfreq);
		ret = hal_cpufreq_set_freq(sfreq);

		gfreq = hal_cpufreq_get_freq();
		if (gfreq <= 0) {
			printf("Error: get freq failed, return %d\n", gfreq);
			break;
		}
		printf("%d, Get frequency: %d Hz.\n", testcnt, gfreq);
		if (gfreq == sfreq)
			okcnt++;

		rt_thread_mdelay(10);

		if (testcnt == (int)TEST_NUMBER) {
			if (okcnt == testcnt) {
				printf("==== random test complete, success "
					"====\n");
			} else
				printf("==== random test complete, fail ====\n");
			break;
		}
	}

	return 0;
}


int cmd_cpufreq(int argc, char **argv)
{
	int ret, i;
	int opt, size;
	int sfreq, gfreq = -1, savefreq;

	while ((opt = getopt(argc, argv, "ltghs:")) != -1) {
		switch (opt) {
		case 'l':
			size = hal_cpufreq_get_freq_table_size();
			if (size <= 0) {
				printf("Error: get freq table size failed, "
					"return %d\n", size);
				break;
			}

			for (i = 0; i < size; i++) {
				gfreq = hal_cpufreq_get_freq_table_freq(i);
				if (gfreq <= 0) {
					printf("Error: get freq table freq "
					"failed, index %d, return %d",
							i, gfreq);
					break;
				}
				printf("%d ", gfreq);
			}
			printf("\n");
			break;
		case 's':
			sfreq = atoi(optarg);
			if (sfreq < 0) {
				printf("Error: value less than 0\n");
				return -EINVAL;
			}
			ret = hal_cpufreq_set_freq((u32)sfreq);
			if (ret)
				printf("Error: set freq failed, return %d\n",
									ret);
			break;
		case 'g':
			gfreq = hal_cpufreq_get_freq();
			if (gfreq <= 0)
				printf("Error: get freq failed, return %d\n",
									gfreq);
			else
				printf("%d\n", gfreq);
			break;
		case 't':
			size = hal_cpufreq_get_freq_table_size();
			if (size <= 0) {
				printf("Error: get freq table size failed, "
					"return %d\n", size);
				break;
			}

			savefreq = hal_cpufreq_get_freq();
			if (savefreq <= 0) {
				printf("Error: get freq failed, return %d\n",
								savefreq);
				break;
			} else
				printf ("save current frequency: %d Hz\n",
								savefreq);

			cmd_cpufreq_test_seq(size);
			cmd_cpufreq_test_minmax(size);
			cmd_cpufreq_test_random(size);

			ret = hal_cpufreq_set_freq((u32)savefreq);
			if (ret) {
				printf("Error: set freq failed, return %d\n",
									ret);
				break;
			}

			printf("restore frequency: %d Hz\n", savefreq);
			break;
		case 'h':
		default:
			cmd_cpufreq_help();
			break;
		}

	}

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_cpufreq, cpufreq, cmd cpufreq);

