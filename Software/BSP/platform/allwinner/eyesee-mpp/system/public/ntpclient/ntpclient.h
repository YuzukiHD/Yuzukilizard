#ifndef _NTP_CLIENT_H_
#define _NTP_CLIENT_H_

#define DEF_NTP_HEAD_LI        (0  << 30)
#define DEF_NTP_HEAD_VN        (3  << 27)
#define DEF_NTP_HEAD_MODE      (3  << 24)
#define DEF_NTP_HEAD_STARTUM   (0  << 16)
#define DEF_NTP_HEAD_POLL      (4  << 8)
#define DEF_NTP_HEAD_PRECISION (-6 && 0xFF)

/**
 * 该宏用于转换微秒到NTP时间戳小数部分，
 * 即将usec*4294.967296(usec*10^6/(2^32/10^12))，转换为非浮点数运算
 */
#define NTPFRAC(x) (4294 * (x) + ((1981 * (x))>>11))
/* 上述宏的逆过程 */
#define USEC(x) (((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16))

#define DEF_NTP_SERVER "pool.ntp.org"     //ntp官方时间
#define DEF_NTP_PORT   123

/**
 * 因为协议定义的NTP timestamp format是从1900年0H开始计算的，
 * 但UTC是从1970年0H开始算的，所以这里需要一个偏移量
 *
 * 从1900年-1970年的秒数，17为润年多出来的天数
 */
const unsigned long OFFSET_1900_TO_1970 = ((365ul * 70ul + 17ul) * 24ul * 60ul * 60ul);

typedef struct ntp_timestamp {
    unsigned int integer;
    unsigned int fraction;
} ntp_timestamp_t;

#endif /* define ntpclient.h */
