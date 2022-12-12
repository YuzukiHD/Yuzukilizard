
#define LOG_TAG "misc_rw"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <utils/Log.h>
#include <sys/stat.h>
#include <sys/types.h>


static const char *MISC_DEVICE = "/dev/block/by-name/misc";

#define LOGE(...) fprintf(stderr, "E:" __VA_ARGS__)

/* Bootloader Message
 */
struct bootloader_message {
    char command[32];
    char status[32];
    char recovery[1024];
};


// ------------------------------------
// for misc partitions on block devices
// ------------------------------------

static int get_bootloader_message_block(struct bootloader_message *out,
                                        const char* device) {
    FILE* f = fopen(device, "rb");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    struct bootloader_message temp;
    int count = fread(&temp, sizeof(temp), 1, f);
    if (count != 1) {
        LOGE("Failed reading %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    memcpy(out, &temp, sizeof(temp));
    return 0;
}

static int set_bootloader_message_block(const struct bootloader_message *in,
                                        const char* device) {
	LOGE(" the open device is %s\n",device);									
    FILE* f = fopen(device, "wb");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    int count = fwrite(in, sizeof(*in), 1, f);
    if (count != 1) {
        LOGE("Failed writing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
	fsync(f);
    fsync(fileno(f));
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    return 0;
}

/* force the next boot to recovery/efex */
int write_misc(char *reason){
	struct bootloader_message boot, temp;
	char device[32] = {0};
	memset(&boot, 0, sizeof(boot));
	if(!strcmp("recovery",reason)){
       reason = "boot-recovery";
	}
//	strcpy(boot.command, "boot-recovery");
	strcpy(boot.command,reason);
	sprintf(device,"%s",MISC_DEVICE);
	if (set_bootloader_message_block(&boot, device) )
		return -1;

	//read for compare
	memset(&temp, 0, sizeof(temp));
	if (get_bootloader_message_block(&temp, device))
		return -1;

	if( memcmp(&boot, &temp, sizeof(boot)) )
		return -1;

	return 0;

}

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *   --set_encrypted_filesystem=on|off - enables / diasables encrypted fs
 */
static const char *COMMAND_FILE = "/cache/recovery/command";

int go_update_package(const char *path){


	return 0;
}

#define CRASH_MSG_MAGIC		0xCAA58FAC
#define CRASH_MSG_OFFSET	1048576
#define MAX_CRASH_COUNT		3

struct crash_message {
    unsigned int magic_s;
    int count;
    char reason[512];
    int ever_wiped;
    unsigned int magic_e;
//    int crash_hist[MAX_CRASH_COUNT + 1];
//    int mount_failed;
//    int remounted_ro;
//    int critical_crashed;
//    int normal;
    int unknown;
};

static int get_crash_message(struct crash_message *out,
                                        const char* device) {
    FILE* f = fopen(device, "rb");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    struct crash_message temp;
    fseek(f, CRASH_MSG_OFFSET, SEEK_SET);
    int count = fread(&temp, sizeof(temp), 1, f);
    if (count != 1) {
        LOGE("Failed reading %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    memcpy(out, &temp, sizeof(temp));
    return 0;
}

static int set_crash_message(const struct crash_message *in,
                                        const char* device) {
    FILE* f = fopen(device, "wb");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    fseek(f, CRASH_MSG_OFFSET, SEEK_SET);
    int count = fwrite(in, sizeof(*in), 1, f);
    if (count != 1) {
        LOGE("Failed writing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    fflush(f);
    fsync(fileno(f));
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    return 0;
}

static const char *RECOV_DIR = "/cache/recovery";

int write_command_file(const char* command)
{
	struct stat st;
	LOGE("%s: %s", __FUNCTION__, command);

	if(stat(RECOV_DIR, &st) != 0) {
		if(mkdir(RECOV_DIR, 0777) < 0){
			LOGE("Can't mkdir %s\n(%s)\n", RECOV_DIR, strerror(errno));
	        return -1;
		}
	}
	remove(COMMAND_FILE);

    FILE* f = fopen(COMMAND_FILE, "w");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", COMMAND_FILE, strerror(errno));
        return -1;
    }

   	int count = fwrite(command, strlen(command), 1, f);

    if (count != 1) {
        LOGE("Failed writing %s\n(%s)\n", COMMAND_FILE, strerror(errno));
        return -1;
    }
    fflush(f);
    fsync(fileno(f));
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", COMMAND_FILE, strerror(errno));
        return -1;
    }

    return 0;
}

/*
 *返回值：
 *    0 : 只做reboot
 *    -1: 访问misc分区出错，不做操作
 *    1 : 连续出错次数达到上限
 *    2 : 已经恢复出厂设置，但仍然出错，不做操作
 */

int set_misc_crash(const char *reason, const char *error_msg)
{
    struct crash_message crash, temp;
    int ret = 0;

    memset(&crash, 0, sizeof(crash));
    if (get_crash_message(&crash, MISC_DEVICE)) {
    	return -1;
    }
	
    if (crash.magic_s == CRASH_MSG_MAGIC && crash.magic_e == CRASH_MSG_MAGIC) {
    	if (crash.ever_wiped > 0)
			return 2;
    	if (crash.count < 0) {
    		crash.count = 0;
    	}
    } else {
    	crash.magic_s = crash.magic_e = CRASH_MSG_MAGIC;
    	crash.count = 0;
    }
    
    if (crash.count < MAX_CRASH_COUNT - 1) {
		++crash.count;
	} else {
		ret = 1;
		crash.ever_wiped = 1;
		crash.count = 0;
	}

    memset(crash.reason, 0, sizeof(crash.reason));
    strncpy(crash.reason, error_msg, sizeof(crash.reason));

    if (set_crash_message(&crash, MISC_DEVICE) < 0)
        return -1;

    //read for compare
    memset(&temp, 0, sizeof(temp));
    if (get_crash_message(&temp, MISC_DEVICE) < 0)
        return -1;

    if (memcmp(&crash, &temp, sizeof(crash)))
        return -1;
	
	if (ret && reason != NULL) {
		return 1;
	}

    return ret;
}

int clear_crash_message()
{
	struct crash_message crash, temp;

    memset(&temp, 0, sizeof(temp));
    if (get_crash_message(&temp, MISC_DEVICE) == 0) {
    	if (temp.magic_s != CRASH_MSG_MAGIC || temp.magic_e != CRASH_MSG_MAGIC)
        	return 0;
    }

	memset(&crash, 0, sizeof(crash));
    if (set_crash_message(&crash, MISC_DEVICE) < 0)
        return -1;

    //read for compare
    memset(&temp, 0, sizeof(temp));
    if (get_crash_message(&temp, MISC_DEVICE) < 0)
        return -1;

    if (memcmp(&crash, &temp, sizeof(crash)))
        return -1;
    return 0;
}

/*
 *返回值：
 *    -1: 检测不到上次启动的crash信息（包括访问misc分区出错）
 *    0 : 上一次运行过程中系统有出错
 *    1 : 已经进行过恢复出厂设置
 *    2 : 设置恢复出厂设置命令失败
 */
int get_last_crash_reason(char *reason, int len)
{
	struct crash_message msg;
	int ret = 0;

	memset(&msg, 0, sizeof(msg));
	if (get_crash_message(&msg, MISC_DEVICE) < 0)
        return -1;

	if (msg.magic_s != CRASH_MSG_MAGIC || msg.magic_e != CRASH_MSG_MAGIC) {
		return -1;
	}

	if (msg.ever_wiped == 1) {
		msg.ever_wiped = 2;
		set_crash_message(&msg, MISC_DEVICE);
		ret = 1;
	} else if (msg.ever_wiped == 2) {
		ret = 2;
	}
    strncpy(reason, msg.reason, len);
    return ret;
}