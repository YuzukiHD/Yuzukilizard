/*
*********************************************************************************************************
*                                                    ePDK
*                                    the Easy Portable/Player Develop Kits
*                                             audio driver Module
*
*                                    (c) Copyright 2009-2010, kevin China
*                                             All Rights Reserved
*
* File    : ir_remote.h
* By      : victor
* Version : V1.0
* Date    : 2010-12-31
*********************************************************************************************************
*/
#ifndef _IR_KEYMAP_H_
#define  _IR_KEYMAP_H_

__u32 ir_keymap[256] =
{
    /* 0             1               2             3 */
    KEY_RESERVED, KPAD_VOICEDOWN,   KPAD_DOWN,   KPAD_UP,
    /* 4             5               6             7 */
    KPAD_RETURN, KEY_RESERVED,    KEY_1,  KEY_MEDIA_PREV_TRACK,
    /* 8             9               10            11 */
    KEY_RESERVED, KEY_MEDIA_PREV_TRACK, KEY_RESERVED, KEY_RESERVED,
    /* 12            13              14            15 */
    KEY_RESERVED, KEY_MEDIA_PLAY_PAUSE, KPAD_LEFT, KEY_RESERVED,
    /* 16            17              18             19 */
    KEY_BROWSER_HOME, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 20            21              22             23 */
    KEY_VOLUME_MUTE, KEY_RESERVED,    KEY_4, KEY_RESERVED,
    /* 24            25              26             27 */
    KEY_MEDIA_STOP, KPAD_ENTER,    KPAD_RIGHT, KEY_RESERVED,
    /* 28            29              30             31 */
    KEY_SLEEP, KEY_MEDIA_NEXT_TRACK, KEY_RESERVED, KEY_RESERVED,
    /* 32            33              34            35 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 36            37              38            39 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 40            41              42            43 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 44            45              46            47 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 48            49              50             51 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 52            53              54            55 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 56            57             58            59 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 60            61            62             63 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 64             65            66            67 */
    KEY_5,        KEY_3,      KEY_9,     KEY_RESERVED,
    /* 68            69             70            71 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 72             73            74            75 */
    KEY_8,        KEY_HOME, KEY_RESERVED, KEY_RESERVED,
    /* 76             77            78            79 */
    KEY_0,  KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 80             81            82            83 */
    KPAD_VOICEUP, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 84             85             86            87 */
    KEY_RESERVED,      KEY_6,   KEY_RESERVED, KEY_RESERVED,
    /* 88             89             90            91 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 92             93             94            95 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 96             97             98             99 */
    KEY_MEDIA_NEXT_TRACK, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 100            101            102           103 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 104             105            106          107 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 108             109          100            111 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 112            113             114           115 */
    KEY_BROWSER_FAVORITES, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 116            117              118         119 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 120            121              122        123 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 124            125              126         127 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 128            129              130       131 */
    KEY_RESERVED,       KEY_7,  KEY_RESERVED, KEY_RESERVED,
    /* 132            133              134        135 */
    KPAD_ZOOM,      KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 136            137              138         139 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 140            141              142       143 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 144            145              146        147 */
    KEY_RESERVED,      KEY_2,      KEY_RESERVED, KEY_RESERVED,
    /* 148            149              150        151 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 152            153              154      155 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 156            157             158       159 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 160            161            162         163 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 164             165            166         167 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 168            169             170         171 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 172             173            174         175 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 176             177            178          179 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 180             181            182         183 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 184             185             186        187 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 188             189             190        191 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 192             193             194       195 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 196             197             198        199 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 200             201            202             203 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 204             205            206          207 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 208             209          100            211 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 212            213             214           215 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 216            217              218         219 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 220            221              222        223 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 224            225              226         227 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 228            229              230       231 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 132            233              234        235 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 236            237              238         239 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 240            241              242       243 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 244            245              246        247 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 248            249              250        251 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    /* 252            253              254      255 */
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED

};

#endif

