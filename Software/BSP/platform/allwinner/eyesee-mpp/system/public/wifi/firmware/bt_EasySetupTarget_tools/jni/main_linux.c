#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "easy_setup.h"

int killed = 0;
int debug_enable = 0;

void usage() {
    printf("-h: show help message\n");
    printf("-d: show debug message\n");
    printf("-k <v>: set 16-char key for all protocols\n");
    printf("-p <v>: bitmask of protocols to enable\n");
    printf("  0x%04x - bcast\n", 1<<EASY_SETUP_PROTO_BCAST);
    printf("  0x%04x - neeze\n", 1<<EASY_SETUP_PROTO_NEEZE);
    printf("  0x%04x - Air Kiss\n", 1<<EASY_SETUP_PROTO_AKISS);
    printf("  0x%04x - Xiaoyi\n", 1<<EASY_SETUP_PROTO_XIAOYI);
    printf("  0x%04x - changhong\n", 1<<EASY_SETUP_PROTO_CHANGHONG);
    printf("  0x%04x - jingdong\n", 1<<EASY_SETUP_PROTO_JINGDONG);
    printf("  0x%04x - jd JoyLink\n", 1<<EASY_SETUP_PROTO_JD);
}

static void signal_handler(int sig) {
    printf(" sig  aborted do signal_handler! \n");
    killed = 1;
}

int main(int argc, char* argv[])
{
    int ret;
    int len;
    uint16 val;

    for (;;) {
        int c = getopt(argc, argv, "dhk:p:");
        if (c < 0) {
            break;
        }

        switch (c) {
            case 'd':
                debug_enable = 1;
                break;
            case 'k':
                bcast_set_key(optarg);
                bcast_set_key_qqcon(optarg);
                neeze_set_key(optarg);
                neeze_set_key_qqcon(optarg);
                akiss_set_key(optarg);
                //jingdong_set_key(optarg);
                //jd_set_key(optarg);
                break;
            case 'p':
                sscanf(optarg, "%04x", (uint32*)&val);
                easy_setup_enable_protocols(val);
                break;
            case 'h':
                usage();
                return 0;
            default:
                usage();
                return 0;
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ret = easy_setup_start();
    if (ret) return ret;

    /* query for result, blocks until bcast comes or times out */
    ret = easy_setup_query();
    if (!ret) {
        char ssid[33]; /* ssid of 32-char length, plus trailing '\0' */
        ret = easy_setup_get_ssid(ssid, sizeof(ssid));
        if (!ret) {
            printf("ssid: %s\n", ssid);
        }

        char password[65]; /* password is 64-char length, plus trailing '\0' */
        ret = easy_setup_get_password(password, sizeof(password));
        if (!ret) {
            printf("password: %s\n", password);
        }

        uint8 protocol;
        ret = easy_setup_get_protocol(&protocol);
        if (ret) {
            printf("failed getting protocol.\n");
        } else if (protocol == EASY_SETUP_PROTO_BCAST) {
            char ip[16]; /* ipv4 max length */
            ret = bcast_get_sender_ip(ip, sizeof(ip));
            if (!ret) {
                printf("sender ip: %s\n", ip);
            }

            uint16 port;
            ret = bcast_get_sender_port(&port);
            if (!ret) {
                printf("sender port: %d\n", port);
            }
        } else if (protocol == EASY_SETUP_PROTO_NEEZE) {
            char ip[16]; /* ipv4 max length */
            ret = neeze_get_sender_ip(ip, sizeof(ip));
            if (!ret) {
                printf("sender ip: %s\n", ip);
            }

            uint16 port;
            ret = neeze_get_sender_port(&port);
            if (!ret) {
                printf("sender port: %d\n", port);
            }
        } else if (protocol == EASY_SETUP_PROTO_AKISS) {
            uint8 random;
            ret = akiss_get_random(&random);
            if (!ret) {
                printf("random: 0x%02x\n", random);
            }
        } else if (protocol == EASY_SETUP_PROTO_CHANGHONG) {
            uint8 sec;
            ret = changhong_get_sec_mode(&sec);
            if (!ret) {
                printf("sec mode: 0x%02x\n", sec);
            }
        } else if (protocol == EASY_SETUP_PROTO_JINGDONG) {
            uint8 sec;
            //ret = jingdong_get_sec_mode(&sec);
            if (!ret) {
                printf("sec mode: 0x%02x\n", sec);
            }
        } else if (protocol == EASY_SETUP_PROTO_JD) {
            uint8 crc;
            //ret = jd_get_crc(&crc);
            if (!ret) {
                printf("crc: 0x%02x\n", crc);
            }

            uint32 ip;
            //ret = jd_get_ip(&ip);
            if (!ret) {
                printf("ip: 0x%08x\n", ip);
            }

            uint16 port;
            //ret = jd_get_port(&port);
            if (!ret) {
                printf("port: 0x%04x\n", port);
            }
        } else if (protocol == EASY_SETUP_PROTO_XIAOYI) {
            uint8 buf[128];
            uint8 len = sizeof(buf);
            //ret = xiaoyi_get_buffer(&len, buf);
            if (!ret) {
                printf("buf(%d) - ", len);
                int i;
                for (i=0; i<len; i++) printf("%02x ", buf[i]);
                printf("\n");
            }
        }

#if 0
        /* if easy_setup_get_security() returns -1, try it more times */
        int tries = 3;
        while (tries--) {
            ret = easy_setup_get_security();
            if (ret != -1) break;
        }
        printf("security: ");
        if (ret == WLAN_SECURITY_WPA2) printf("wpa2\n");
        else if (ret == WLAN_SECURITY_WPA) printf("wpa\n");
        else if (ret == WLAN_SECURITY_WEP) printf("wep\n");
        else if (ret == WLAN_SECURITY_NONE) printf("none\n");
        else printf("wpa2");
#endif
    }

    /* must do this! */
    easy_setup_stop();

    return 0;
}

