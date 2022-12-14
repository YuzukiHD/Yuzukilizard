#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mod_defs.h>
#include <list.h>

#include <hal_cmd.h>

extern int32_t console_LKeyDevEvent(__input_dev_t *dev, uint32_t type, uint32_t code, int32_t value);

typedef struct _modlist
{
    struct list_head list;
    __mp *mp;
    __u32 mid;
    char modname[32];
} modlist;

static struct list_head list_mod_head = {NULL, NULL};

static int cmd_insmod(int argc, char **argv)
{
    int i = 0;
    modlist *plistmod = NULL;
    struct list_head *plist = NULL;

    printf("\t install mod para num [%d]: ", argc);
    for (i = 0; i < argc; i ++)
    {
        printf("%s ", argv[i]);
    }
    printf("\r\n");

    if (argc != 2)
    {
        printf("please check install only one mod\r\n");
        return 0;
    }

    if (NULL == list_mod_head.next)
    {
        INIT_LIST_HEAD(&list_mod_head);
    }

    list_for_each(plist, &list_mod_head)
    {
        plistmod = list_entry(plist, modlist, list);
        if (0 == rt_strcmp(argv[1], plistmod->modname))
        {
            printf("%s L%d %s has been installed!\r\n", __FUNCTION__, __LINE__, argv[1]);
            return plistmod->mid;
        }
    }

    plistmod = (modlist *)malloc(sizeof(modlist));
    if (NULL == plistmod)
    {
        printf("%s L%d request mem failed !\r\n", __FUNCTION__, __LINE__);
        return 0;
    }
    memset(plistmod, 0, sizeof(modlist));

    memcpy(plistmod->modname, argv[1], rt_strlen(argv[1]));

    plistmod->mid = esMODS_MInstall(argv[1], 0);

    if (!plistmod->mid)
    {
        printf("%s install fail!\r\n", argv[1]);
        free(plistmod);
        return 0;
    }

    plistmod->mp = esMODS_MOpen(plistmod->mid, 0);

    if (plistmod->mp == 0)
    {
        printf("%s start fail!\r\n", argv[1]);
        esMODS_MUninstall(plistmod->mid);
        free(plistmod);
        return 0;
    }

    list_add_tail(&plistmod->list, &list_mod_head);

    printf("Mod:%s Installed!, mod id=%d\r\n", argv[1], plistmod->mid);
    return plistmod->mid;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_insmod, __cmd_insmod, install a mod);

static int cmd_uninsmod(int argc, char **argv)
{
    int i = 0, ret = 0;
    modlist *plistmod = NULL;
    struct list_head *plist = NULL;

    printf("\t uninsmod mod para num [%d]: ", argc);
    for (i = 0; i < argc; i ++)
    {
        printf("%s ", argv[i]);
    }
    printf("\r\n");

    if (argc != 2)
    {
        printf("please check, only uninstall one mod once\r\n");
        return -1;
    }

    if (NULL == list_mod_head.next)
    {
        INIT_LIST_HEAD(&list_mod_head);
    }

    list_for_each(plist, &list_mod_head)
    {
        plistmod = list_entry(plist, modlist, list);
        if (0 == rt_strcmp(argv[1], plistmod->modname))
        {
            printf("%s L%d %s find the module!\r\n", __FUNCTION__, __LINE__, argv[1]);
            break;
        }
        plistmod = NULL;
    }

    if (NULL == plistmod)
    {
        printf("%s L%d %s module do not be found!\r\n", __FUNCTION__, __LINE__, argv[1]);
        return -1;
    }

    ret = esMODS_MClose(plistmod->mp);

    ret = esMODS_MUninstall(plistmod->mid);

    list_del(&(plistmod->list));
    free(plistmod);

    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_uninsmod, __cmd_uninsmod, uninstall a mod);

typedef struct _keyboard_map
{
    char *key_str;
    int  keycode;
} keyboard_map;

static keyboard_map keymap[] =
{
    {"menu",       KPAD_MENU       },      //
    {"left",       KPAD_LEFT       },      //
    {"right",       KPAD_RIGHT      },     //
    {"up",       KPAD_UP         },        //
    {"down",       KPAD_DOWN       },
    {"return",       KPAD_RETURN     },
    {"volu",       KPAD_VOICEUP    },
    {"vold",       KPAD_VOICEDOWN  },
    {"enter",       KPAD_ENTER      },
    {"poweroff",       KPAD_POWEROFF   },
    {NULL,       0               },
};

static int cmd_send_key(int argc, char **argv)
{
    int32_t i = 0;

    printf("%d parameters, and %s key_string is %s !\r\n", argc, argv[0], argv[1]);

    if (argc != 2)
    {
        printf("send_key [keystring] [value]\r\n"
               "keystring: menu|left|right|up|down|return|enter|volu|vold\r\n");
        return -1;
    }

    for (i = 0; i < 32; i ++)
    {
        if (NULL == keymap[i].key_str)
        {
            printf("the key is not match anyone key in the keymap!\r\n");
            return EPDK_FAIL;
        }

        if (0 == strncmp(argv[1], keymap[i].key_str, strlen(keymap[i].key_str)))
        {
            printf("send key '%s' keycode 0x%X\r\n", keymap[i].key_str, keymap[i].keycode);
            console_LKeyDevEvent(NULL,  EV_KEY, keymap[i].keycode, 1); //1 is down;   0 is up;
            console_LKeyDevEvent(NULL,  EV_SYN, 0, 0);                 //1 is down;   0 is up;
            usleep(20 * 1000);
            console_LKeyDevEvent(NULL,  EV_KEY, keymap[i].keycode,  0);//1 is down;   0 is up;
            console_LKeyDevEvent(NULL,  EV_SYN, 0, 0);                 //1 is down;   0 is up;
            return EPDK_OK;
        }
    }
    return EPDK_OK;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_send_key, __cmd_send_key, send a to keyboar of input system);
