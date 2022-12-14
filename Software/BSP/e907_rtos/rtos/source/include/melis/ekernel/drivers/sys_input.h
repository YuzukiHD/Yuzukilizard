/*
*********************************************************************************************************
*                                                    eMOD
*                                         the melis Operation System
*                                               input sub-system
*                                        input sub-system internal head file
*
*                                    (c) Copyright 2010-2012, sunny China
*                                              All Rights Reserved
*
* File   : sys_input.h
* Version: V1.0
* By     : Sunny
* Date   : 2010-10-27 8:47:23
* Note   : input system extern service for input driver and event user.
*********************************************************************************************************
*/
#ifndef __SYS_INPUT_H__
#define __SYS_INPUT_H__
#include <typedef.h>
#include <ktype.h>
#include <sys_device.h>

/* key action value define */
#define KEY_UP_ACTION                   0
#define KEY_DOWN_ACTION                 1
#define KEY_REPEAT_ACTION               2

/*
 * Event types
 */
#define EV_SYN                          0x00
#define EV_KEY                          0x01
#define EV_REL                          0x02
#define EV_ABS                          0x03
#define EV_MSC                          0x04
#define EV_SW                           0x05
#define EV_LED                          0x11
#define EV_SND                          0x12
#define EV_REP                          0x14
#define EV_FF                           0x15
#define EV_PWR                          0x16
#define EV_FF_STATUS                    0x17
#define EV_MAX                          0x1f
#define EV_CNT                          (EV_MAX+1)

/*
 * Synchronization events.
 */
#define SYN_REPORT                      0
#define SYN_CONFIG                      1
#define SYN_MT_REPORT                   2

/*
 * Keys and buttons,
 * Standard Set.
 */
#define KEY_RESERVED                    0x00
#define KEY_LBUTTON                     0x01
#define KEY_RBUTTON                     0x02
#define KEY_CANCEL                      0x03
#define KEY_MBUTTON                     0x04    /* NOT contiguous with L & RBUTTON */
#define KEY_XBUTTON1                    0x05    /* NOT contiguous with L & RBUTTON */
#define KEY_XBUTTON2                    0x06    /* NOT contiguous with L & RBUTTON */

#define KEY_BACK                        0x08
#define KEY_TAB                         0x09

#define KEY_CLEAR                       0x0C
#define KEY_RETURN                      0x0D

#define KEY_SHIFT                       0x10
#define KEY_CONTROL                     0x11
#define KEY_MENU                        0x12
#define KEY_PAUSE                       0x13
#define KEY_CAPITAL                     0x14

#define KEY_KANA                        0x15
#define KEY_HANGEUL                     0x15  /* old name - should be here for compatibility */
#define KEY_HANGUL                      0x15
#define KEY_JUNJA                       0x17
#define KEY_FINAL                       0x18
#define KEY_HANJA                       0x19
#define KEY_KANJI                       0x19

#define KEY_ESCAPE                      0x1B

#define KEY_CONVERT                     0x1c
#define KEY_NOCONVERT                   0x1d

#define KEY_SPACE                       0x20
#define KEY_PRIOR                       0x21
#define KEY_NEXT                        0x22
#define KEY_END                         0x23
#define KEY_HOME                        0x24
#define KEY_LEFT                        0x25
#define KEY_UP                          0x26
#define KEY_RIGHT                       0x27
#define KEY_DOWN                        0x28
#define KEY_SELECT                      0x29
#define KEY_PRINT                       0x2A
#define KEY_EXECUTE                     0x2B
#define KEY_SNAPSHOT                    0x2C
#define KEY_INSERT                      0x2D
#define KEY_DELETE                      0x2E
#define KEY_HELP                        0x2F

/* KEY_0 thru KEY_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
#define KEY_0                           0x30
#define KEY_1                           0x31
#define KEY_2                           0x32
#define KEY_3                           0x33
#define KEY_4                           0x34
#define KEY_5                           0x35
#define KEY_6                           0x36
#define KEY_7                           0x37
#define KEY_8                           0x38
#define KEY_9                           0x39

/* KEY_A thru KEY_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */
#define KEY_A                           0x41
#define KEY_B                           0x42
#define KEY_C                           0x43
#define KEY_D                           0x44
#define KEY_E                           0x45
#define KEY_F                           0x46
#define KEY_G                           0x47
#define KEY_H                           0x48
#define KEY_I                           0x49
#define KEY_J                           0x4A
#define KEY_K                           0x4B
#define KEY_L                           0x4C
#define KEY_M                           0x4D
#define KEY_N                           0x4E
#define KEY_O                           0x4F
#define KEY_P                           0x50
#define KEY_Q                           0x51
#define KEY_R                           0x52
#define KEY_S                           0x53
#define KEY_T                           0x54
#define KEY_U                           0x55
#define KEY_V                           0x56
#define KEY_W                           0x57
#define KEY_X                           0x58
#define KEY_Y                           0x59
#define KEY_Z                           0x5A

#define KEY_LWIN                        0x5B
#define KEY_RWIN                        0x5C
#define KEY_APPS                        0x5D

#define KEY_SLEEP                       0x5F

#define KEY_NUMPAD0                     0x60
#define KEY_NUMPAD1                     0x61
#define KEY_NUMPAD2                     0x62
#define KEY_NUMPAD3                     0x63
#define KEY_NUMPAD4                     0x64
#define KEY_NUMPAD5                     0x65
#define KEY_NUMPAD6                     0x66
#define KEY_NUMPAD7                     0x67
#define KEY_NUMPAD8                     0x68
#define KEY_NUMPAD9                     0x69
#define KEY_MULTIPLY                    0x6A
#define KEY_ADD                         0x6B
#define KEY_SEPARATOR                   0x6C
#define KEY_SUBTRACT                    0x6D
#define KEY_DECIMAL                     0x6E
#define KEY_DIVIDE                      0x6F
#define KEY_F1                          0x70
#define KEY_F2                          0x71
#define KEY_F3                          0x72
#define KEY_F4                          0x73
#define KEY_F5                          0x74
#define KEY_F6                          0x75
#define KEY_F7                          0x76
#define KEY_F8                          0x77
#define KEY_F9                          0x78
#define KEY_F10                         0x79
#define KEY_F11                         0x7A
#define KEY_F12                         0x7B
#define KEY_F13                         0x7C
#define KEY_F14                         0x7D
#define KEY_F15                         0x7E
#define KEY_F16                         0x7F
#define KEY_F17                         0x80
#define KEY_F18                         0x81
#define KEY_F19                         0x82
#define KEY_F20                         0x83
#define KEY_F21                         0x84
#define KEY_F22                         0x85
#define KEY_F23                         0x86
#define KEY_F24                         0x87

#define KEY_NUMLOCK                     0x90
#define KEY_SCROLL                      0x91

/* KEY_L* & KEY_R* - left and right Alt, Ctrl and Shift virtual keys. */
#define KEY_LSHIFT                      0xA0
#define KEY_RSHIFT                      0xA1
#define KEY_LCONTROL                    0xA2
#define KEY_RCONTROL                    0xA3
#define KEY_LMENU                       0xA4
#define KEY_RMENU                       0xA5

#define KEY_EXTEND_BSLASH               0xE2
#define KEY_OEM_102                     0xE2

#define KEY_PROCESSKEY                  0xE5

#define KEY_ATTN                        0xF6
#define KEY_CRSEL                       0xF7
#define KEY_EXSEL                       0xF8
#define KEY_EREOF                       0xF9
#define KEY_PLAY                        0xFA
#define KEY_ZOOM                        0xFB
#define KEY_NONAME                      0xFC
#define KEY_PA1                         0xFD
#define KEY_OEM_CLEAR                   0xFE

#define KEY_SEMICOLON                   0xBA
#define KEY_EQUAL                       0xBB
#define KEY_COMMA                       0xBC
#define KEY_HYPHEN                      0xBD
#define KEY_PERIOD                      0xBE
#define KEY_SLASH                       0xBF
#define KEY_BACKQUOTE                   0xC0

#define KEY_BROWSER_BACK                0xA6
#define KEY_BROWSER_FORWARD             0xA7
#define KEY_BROWSER_REFRESH             0xA8
#define KEY_BROWSER_STOP                0xA9
#define KEY_BROWSER_SEARCH              0xAA
#define KEY_BROWSER_FAVORITES           0xAB
#define KEY_BROWSER_HOME                0xAC
#define KEY_VOLUME_MUTE                 0xAD
#define KEY_VOLUME_DOWN                 0xAE
#define KEY_VOLUME_UP                   0xAF
#define KEY_MEDIA_NEXT_TRACK            0xB0
#define KEY_MEDIA_PREV_TRACK            0xB1
#define KEY_MEDIA_STOP                  0xB2
#define KEY_MEDIA_PLAY_PAUSE            0xB3
#define KEY_LAUNCH_MAIL                 0xB4
#define KEY_LAUNCH_MEDIA_SELECT         0xB5
#define KEY_LAUNCH_APP1                 0xB6
#define KEY_LAUNCH_APP2                 0xB7

#define KEY_LBRACKET                    0xDB
#define KEY_BACKSLASH                   0xDC
#define KEY_RBRACKET                    0xDD
#define KEY_APOSTROPHE                  0xDE
#define KEY_OFF                         0xDF

/* buttons */
#define BTN_MOUSE                       0x110
#define BTN_LEFT                        0x110
#define BTN_RIGHT                       0x111
#define BTN_MIDDLE                      0x112
#define BTN_SIDE                        0x113
#define BTN_EXTRA                       0x114
#define BTN_FORWARD                     0x115
#define BTN_BACK                        0x116
#define BTN_TASK                        0x117

#define BTN_DIGI                        0x140
#define BTN_TOOL_PEN                    0x140
#define BTN_TOOL_RUBBER                 0x141
#define BTN_TOOL_BRUSH                  0x142
#define BTN_TOOL_PENCIL                 0x143
#define BTN_TOOL_AIRBRUSH               0x144
#define BTN_TOOL_FINGER                 0x145
#define BTN_TOOL_MOUSE                  0x146
#define BTN_TOOL_LENS                   0x147
#define BTN_TOUCH                       0x14a
#define BTN_STYLUS                      0x14b
#define BTN_STYLUS2                     0x14c
#define BTN_TOOL_DOUBLETAP              0x14d
#define BTN_TOOL_TRIPLETAP              0x14e

#define BTN_WHEEL                       0x150
#define BTN_GEAR_DOWN                   0x150
#define BTN_GEAR_UP                     0x151

/* keypad keys, use for PVP remote and so on */
#define KPAD_HOME                       0x200
#define KPAD_MUSIC                      0x201
#define KPAD_VIDIO                      0X202
#define KPAD_PICTURE                    0x203
#define KPAD_TV                         0x204
#define KPAD_FM                         0x205
#define KPAD_UP                         0x206
#define KPAD_DOWN                       0x207
#define KPAD_LCDOFF                     0x208
#define KPAD_RESERVED                   0x209
#define KPAD_NUM0                       0x20A
#define KPAD_NUM1                       0x20B
#define KPAD_NUM2                       0x20C
#define KPAD_NUM3                       0x20D
#define KPAD_NUM4                       0x20E
#define KPAD_NUM5                       0x20F
#define KPAD_NUM6                       0x210
#define KPAD_NUM7                       0x211
#define KPAD_NUM8                       0x212
#define KPAD_NUM9                       0x213
#define KPAD_POWEROFF                   0x214
#define KPAD_ENTER                      0x215
#define KPAD_SCAN                       0x216
#define KPAD_SHUTDOWN                   0x217
#define KPAD_RECORD                     0x218
#define KPAD_ECHO_VOL                   0x219
#define KPAD_TF_USB                    	0x220

/*IR KEY*/
#define IR_KPAD_MUTE                    0x250
#define IR_KPAD_REPEAT                  0x251
#define IR_KPAD_POWERON_OFF             0x252
#define IR_KPAD_PHOTO                   0x253
#define IR_KPAD_MUSIC                   0x254
#define IR_KPAD_MOVIE                   0x255
#define IR_KPAD_STOP                    0x256
#define IR_KPAD_UP                      0x257
#define IR_KPAD_PLAY_PAUSE              0x258
#define IR_KPAD_LEFT                    0x259
#define IR_KPAD_ENTER                   0x25A
#define IR_KPAD_RIGHT                   0x25B
#define IR_KPAD_SETUP                   0x25C
#define IR_KPAD_DOWN                    0x25D
#define IR_KPAD_MODE                    0x25E
#define IR_KPAD_VOLADD                  0x25F
#define IR_KPAD_FF                      0x260
#define IR_KPAD_PREV                    0x261
#define IR_KPAD_VOLDEC                  0x262
#define IR_KPAD_RR                      0x263
#define IR_KPAD_NEXT                    0x264
#define IR_KPAD_EBOOK                   0x265
#define IR_KPAD_POWEROFF                0x266
#define IR_KPAD_MENU                    0x267
#define IR_KPAD_RETURN                  0x268
#define IR_KPAD_VOICEUP                 0x269
#define IR_KPAD_VOICEDOWN               0x26a
#define IR_KPAD_MIC_ONOFF               0x26b
#define IR_KPAD_REC_ONOFF               0x26c
#define IR_KPAD_MIC_ADD                 0x26d
#define IR_KPAD_MIC_DEC                 0x26e
#define IR_KPAD_ECHO                    0x26f
#define IR_KPAD_LYRIC_ADD               0x270
#define IR_KPAD_LYRIC_DEC               0x271
#define IR_KPAD_ACCOMP_ONOFF            0x272

#define IR_KPAD_NUM1                    0x276
#define IR_KPAD_NUM2                    0x277
#define IR_KPAD_NUM3                    0x278
#define IR_KPAD_NUM4                    0x279
#define IR_KPAD_NUM5                    0x280
#define IR_KPAD_NUM6                    0x281
#define IR_KPAD_NUM7                    0x282
#define IR_KPAD_NUM8                    0x283
#define IR_KPAD_NUM9                    0x284
#define IR_KPAD_NUM0                    0x285
#define IR_KPAD_EQ                      0x286
#define IR_KPAD_SINGER                  0x287
#define IR_KPAD_ALPHABET                0x288
#define IR_KPAD_DIGITAL                 0x289
#define IR_KPAD_FAVOR                   0x28a
#define IR_KPAD_NEWSONG                 0x28b
#define IR_KPAD_SEL_LIST                0x28c
#define IR_KPAD_DELETE                  0x28d
#define IR_KPAD_CUTSONG                 0x28e
#define IR_KPAD_AUX                     0x28f
#define IR_KPAD_NTFS_PAL                0x290
#define IR_KAPD_ECHO_SET                0x291
#define KPAD_ECHO_SET                   0x292
#define IR_KPAD_SEL_TIME                0x293

#define KPAD_MUTE                       0X29e

#define KPAD_VOICEDOWN                  0X29f
#define KPAD_VOICEUP                    0X2a0

#define KPAD_RETURN                     0X2a1
#define KPAD_LEFT                       0X2a2
#define KPAD_RIGHT                      0X2a3

#define KPAD_ZOOM                       0X2a4
#define KPAD_MENU                       0X2a5


#define KPAD_MOVIE                      0X2a6
#define KPAD_TVOUT                      0X2a7
#define KPAD_POWER                      0X2a8

#define KPAD_EBOOK                      0X2a9
#define KPAD_MODE                       0X2aa




/* We avoid low common keys in module aliases so they don't get huge. */
#define KEY_MAX                         0x2ff
#define KEY_CNT                         (KEY_MAX+1)

/*
 * Relative axes
 */
#define REL_X                           0x00
#define REL_Y                           0x01
#define REL_Z                           0x02
#define REL_RX                          0x03
#define REL_RY                          0x04
#define REL_RZ                          0x05
#define REL_HWHEEL                      0x06
#define REL_DIAL                        0x07
#define REL_WHEEL                       0x08
#define REL_MISC                        0x09
#define REL_MAX                         0x0f
#define REL_CNT                         (REL_MAX+1)

/*
 * Absolute axes
 */
#define ABS_X                           0x00
#define ABS_Y                           0x01
#define ABS_Z                           0x02
#define ABS_RX                          0x03
#define ABS_RY                          0x04
#define ABS_RZ                          0x05
#define ABS_THROTTLE                    0x06
#define ABS_RUDDER                      0x07
#define ABS_WHEEL                       0x08
#define ABS_GAS                         0x09
#define ABS_BRAKE                       0x0a
#define ABS_HAT0X                       0x10
#define ABS_HAT0Y                       0x11
#define ABS_HAT1X                       0x12
#define ABS_HAT1Y                       0x13
#define ABS_HAT2X                       0x14
#define ABS_HAT2Y                       0x15
#define ABS_HAT3X                       0x16
#define ABS_HAT3Y                       0x17
#define ABS_PRESSURE                    0x18
#define ABS_DISTANCE                    0x19
#define ABS_TILT_X                      0x1a
#define ABS_TILT_Y                      0x1b
#define ABS_TOOL_WIDTH                  0x1c
#define ABS_VOLUME                      0x20
#define ABS_MISC                        0x28
#define ABS_MAX                         0x3f
#define ABS_CNT                         (ABS_MAX+1)

/*
 * Misc events
 */
#define MSC_SERIAL                      0x00
#define MSC_PULSELED                    0x01
#define MSC_GESTURE                     0x02
#define MSC_RAW                         0x03
#define MSC_SCAN                        0x04
#define MSC_MAX                         0x07
#define MSC_CNT                         (MSC_MAX+1)

/*
 * LEDs
 */
#define LED_NUML                        0x00
#define LED_CAPSL                       0x01
#define LED_SCROLLL                     0x02
#define LED_COMPOSE                     0x03
#define LED_KANA                        0x04
#define LED_SLEEP                       0x05
#define LED_SUSPEND                     0x06
#define LED_MUTE                        0x07
#define LED_MISC                        0x08
#define LED_MAIL                        0x09
#define LED_CHARGING                    0x0a
#define LED_MAX                         0x0f
#define LED_CNT                         (LED_MAX+1)

/*
 * Autorepeat values
 */
#define REP_DELAY                       0x00
#define REP_PERIOD                      0x01
#define REP_MAX                         0x01

/*
 * Sounds
 */
#define SND_CLICK                       0x00
#define SND_BELL                        0x01
#define SND_TONE                        0x02
#define SND_MAX                         0x07
#define SND_CNT                         (SND_MAX+1)

#define FF_MAX                          0x7f
#define FF_CNT                          (FF_MAX+1)


/* The class name of INPUT devices */
#define INPUT_CLASS_NAME                "INPUT"

/* INPUT logical devices name, use for graber logical device */
#define INPUT_LKEYBOARD_DEV_NAME        "LKeyBoardDev"
#define INPUT_LMOUSE_DEV_NAME           "LMouseDev"
#define INPUT_LTS_DEV_NAME              "LTSDev"

/* input system limits */
#define INPUT_LOGICALDEV_MAX            16   /*  max logical device number              */
#define INPUT_CHILDREN_MAX              8    /*  max children number of logical device  */
#define INPUT_EV_BUFFER_MAXLEN          256  /*  max event buffer length                */
#define INPUT_MAX_EVENT_NR              64   /*  max event number                       */

/* input device classes */
#define INPUT_KEYBOARD_CLASS            0    /* keyboard class device   */
#define INPUT_MOUSE_CLASS               1    /* mouse class device      */
#define INPUT_TS_CLASS                  2    /* touchpanel class device */

/* basic bit operations */
#define INPUT_BITS_PER_BYTE             8
#define INPUT_DIV_ROUND_UP(n,d)         (((n) + (d) - 1) / (d))
#define INPUT_BITS_PER_LONG             (sizeof(long) * INPUT_BITS_PER_BYTE)
#define INPUT_BIT(nr)                   (1UL << (nr))
#define INPUT_BIT_MASK(nr)              (1UL << ((nr) % INPUT_BITS_PER_LONG))
#define INPUT_BIT_WORD(nr)              ((nr) / INPUT_BITS_PER_LONG)
#define INPUT_BITS_TO_LONGS(nr)         INPUT_DIV_ROUND_UP(nr, INPUT_BITS_PER_LONG)

/* logical device ioctl command */
#define INPUT_SET_REP_PERIOD            0x100

typedef struct __INPUT_PID
{
    uint16_t            product;
    uint16_t            version;
} __input_pid_t;

typedef struct __INPUT_DEV
{
    /* 以下4个成员为注册通用设备所用 */
    const char          *classname;
    const char          *name;
    __dev_devop_t       *ops;
    void                *parg;

    /* 指向通用设备 */
    __hdle              devnode;

    /* 在逻辑设备的物理设备子表数组中的index序号 */
    unsigned char       seq;

    /* 指向该物理设备对应的逻辑设备 */
    __hdle              ldevp;

    /* 设备类型，用于物理输入设备注册时匹配相应的逻辑输入设备 */
    uint32_t            device_class;

    /* 物理输入设备的身份信息 */
    __input_pid_t       id;

    /* 设备支持的事件bitmap，相应位为1表示支持该种事件 */
    unsigned long       evbit[INPUT_BITS_TO_LONGS(EV_CNT)];
    /* 设备支持的按键bitmap，相应位为1表示支持该按键 */
    unsigned long       keybit[INPUT_BITS_TO_LONGS(KEY_CNT)];
    /* 设备支持的相对位移类别bitmap，如x、y、z，相应位为1表示支持该种相对位移 */
    unsigned long       relbit[INPUT_BITS_TO_LONGS(REL_CNT)];
    /* 设备支持的绝对位移类别bitmap，如x、y、volume，相应位为1表示支持该种绝对位移 */
    unsigned long       absbit[INPUT_BITS_TO_LONGS(ABS_CNT)];
    /* 设备支持的led灯种类bitmap，如capslock灯、scroll灯，相应位为1表示支持该种灯 */
    unsigned long       ledbit[INPUT_BITS_TO_LONGS(LED_CNT)];
    /* 设备支持的声音种类bitmap，如click、bell，相应位为1表示支持该种声音 */
    unsigned long       sndbit[INPUT_BITS_TO_LONGS(SND_CNT)];
    /* 设备支持的力反馈种类bitmap，如手柄震动，相应位为1表示支持该种力反馈 */
    unsigned long       ffbit[INPUT_BITS_TO_LONGS(FF_CNT)];

    /* 重复键码 */
    uint32_t            repeat_key;

    /* 重复按键timer */
    __hdle              timer;

    /* 存放当前的绝对值，下一次将被拿来做降噪参考 */
    int32_t             abs[ABS_MAX + 1];

    /*  存放重复按键的各阶段时间间隔 */
    /*  rep[REP_DELAY]存放被忽略的第一次重复按键次数，默认为0 */
    int32_t             rep[REP_MAX + 1];

    /* key/led/snd状态bitmap，用来check对应code的值是否反转 */
    unsigned long       key[INPUT_BITS_TO_LONGS(KEY_CNT)];
    unsigned long       led[INPUT_BITS_TO_LONGS(LED_CNT)];
    unsigned long       snd[INPUT_BITS_TO_LONGS(SND_CNT)];

    /* 用来校正绝对值的信息数组 */
    int32_t             absmax[ABS_MAX + 1];
    int32_t             absmin[ABS_MAX + 1];

    /* 用来降噪的参考值 */
    int32_t             absfuzz[ABS_MAX + 1];

    /* 物理事件处理函数 */
    int32_t             (*event)(struct __INPUT_DEV *dev, __u32 type, __u32 code, __s32 value);

    /* 物理事件缓存区，收到EV_SYN后会转移到循环队列 */
    uint32_t            evbuf[INPUT_EV_BUFFER_MAXLEN];
    /*  物理事件当前指针 */
    int32_t             ev_pos;
} __input_dev_t;

typedef struct __INPUT_EVENT
{
    uint32_t            type;           /*  事件类型，key/rel/abs...    */
    uint32_t            code;           /*  事件代码                    */
    int32_t             value;          /*  事件值                      */
} __input_event_t;

typedef struct __INPUT_EVENT_PACKET
{
    void                *pGrabArg;               /*  注册回调函数时传入的私有句柄*/
    uint32_t            ldev_id;                /*  逻辑设备ID                  */
    uint32_t            pdev_id;                /*  物理设备ID                  */
    int32_t             event_cnt;              /*  事件的个数                  */
    __input_event_t     events[INPUT_MAX_EVENT_NR];/*  事件缓冲区               */
} __input_event_packet_t;


int32_t     input_init(void);
int32_t     input_exit(void);

#endif  /* __SYS_INPUT_H__ */
