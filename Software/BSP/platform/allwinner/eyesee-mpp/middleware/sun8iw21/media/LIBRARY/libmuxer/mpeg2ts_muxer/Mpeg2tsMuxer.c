//#include <CDX_LogNDebug.h>
//#define LOG_NDEBUG 0
#define LOG_TAG "Mpeg2tstsMuxer"
#include <utils/plat_log.h>

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Mpeg2tsMuxer.h"
#include <cedarx_stream.h>

#define CODEC_TYPE_SUBTITLE 3
#define ADTS_HEADER_LENGTH 7

#define CONFIG_HARDCODED_TABLES 0
#define CONFIG_SMALL 0


typedef enum AAC_PROFILE_TYPE
{
    AAC_PROFILE_MP  = 0,
    AAC_PROFILE_LC  = 1,
    AAC_PROFILE_SSR = 2,
    AAC_PROFILE_
} aac_profile_type_t;

static struct {
    uint8_t  le;
    uint8_t  bits;
    uint32_t poly;
} ts_crc_table_params[AV_CRC_MAX] = {
    [AV_CRC_8_ATM]      = { 0,  8,       0x07 },
    [AV_CRC_16_ANSI]    = { 0, 16,     0x8005 },
    [AV_CRC_16_CCITT]   = { 0, 16,     0x1021 },
    [AV_CRC_32_IEEE]    = { 0, 32, 0x04C11DB7 },
    [AV_CRC_32_IEEE_LE] = { 1, 32, 0xEDB88320 },
};

static  AVCRC ts_crc_table[AV_CRC_MAX][257] = {
    [AV_CRC_8_ATM] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31,
        0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
        0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
        0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1,
        0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
        0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE,
        0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
        0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
        0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80,
        0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8,
        0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
        0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
        0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F,
        0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
        0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7,
        0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
        0xFA, 0xFD, 0xF4, 0xF3, 0x01
    },
    [AV_CRC_16_ANSI] = {
        0x0000, 0x0580, 0x0F80, 0x0A00, 0x1B80, 0x1E00, 0x1400, 0x1180,
        0x3380, 0x3600, 0x3C00, 0x3980, 0x2800, 0x2D80, 0x2780, 0x2200,
        0x6380, 0x6600, 0x6C00, 0x6980, 0x7800, 0x7D80, 0x7780, 0x7200,
        0x5000, 0x5580, 0x5F80, 0x5A00, 0x4B80, 0x4E00, 0x4400, 0x4180,
        0xC380, 0xC600, 0xCC00, 0xC980, 0xD800, 0xDD80, 0xD780, 0xD200,
        0xF000, 0xF580, 0xFF80, 0xFA00, 0xEB80, 0xEE00, 0xE400, 0xE180,
        0xA000, 0xA580, 0xAF80, 0xAA00, 0xBB80, 0xBE00, 0xB400, 0xB180,
        0x9380, 0x9600, 0x9C00, 0x9980, 0x8800, 0x8D80, 0x8780, 0x8200,
        0x8381, 0x8601, 0x8C01, 0x8981, 0x9801, 0x9D81, 0x9781, 0x9201,
        0xB001, 0xB581, 0xBF81, 0xBA01, 0xAB81, 0xAE01, 0xA401, 0xA181,
        0xE001, 0xE581, 0xEF81, 0xEA01, 0xFB81, 0xFE01, 0xF401, 0xF181,
        0xD381, 0xD601, 0xDC01, 0xD981, 0xC801, 0xCD81, 0xC781, 0xC201,
        0x4001, 0x4581, 0x4F81, 0x4A01, 0x5B81, 0x5E01, 0x5401, 0x5181,
        0x7381, 0x7601, 0x7C01, 0x7981, 0x6801, 0x6D81, 0x6781, 0x6201,
        0x2381, 0x2601, 0x2C01, 0x2981, 0x3801, 0x3D81, 0x3781, 0x3201,
        0x1001, 0x1581, 0x1F81, 0x1A01, 0x0B81, 0x0E01, 0x0401, 0x0181,
        0x0383, 0x0603, 0x0C03, 0x0983, 0x1803, 0x1D83, 0x1783, 0x1203,
        0x3003, 0x3583, 0x3F83, 0x3A03, 0x2B83, 0x2E03, 0x2403, 0x2183,
        0x6003, 0x6583, 0x6F83, 0x6A03, 0x7B83, 0x7E03, 0x7403, 0x7183,
        0x5383, 0x5603, 0x5C03, 0x5983, 0x4803, 0x4D83, 0x4783, 0x4203,
        0xC003, 0xC583, 0xCF83, 0xCA03, 0xDB83, 0xDE03, 0xD403, 0xD183,
        0xF383, 0xF603, 0xFC03, 0xF983, 0xE803, 0xED83, 0xE783, 0xE203,
        0xA383, 0xA603, 0xAC03, 0xA983, 0xB803, 0xBD83, 0xB783, 0xB203,
        0x9003, 0x9583, 0x9F83, 0x9A03, 0x8B83, 0x8E03, 0x8403, 0x8183,
        0x8002, 0x8582, 0x8F82, 0x8A02, 0x9B82, 0x9E02, 0x9402, 0x9182,
        0xB382, 0xB602, 0xBC02, 0xB982, 0xA802, 0xAD82, 0xA782, 0xA202,
        0xE382, 0xE602, 0xEC02, 0xE982, 0xF802, 0xFD82, 0xF782, 0xF202,
        0xD002, 0xD582, 0xDF82, 0xDA02, 0xCB82, 0xCE02, 0xC402, 0xC182,
        0x4382, 0x4602, 0x4C02, 0x4982, 0x5802, 0x5D82, 0x5782, 0x5202,
        0x7002, 0x7582, 0x7F82, 0x7A02, 0x6B82, 0x6E02, 0x6402, 0x6182,
        0x2002, 0x2582, 0x2F82, 0x2A02, 0x3B82, 0x3E02, 0x3402, 0x3182,
        0x1382, 0x1602, 0x1C02, 0x1982, 0x0802, 0x0D82, 0x0782, 0x0202,
        0x0001
    },
    [AV_CRC_16_CCITT] = {
        0x0000, 0x2110, 0x4220, 0x6330, 0x8440, 0xA550, 0xC660, 0xE770,
        0x0881, 0x2991, 0x4AA1, 0x6BB1, 0x8CC1, 0xADD1, 0xCEE1, 0xEFF1,
        0x3112, 0x1002, 0x7332, 0x5222, 0xB552, 0x9442, 0xF772, 0xD662,
        0x3993, 0x1883, 0x7BB3, 0x5AA3, 0xBDD3, 0x9CC3, 0xFFF3, 0xDEE3,
        0x6224, 0x4334, 0x2004, 0x0114, 0xE664, 0xC774, 0xA444, 0x8554,
        0x6AA5, 0x4BB5, 0x2885, 0x0995, 0xEEE5, 0xCFF5, 0xACC5, 0x8DD5,
        0x5336, 0x7226, 0x1116, 0x3006, 0xD776, 0xF666, 0x9556, 0xB446,
        0x5BB7, 0x7AA7, 0x1997, 0x3887, 0xDFF7, 0xFEE7, 0x9DD7, 0xBCC7,
        0xC448, 0xE558, 0x8668, 0xA778, 0x4008, 0x6118, 0x0228, 0x2338,
        0xCCC9, 0xEDD9, 0x8EE9, 0xAFF9, 0x4889, 0x6999, 0x0AA9, 0x2BB9,
        0xF55A, 0xD44A, 0xB77A, 0x966A, 0x711A, 0x500A, 0x333A, 0x122A,
        0xFDDB, 0xDCCB, 0xBFFB, 0x9EEB, 0x799B, 0x588B, 0x3BBB, 0x1AAB,
        0xA66C, 0x877C, 0xE44C, 0xC55C, 0x222C, 0x033C, 0x600C, 0x411C,
        0xAEED, 0x8FFD, 0xECCD, 0xCDDD, 0x2AAD, 0x0BBD, 0x688D, 0x499D,
        0x977E, 0xB66E, 0xD55E, 0xF44E, 0x133E, 0x322E, 0x511E, 0x700E,
        0x9FFF, 0xBEEF, 0xDDDF, 0xFCCF, 0x1BBF, 0x3AAF, 0x599F, 0x788F,
        0x8891, 0xA981, 0xCAB1, 0xEBA1, 0x0CD1, 0x2DC1, 0x4EF1, 0x6FE1,
        0x8010, 0xA100, 0xC230, 0xE320, 0x0450, 0x2540, 0x4670, 0x6760,
        0xB983, 0x9893, 0xFBA3, 0xDAB3, 0x3DC3, 0x1CD3, 0x7FE3, 0x5EF3,
        0xB102, 0x9012, 0xF322, 0xD232, 0x3542, 0x1452, 0x7762, 0x5672,
        0xEAB5, 0xCBA5, 0xA895, 0x8985, 0x6EF5, 0x4FE5, 0x2CD5, 0x0DC5,
        0xE234, 0xC324, 0xA014, 0x8104, 0x6674, 0x4764, 0x2454, 0x0544,
        0xDBA7, 0xFAB7, 0x9987, 0xB897, 0x5FE7, 0x7EF7, 0x1DC7, 0x3CD7,
        0xD326, 0xF236, 0x9106, 0xB016, 0x5766, 0x7676, 0x1546, 0x3456,
        0x4CD9, 0x6DC9, 0x0EF9, 0x2FE9, 0xC899, 0xE989, 0x8AB9, 0xABA9,
        0x4458, 0x6548, 0x0678, 0x2768, 0xC018, 0xE108, 0x8238, 0xA328,
        0x7DCB, 0x5CDB, 0x3FEB, 0x1EFB, 0xF98B, 0xD89B, 0xBBAB, 0x9ABB,
        0x754A, 0x545A, 0x376A, 0x167A, 0xF10A, 0xD01A, 0xB32A, 0x923A,
        0x2EFD, 0x0FED, 0x6CDD, 0x4DCD, 0xAABD, 0x8BAD, 0xE89D, 0xC98D,
        0x267C, 0x076C, 0x645C, 0x454C, 0xA23C, 0x832C, 0xE01C, 0xC10C,
        0x1FEF, 0x3EFF, 0x5DCF, 0x7CDF, 0x9BAF, 0xBABF, 0xD98F, 0xF89F,
        0x176E, 0x367E, 0x554E, 0x745E, 0x932E, 0xB23E, 0xD10E, 0xF01E,
        0x0001
    },
    [AV_CRC_32_IEEE] = {
        0x00000000, 0xB71DC104, 0x6E3B8209, 0xD926430D, 0xDC760413, 0x6B6BC517,
        0xB24D861A, 0x0550471E, 0xB8ED0826, 0x0FF0C922, 0xD6D68A2F, 0x61CB4B2B,
        0x649B0C35, 0xD386CD31, 0x0AA08E3C, 0xBDBD4F38, 0x70DB114C, 0xC7C6D048,
        0x1EE09345, 0xA9FD5241, 0xACAD155F, 0x1BB0D45B, 0xC2969756, 0x758B5652,
        0xC836196A, 0x7F2BD86E, 0xA60D9B63, 0x11105A67, 0x14401D79, 0xA35DDC7D,
        0x7A7B9F70, 0xCD665E74, 0xE0B62398, 0x57ABE29C, 0x8E8DA191, 0x39906095,
        0x3CC0278B, 0x8BDDE68F, 0x52FBA582, 0xE5E66486, 0x585B2BBE, 0xEF46EABA,
        0x3660A9B7, 0x817D68B3, 0x842D2FAD, 0x3330EEA9, 0xEA16ADA4, 0x5D0B6CA0,
        0x906D32D4, 0x2770F3D0, 0xFE56B0DD, 0x494B71D9, 0x4C1B36C7, 0xFB06F7C3,
        0x2220B4CE, 0x953D75CA, 0x28803AF2, 0x9F9DFBF6, 0x46BBB8FB, 0xF1A679FF,
        0xF4F63EE1, 0x43EBFFE5, 0x9ACDBCE8, 0x2DD07DEC, 0x77708634, 0xC06D4730,
        0x194B043D, 0xAE56C539, 0xAB068227, 0x1C1B4323, 0xC53D002E, 0x7220C12A,
        0xCF9D8E12, 0x78804F16, 0xA1A60C1B, 0x16BBCD1F, 0x13EB8A01, 0xA4F64B05,
        0x7DD00808, 0xCACDC90C, 0x07AB9778, 0xB0B6567C, 0x69901571, 0xDE8DD475,
        0xDBDD936B, 0x6CC0526F, 0xB5E61162, 0x02FBD066, 0xBF469F5E, 0x085B5E5A,
        0xD17D1D57, 0x6660DC53, 0x63309B4D, 0xD42D5A49, 0x0D0B1944, 0xBA16D840,
        0x97C6A5AC, 0x20DB64A8, 0xF9FD27A5, 0x4EE0E6A1, 0x4BB0A1BF, 0xFCAD60BB,
        0x258B23B6, 0x9296E2B2, 0x2F2BAD8A, 0x98366C8E, 0x41102F83, 0xF60DEE87,
        0xF35DA999, 0x4440689D, 0x9D662B90, 0x2A7BEA94, 0xE71DB4E0, 0x500075E4,
        0x892636E9, 0x3E3BF7ED, 0x3B6BB0F3, 0x8C7671F7, 0x555032FA, 0xE24DF3FE,
        0x5FF0BCC6, 0xE8ED7DC2, 0x31CB3ECF, 0x86D6FFCB, 0x8386B8D5, 0x349B79D1,
        0xEDBD3ADC, 0x5AA0FBD8, 0xEEE00C69, 0x59FDCD6D, 0x80DB8E60, 0x37C64F64,
        0x3296087A, 0x858BC97E, 0x5CAD8A73, 0xEBB04B77, 0x560D044F, 0xE110C54B,
        0x38368646, 0x8F2B4742, 0x8A7B005C, 0x3D66C158, 0xE4408255, 0x535D4351,
        0x9E3B1D25, 0x2926DC21, 0xF0009F2C, 0x471D5E28, 0x424D1936, 0xF550D832,
        0x2C769B3F, 0x9B6B5A3B, 0x26D61503, 0x91CBD407, 0x48ED970A, 0xFFF0560E,
        0xFAA01110, 0x4DBDD014, 0x949B9319, 0x2386521D, 0x0E562FF1, 0xB94BEEF5,
        0x606DADF8, 0xD7706CFC, 0xD2202BE2, 0x653DEAE6, 0xBC1BA9EB, 0x0B0668EF,
        0xB6BB27D7, 0x01A6E6D3, 0xD880A5DE, 0x6F9D64DA, 0x6ACD23C4, 0xDDD0E2C0,
        0x04F6A1CD, 0xB3EB60C9, 0x7E8D3EBD, 0xC990FFB9, 0x10B6BCB4, 0xA7AB7DB0,
        0xA2FB3AAE, 0x15E6FBAA, 0xCCC0B8A7, 0x7BDD79A3, 0xC660369B, 0x717DF79F,
        0xA85BB492, 0x1F467596, 0x1A163288, 0xAD0BF38C, 0x742DB081, 0xC3307185,
        0x99908A5D, 0x2E8D4B59, 0xF7AB0854, 0x40B6C950, 0x45E68E4E, 0xF2FB4F4A,
        0x2BDD0C47, 0x9CC0CD43, 0x217D827B, 0x9660437F, 0x4F460072, 0xF85BC176,
        0xFD0B8668, 0x4A16476C, 0x93300461, 0x242DC565, 0xE94B9B11, 0x5E565A15,
        0x87701918, 0x306DD81C, 0x353D9F02, 0x82205E06, 0x5B061D0B, 0xEC1BDC0F,
        0x51A69337, 0xE6BB5233, 0x3F9D113E, 0x8880D03A, 0x8DD09724, 0x3ACD5620,
        0xE3EB152D, 0x54F6D429, 0x7926A9C5, 0xCE3B68C1, 0x171D2BCC, 0xA000EAC8,
        0xA550ADD6, 0x124D6CD2, 0xCB6B2FDF, 0x7C76EEDB, 0xC1CBA1E3, 0x76D660E7,
        0xAFF023EA, 0x18EDE2EE, 0x1DBDA5F0, 0xAAA064F4, 0x738627F9, 0xC49BE6FD,
        0x09FDB889, 0xBEE0798D, 0x67C63A80, 0xD0DBFB84, 0xD58BBC9A, 0x62967D9E,
        0xBBB03E93, 0x0CADFF97, 0xB110B0AF, 0x060D71AB, 0xDF2B32A6, 0x6836F3A2,
        0x6D66B4BC, 0xDA7B75B8, 0x035D36B5, 0xB440F7B1, 0x00000001
    },
    [AV_CRC_32_IEEE_LE] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D, 0x00000001
    },
};

const unsigned int AudioSampleRate[13] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000,
    11025, 8000, 7350
};

static uint16_t bswap_16(uint16_t x)
{
    x= (x>>8) | (x<<8);
    return x;
}

static uint32_t bswap_32(uint32_t x)
{
    x= ((x<<8)&0xFF00FF00) | ((x>>8)&0x00FF00FF);
    x= (x>>16) | (x<<16);
    return x;
}

static uint64_t bswap_64(uint64_t x)
{
    union
    {
        uint64_t ll;
        uint32_t l[2];
    } w, r;
    w.ll = x;
    r.l[0] = bswap_32 (w.l[1]);
    r.l[1] = bswap_32 (w.l[0]);
    return r.ll;
}

#define be2me_16(x) bswap_16(x)
#define be2me_32(x) bswap_32(x)
#define be2me_64(x) bswap_64(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)


static void put_buffer(FILE *s, uint8_t *buf, int32_t size)
{
    uint32_t size_file;
    size_file = fwrite(buf,1,size,s);
    if((unsigned int)size_file!=(unsigned int)size)
    {
        aloge("write the bytes is error!\n");
    }
}
static void put_buffer_cache(FsWriter *pFsWriter, char *buf, int size)
{
#if 0
    CDX_S64 tm1, tm2, tm3;
    tm1 = CDX_GetNowUs();
#endif
	pFsWriter->fsWrite(pFsWriter, (char*)buf, size);
#if 0
    tm2 = CDX_GetNowUs();
    if((tm2-tm1)/1000 > 40)
    {
        if(mov->tracks[0].enc)
        {
            alogd("[%dx%d]Be careful, forbid to appear this case: fwrite [%d]kByte, [%lld]ms >40ms", 
                __FUNCTION__, __LINE__, mov->tracks[0].enc->width, mov->tracks[0].enc->height, size/1024, (tm2-tm1)/1000);
        }
        else
        {
            alogd("Be careful, forbid to appear this case: fwrite [%d]kByte, [%lld]ms >40ms", 
                __FUNCTION__, __LINE__, size/1024, (tm2-tm1)/1000);
        }
        
    }
#endif
    return;
}
static void flush_payload_cache(FsWriter *pFsWriter)
{
//    CDX_S64 tm1, tm2, tm3;
//    tm1 = CDX_GetNowUs();
    pFsWriter->fsFlush(pFsWriter);
//    tm2 = CDX_GetNowUs();
//    alogd("[%dx%d] flush, [%lld]ms", mov->tracks[0].enc->width, mov->tracks[0].enc->height, (tm2-tm1)/1000);
}



void section_write_packet(MpegTSSection *s, uint8_t *packet)
{
    AVFormatContext *ctx = s->opaque;
    //put_buffer(ctx->pb_cache, packet, TS_PACKET_SIZE);
    put_buffer_cache(ctx->mpFsWriter, (char*)packet, TS_PACKET_SIZE);
    ctx->data_size = ctx->data_size + TS_PACKET_SIZE;
}

static uint32_t av_crc(const uint32_t *ctx, int32_t crc, const uint8_t *buffer, int32_t length)
{

    const uint8_t *end= buffer+length;
	
#if !CONFIG_SMALL
	if(!ctx[256]) {
		while(((intptr_t) buffer & 3) && buffer < end)
			crc = ctx[((uint8_t)crc) ^ *buffer++] ^ (crc >> 8);

		while(buffer<end-3){
			crc ^= le2me_32(*(const uint32_t*)buffer); buffer+=4;
			crc =  ctx[3*256 + ( crc	 &0xFF)]
				  ^ctx[2*256 + ((crc>>8 )&0xFF)]
				  ^ctx[1*256 + ((crc>>16)&0xFF)]
				  ^ctx[0*256 + ((crc>>24)	  )];
		}
	}
#endif

    while(buffer<end)
        crc = ctx[((uint8_t)crc) ^ *buffer++] ^ (crc >> 8);
    return crc;

}


static int av_crc_init(uint32_t *ctx, int le, int bits, uint32_t poly, int ctx_size)
{
    int i, j;
    uint32_t c;

    if (bits < 8 || bits > 32 || poly >= (1LL<<bits))
        return -1;
    if (ctx_size != sizeof(uint32_t)*257 && ctx_size != sizeof(uint32_t)*1024)
        return -1;

    for (i = 0; i < 256; i++) {
        if (le) {
            for (c = i, j = 0; j < 8; j++)
                c = (c>>1)^(poly & (-(c&1)));
            ctx[i] = c;
        } else {
            for (c = i << 24, j = 0; j < 8; j++)
                c = (c<<1) ^ ((poly<<(32-bits)) & (((int32_t)c)>>31) );
            ctx[i] = bswap_32(c);
        }
    }
    ctx[256]=1;
    return 0;
}

static uint32_t *av_crc_get_table(AVCRCId crc_id)
{

#if !CONFIG_HARDCODED_TABLES

		//alogd("shuzu: %d", FF_ARRAY_ELEMS(ts_crc_table[crc_id])-1);
		if (!ts_crc_table[crc_id][FF_ARRAY_ELEMS(ts_crc_table[crc_id])-1]) {
			if (av_crc_init(ts_crc_table[crc_id],
							ts_crc_table_params[crc_id].le,
							ts_crc_table_params[crc_id].bits,
							ts_crc_table_params[crc_id].poly,
							sizeof(ts_crc_table[crc_id])) < 0)
				return NULL;
		}
#endif
    return ts_crc_table[crc_id];
}


static void init_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size)
{
    if(buffer_size < 0)
    {
        buffer_size = 0;
        buffer = NULL;
    }

    s->size_in_bits= 8*buffer_size;
    s->buf = buffer;
    s->buf_end = s->buf + buffer_size;

    s->buf_ptr = s->buf;
    s->bit_left=32;
    s->bit_buf=0;
}

static void put_bits(PutBitContext *s, int n, unsigned int value)
{
    unsigned int bit_buf;
    int bit_left;

    if(!(n <= 31 && value < (1U << n)))
    {
        printf("the n and value is error!\n");
    }

    bit_buf = s->bit_buf;
    bit_left = s->bit_left;

    bit_buf |= value << (32 - bit_left);
    if (n >= bit_left)
    {
        *(uint32_t *)s->buf_ptr = le2me_32(bit_buf);
        s->buf_ptr+=4;
        bit_buf = (bit_left==32)?0:value >> bit_left;
        bit_left+=32;
    }
    bit_left-=n;

    s->bit_buf = bit_buf;
    s->bit_left = bit_left;
}


unsigned long GenerateCRC32(unsigned char * DataBuf,unsigned long  len) 
{
	register unsigned long i;
	unsigned long crc = 0xffffffff;
    static unsigned long crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7, 
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75, 
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef, 
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0, 
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072, 
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba, 
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050, 
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1, 
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53, 
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9, 
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b, 
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71, 
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3, 
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec, 
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676, 
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

	for (i = 0; i < len; i++)
		crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *DataBuf++) & 0xff];
	return crc;
}  


static void mpegts_write_section(MpegTSSection *s, uint8_t *buf, int len)
{
	AVFormatContext* context = (AVFormatContext*)s->opaque;
    MpegTSWrite *ts = ((AVFormatContext*)s->opaque)->priv_data;
    unsigned int crc;
    unsigned char packet[TS_PACKET_SIZE];
    const unsigned char *buf_ptr;
    unsigned char *q;
    int first, b, len1, left;

	crc = GenerateCRC32(buf, len -4);
	
    //crc = bswap_32(av_crc(av_crc_get_table(AV_CRC_32_IEEE), -1, buf, len - 4));
    buf[len - 4] = (crc >> 24) & 0xff;
    buf[len - 3] = (crc >> 16) & 0xff;
    buf[len - 2] = (crc >> 8) & 0xff;
    buf[len - 1] = (crc) & 0xff;

    /* send each packet */
    buf_ptr = buf;
    while (len > 0) {
        first = (buf == buf_ptr);
        q = packet;
        *q++ = 0x47;
        b = (s->pid >> 8);
        if (first)
            b |= 0x40;
        *q++ = b;
        *q++ = s->pid;
        s->cc = (s->cc + 1) & 0xf;
        *q++ = 0x10 | s->cc;//do not write adaption section
        if (first)
            *q++ = 0; /* 0 offset */
        len1 = TS_PACKET_SIZE - (q - packet);
        if (len1 > len)//
            len1 = len;
        memcpy(q, buf_ptr, len1);
        q += len1;
        /* add known padding data */
        left = TS_PACKET_SIZE - (q - packet);
        if (left > 0)
            memset(q, 0xff, left);

		ts->cache_size += TS_PACKET_SIZE;
        ts->cache_size_total += TS_PACKET_SIZE;
		
	    memcpy(ts->ts_write_ptr,packet,TS_PACKET_SIZE);
        ts->ts_write_ptr += TS_PACKET_SIZE;
        if(ts->ts_write_ptr>ts->ts_cache_end)
        {
             ts->ts_write_ptr = ts->ts_cache_start;
        }

		//save bitstream to file if need, when use output bitstream in callback
		//just cache the buffer
		if(context->output_buffer_mode != OUTPUT_CALLBACK_BUFFER) {
		    if(ts->cache_size >= 1024*TS_PACKET_SIZE)
		    {
		        //put_buffer(context->pb_cache, ts->ts_read_ptr, TS_PACKET_SIZE*128);
                put_buffer_cache(context->mpFsWriter,(char*)ts->ts_read_ptr, TS_PACKET_SIZE*128);
		        ts->ts_read_ptr += TS_PACKET_SIZE*128;
		        ts->cache_size = ts->cache_size - TS_PACKET_SIZE*128;
		        ts->cache_page_num ++;
		        if(ts->ts_read_ptr > ts->ts_cache_end)
		        {
		            ts->ts_read_ptr = ts->ts_cache_start;
		        }
		    }
		}

        buf_ptr += len1;
        len -= len1;
    }
}

static void put16(uint8_t **q_ptr, int val)
{
    uint8_t *q;
    q = *q_ptr;
    *q++ = val >> 8;
    *q++ = val;
    *q_ptr = q;
}

static int mpegts_write_section1(MpegTSSection *s, int tid, int id,
                          int version, int sec_num, int last_sec_num,
                          uint8_t *buf, int len)
{
    uint8_t section[1024], *q;
    unsigned int tot_len;

    tot_len = 3 + 5 + len + 4; 
    /* check if not too big */
    if (tot_len > 1024)
        return -1;

    q = section;
    *q++ = tid;//write table_id
    put16(&q, 0xb000 | (len + 5 + 4)); /* 5 byte header + 4 byte CRC */
    put16(&q, id);
    *q++ = 0xc1 | (version << 1); /* current_next_indicator = 1 */
    *q++ = sec_num;
    *q++ = last_sec_num;
    memcpy(q, buf, len);

    mpegts_write_section(s, section, tot_len);
    return 0;
}

static int BSWAP32(unsigned int val)
{
    val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);\
    val= (val>>16) | (val<<16);
	return val;
}

static void put_le32(uint8_t *s, uint32_t val)
{
    uint8_t i;
	for(i=0;i<4;i++)
        *s++ = (uint8_t)(val>>i);
}

static void put_be32(uint8_t *s, uint32_t val)
{
    uint8_t i;
	val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);
    val= (val>>16) | (val<<16);
	for(i=0;i<4;i++)
        *s++ = (__s8)(val>>(i<<3));
}

//add for h264encoder
#define AV_RB32(x)  ((((const uint8_t*)(x))[0] << 24) | \
                     (((const uint8_t*)(x))[1] << 16) | \
                     (((const uint8_t*)(x))[2] <<  8) | \
                      ((const uint8_t*)(x))[3])


static int32_t av_rescale_rnd(int64_t a, int64_t b, int64_t c)
{
    return (a * b + c-1)/c;
}

static uint8_t *ff_find_start_code(uint8_t *  p,uint8_t *end, int32_t *  state)
{
    int i;

    if(p>=end)
        return end;

    for(i=0; i<3; i++){
        uint32_t tmp= *state << 8;
        *state= tmp + *(p++);
        if(tmp == 0x100 || p==end)
            return p;
    }

    while(p<end){
        if     (p[-1] > 1      ) p+= 3;
        else if(p[-2]          ) p+= 2;
        else if(p[-3]|(p[-1]-1)) p++;
        else{
            p++;
            break;
        }
    }

    p= FFMIN(p, end)-4;
    *state= AV_RB32(p);

    return p+4;
}

static void mpegts_write_pat(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    MpegTSService *service;
    uint8_t data[1012], *q;
    int i;

    q = data;
    for(i = 0; i < ts->nb_services; i++) {
        service = ts->services[i];
        put16(&q, service->sid);
        put16(&q, 0xe000 | service->pmt.pid);
    }
    mpegts_write_section1(&ts->pat, PAT_TID, ts->tsid, 1, 0, 0,
                          data, q - data);
}

static void mpegts_write_pmt(AVFormatContext *s, MpegTSService *service)
{
    // MpegTSWrite *ts = s->priv_data;
    uint8_t data[1012], *q, *desc_length_ptr, *program_info_length_ptr;
    int val, stream_type=-1, i;

    q = data;
    put16(&q, 0xe000 | service->pcr_pid);

    program_info_length_ptr = q;
    q += 2; /* patched after */

    /* put program info here */
    val = 0xf000 | (q - program_info_length_ptr - 2);
    program_info_length_ptr[0] = val >> 8;
    program_info_length_ptr[1] = val;

    for(i = 0; i < s->nb_streams; i++)
    {
        AVStream *st = s->streams[i];
        MpegTSWriteStream *ts_st = st->priv_data;
        switch(st->codec.codec_id)
        {
            case CODEC_ID_H264:
                stream_type = STREAM_TYPE_VIDEO_H264;
                break;
            case CODEC_ID_H265:
                stream_type = STREAM_TYPE_VIDEO_HEVC;
                break; 
            case CODEC_ID_AAC:
                stream_type = STREAM_TYPE_AUDIO_AAC;
                break;
			case CODEC_ID_ADPCM:
                stream_type = STREAM_TYPE_AUDIO_LPCM;
                break;
           case CODEC_ID_GGAD: 
                stream_type = STREAM_TYPE_PRIVATE_DATA;
                break;
			default:
				aloge("error type");
        }
        *q++ = stream_type;
        put16(&q, 0xe000 | ts_st->pid);
        desc_length_ptr = q;
		
        q += 2; /* patched after */

		if(stream_type == STREAM_TYPE_AUDIO_LPCM)
		{
			desc_length_ptr[2] = 0x83;
			desc_length_ptr[3] = 0x02;
			desc_length_ptr[4] = 0x46;
			desc_length_ptr[5] = 0x3F;
            q += 4; // add by fangning 2012-12-20 16:55
		}
//		else if(stream_type == STREAM_TYPE_VIDEO_H264)
//		{
//			desc_length_ptr[2] = 0x2A;
//			desc_length_ptr[3] = 0x02;
//			desc_length_ptr[4] = 0x7E;
//			desc_length_ptr[5] = 0x9F;
//		}
//		else if(stream_type == STREAM_TYPE_AUDIO_AAC)
//		{
//			alogd("fixed it later");
//		}

        val = 0xf000 | (q - desc_length_ptr - 2);
        desc_length_ptr[0] = val >> 8;
        desc_length_ptr[1] = val;
    }
    mpegts_write_section1(&service->pmt, PMT_TID, service->sid, 1, 0, 0,
                          data, q - data);
}

static void wirte_pcr_table(AVFormatContext *s, int64_t pts)
{
	//AVFormatContext* context = (AVFormatContext*)s->opaque;
    MpegTSWrite *ts = s->priv_data;
	
	unsigned char buffer[188];
    //pcr 27MHZ, pts unit is 90KHz now(convert in ts_write_packet(), pkt->pts = (pkt->pts - s->base_video_pts) * 90; )
    //The unit of program_clock_reference_base is 90KHz (ISO13818-1, 2.4.3.5).
    // pcr need before pts, we ignore program_clock_reference_extension directly in assignment, buffer[11] = 0.
    long long pcr = pts;
	
	memset(buffer, 0xff, 188);

	buffer[0] = 0x47;
	buffer[1] = 0x50;
	buffer[2] = 0x00; //pcr pid must be 0x1000, used by wifi display
	buffer[3] = 0x20/* | s->pcr_counter ++*/; //wirte adaptation field flag and pcr table counter

    //according to ISO13818-1, 2.4.3.3: The continuity_counter shall not be incremented when the adaptation_field_control of the packet equals '00' or '10'
    //so we can't increase cc. pcr_counter will only be used to record pcr number here.
    s->pcr_counter++;
	//s->pcr_counter &= 0x0f; 
	
	buffer[4] = 0xB7; //wirte adaptation field length
	buffer[5] = 0x10; //set pcr flag

    buffer[6] = pcr >> 25;
    buffer[7] = pcr >> 17;
    buffer[8] = pcr >> 9;
    buffer[9] = pcr >> 1;
    buffer[10] = (pcr & 1) << 7;
    buffer[11] = 0;

	ts->cache_size += TS_PACKET_SIZE;
    ts->cache_size_total += TS_PACKET_SIZE;
	
    memcpy(ts->ts_write_ptr, buffer, TS_PACKET_SIZE);
    ts->ts_write_ptr += TS_PACKET_SIZE;
	
    if(ts->ts_write_ptr>ts->ts_cache_end)
    {
         ts->ts_write_ptr = ts->ts_cache_start;
    }

	//save bitstream to file if need, when use output bitstream in callback
	//just cache the buffer
	if(s->output_buffer_mode != OUTPUT_CALLBACK_BUFFER) {
	    if(ts->cache_size >= 1024*TS_PACKET_SIZE)
	    {
	        //put_buffer(s->pb_cache, ts->ts_read_ptr, TS_PACKET_SIZE*128);
            put_buffer_cache(s->mpFsWriter, (char*)ts->ts_read_ptr, TS_PACKET_SIZE*128);
	        ts->ts_read_ptr += TS_PACKET_SIZE*128;
	        ts->cache_size = ts->cache_size - TS_PACKET_SIZE*128;
	        ts->cache_page_num ++;
	        if(ts->ts_read_ptr > ts->ts_cache_end)
	        {
	            ts->ts_read_ptr = ts->ts_cache_start;
	        }
	    }
	}
}

static void retransmit_si_info(AVFormatContext *s, int64_t pts, int idx)
{
    MpegTSWrite *ts = s->priv_data;
    int i;

    if(s->output_buffer_mode == OUTPUT_CALLBACK_BUFFER) {

		if(s->pat_pmt_counter == 4 || s->pat_pmt_flag == 1) {
			s->pat_pmt_counter = 0;
			s->pat_pmt_flag = 0;
			
			mpegts_write_pat(s);
			for(i = 0; i < ts->nb_services; i++) {
	            mpegts_write_pmt(s, ts->services[i]);
	    	}
		}
//        if (idx == CODEC_TYPE_VIDEO) {
//            wirte_pcr_table(s, pts);
//        }
    } else if (ts->pat_packet_count == ts->pat_packet_period) {
        ts->pat_packet_count = 0;

        mpegts_write_pat(s);
        for(i = 0; i < ts->nb_services; i++) {
            mpegts_write_pmt(s, ts->services[i]);
    	}
//        wirte_pcr_table(s, pts);
    }
}

static void write_pts(uint8_t *q, int fourbits, int64_t pts)
{
    int val;

    val = fourbits << 4 | (((pts >> 30) & 0x07) << 1) | 1;
    *q++ = val;
    val = (((pts >> 15) & 0x7fff) << 1) | 1;
    *q++ = val >> 8;
    *q++ = val;
    val = (((pts) & 0x7fff) << 1) | 1;
    *q++ = val >> 8;
    *q++ = val;
}

/* Add a pes header to the front of payload, and segment into an integer number of
 * ts packets. The final ts packet is padded using an over-sized adaptation header
 * to exactly fill the last ts packet.
 * NOTE: 'payload' contains a complete PES payload.
 */

static void mpegts_write_pes(AVFormatContext *s, AVStream *st,
                             const uint8_t *payload, int payload_size,
                             int64_t pts, int64_t dts, int key, int32_t idx)
{
    MpegTSWriteStream *ts_st = st->priv_data;
    MpegTSWrite *ts = s->priv_data;
    uint8_t buf[TS_PACKET_SIZE];
    uint8_t *q;
    int val, is_start, len, header_len, write_pcr, private_code, flags;
    int afc_len, stuffing_len;
    int64_t pcr = -1; /* avoid warning */
    //int64_t delay = av_rescale_rnd(s->max_delay, 90000, AV_TIME_BASE);//1.09
    int stuffingbypes;

    is_start = 1;
    //ts_st->cc = 0;          // fix bug that the continutiy counter is not correct

    if(0 == idx)    //write pcr according to only one stream. It is very important!
    {
        retransmit_si_info(s, pts, idx);
        if(CODEC_TYPE_VIDEO == st->codec.codec_type)
        {
            int write_pcr = 0;
            if(0 == ts->pat_packet_count)
            {
                write_pcr = 1;
            }
            if(key)
            {
                write_pcr = 1;
            }
            if(write_pcr)
            {
                wirte_pcr_table(s, pts);
            }
        }
        else
        {
            if(0 == ts->pat_packet_count)
            {
                wirte_pcr_table(s, pts);
            }
        }
        ts->pat_packet_count++;
    }

    while (payload_size > 0)
    {
        write_pcr = 0;

#if 0 //do not write pcr currently
        if (ts_st->pid == ts_st->service->pcr_pid)
        {
        
			//alogd("ts_st->service->pcr_pid: %d\n", ts_st->service->pcr_pid);
			ts_st->service->pcr_packet_count++;
            if (ts_st->service->pcr_packet_count >=
                ts_st->service->pcr_packet_period) {

				//alogd("ts_st->service->pcr_packet_count: %d", ts_st->service->pcr_packet_count);
                ts_st->service->pcr_packet_count = 0;
                write_pcr = 1;
                /* XXX: this is incorrect, but at least we have a PCR
                   value */
                pcr = pts;
            }
        }
		write_pcr = 0;
#endif
        /* prepare packet header */
        q = buf;
        *q++ = 0x47;
        val = (ts_st->pid >> 8);
        if (is_start)
            val |= 0x40;
        *q++ = val;
        *q++ = ts_st->pid;
        ts_st->cc = (ts_st->cc + 1) & 0xf;
        *q++ = 0x10 | ts_st->cc | (write_pcr ? 0x20 : 0);
        if (write_pcr)
        {
            *q++ = 7; /* AFC length */
            *q++ = 0x10; /* flags: PCR present */
            *q++ = pcr >> 25;
            *q++ = pcr >> 17;
            *q++ = pcr >> 9;
            *q++ = pcr >> 1;
            *q++ = (pcr & 1) << 7;
            *q++ = 0;
        }
        if (is_start)
        {
            /* write PES header */
            *q++ = 0x00;
            *q++ = 0x00;
            *q++ = 0x01;
            private_code = 0;
            if(st->codec.codec_id == CODEC_ID_H264 || (st->codec.codec_id == CODEC_ID_H265)) 
            {
                *q++ = 0xe0; //for h264
            }
//            else if(st->codec.codec_id == CODEC_ID_H265) 
//            {
//                *q++ = 0xfd; //for h264
//            } 
			else if(st->codec.codec_id == CODEC_ID_ADPCM)
			{
				*q++ = 0xBD; //for LPCM
			}
            else if(st->codec.codec_id == CODEC_ID_AAC)
            {
                *q++ = 0xc0; //for aac
            }
            else if(st->codec.codec_id == CODEC_ID_GGAD)
            {
                *q++ = 0xBF; //for gps private data
                pts = -1;   // no optional pes header;
            }

            if(st->codec.codec_id != CODEC_ID_GGAD)
            {
                header_len = 0;     // pes_header_data_length;
                flags = 0;

                
                stuffingbypes = 2;
                if (pts != -1)
                {
                    header_len += 5;    // For pts

                    header_len += stuffingbypes;
                    flags |= 0x80;       // PTS exist
                }

#if 0
                if(st->codec.codec_id == CODEC_ID_H265)
                {
                    /* set PES_extension_flag */ 
                    pes_extension = 1;
                    flags        |= 0x01;
                    /* One byte for PES2 extension flag +
                     * one byte for extension length +
                     * one byte for extension id */
                    header_len += 3;
                }
#endif /* 0 */
                
                len = payload_size + header_len + 3;    //3B pes_header
                if (private_code != 0)
                    len++;
                if (len > 0xffff)
                    len = 0;
                *q++ = len >> 8;        // pes_packet_length
                *q++ = len;
               // val = 0x84;
                val = 0x80;
                
                /* data alignment indicator is required for subtitle data */
                if (st->codec.codec_type == CODEC_TYPE_SUBTITLE)
                    val |= 0x04;
                *q++ = val;
                *q++ = flags;
                *q++ = header_len;
                if (pts != -1)
                {
                    write_pts(q, flags >> 6, pts);
                    q += 5;
                }

#if 0
                if(pes_extension && (st->codec.codec_id == CODEC_ID_H265))
                { 
                    flags = 0x01;  /* set PES_extension_flag_2 */
                    *q++  = flags;
                    *q++  = 0x80 | 0x01; /* marker bit + extension length */
                    /* Set the stream ID extension flag bit to 0 and
                     * write the extended stream ID. */
                    *q++ = 0x00 | 0x60;
                }
#endif /* 0 */

                //stuffing bytes
                *q++ = 0xFF;
                *q++ = 0xFF;
                
                if (private_code != 0)
                    *q++ = private_code;

            }
            else if(st->codec.codec_id == CODEC_ID_GGAD)
            {
                len = payload_size;
                *q++ = len >> 8;        // pes_packet_length
                *q++ = len;
            }
            is_start = 0;
        }
        /* header size */
        header_len = q - buf;
        /* data len */
        len = TS_PACKET_SIZE - header_len;
        if (len > payload_size)
            len = payload_size;
        stuffing_len = TS_PACKET_SIZE - header_len - len;
        if (stuffing_len > 0)
        {
            /* add stuffing with AFC */
            if (buf[3] & 0x20)
            {
                /* stuffing already present: increase its size */
                afc_len = buf[4] + 1;//afc_len
                //memmove = memcpy(s,d,n);
                memmove(buf + 4 + afc_len + stuffing_len,buf + 4 + afc_len,header_len - (4 + afc_len));
                buf[4] += stuffing_len;
                memset(buf + 4 + afc_len, 0xff, stuffing_len);
            }
            else
            {
                /* add stuffing */
                memmove(buf + 4 + stuffing_len, buf + 4, header_len - 4);
                buf[3] |= 0x20;
                buf[4] = stuffing_len - 1;
                if (stuffing_len >= 2)
                {
                    buf[5] = 0x00;
                    memset(buf + 6, 0xff, stuffing_len - 2);
                }
            }
        }
        memcpy(buf + TS_PACKET_SIZE - len, payload, len);
        payload += len;
        payload_size -= len;
        ts->cache_size += TS_PACKET_SIZE;
        ts->cache_size_total += TS_PACKET_SIZE;

		
        memcpy(ts->ts_write_ptr,buf,TS_PACKET_SIZE);
        ts->ts_write_ptr += TS_PACKET_SIZE;
        if(ts->ts_write_ptr>ts->ts_cache_end)
        {
             ts->ts_write_ptr = ts->ts_cache_start;
        }

		//save output bitstream to file
		if(s->output_buffer_mode != OUTPUT_CALLBACK_BUFFER) {
			if(ts->cache_size >= 1024*TS_PACKET_SIZE)
			{
			    //put_buffer(s->pb_cache, ts->ts_read_ptr, TS_PACKET_SIZE*128);
			    put_buffer_cache(s->mpFsWriter, (char*)ts->ts_read_ptr, TS_PACKET_SIZE*128);
			    ts->ts_read_ptr += TS_PACKET_SIZE*128;
			    ts->cache_size = ts->cache_size - TS_PACKET_SIZE*128;
			    ts->cache_page_num ++;
			    if(ts->ts_read_ptr > ts->ts_cache_end)
			    {
			        ts->ts_read_ptr = ts->ts_cache_start;
			    }
			}
		}
    }

	if(s->output_buffer_mode == OUTPUT_CALLBACK_BUFFER) {
#if 1
        CDXRecorderBsInfo bs_info;
        TSPacketHeader *pkt_hdr = &s->PacketHdr;
        int ret;

        pkt_hdr->stream_type = st->codec.codec_type == CODEC_TYPE_VIDEO ? 0 : 1; //StreamIndex
        if( st->codec.codec_type == CODEC_TYPE_VIDEO)
        {
             pkt_hdr->stream_type = 0;
        }
        else if( st->codec.codec_type == CODEC_TYPE_AUDIO)
        {
             pkt_hdr->stream_type = 1;
        }
        else if( st->codec.codec_type == CODEC_TYPE_DATA)
        {
             pkt_hdr->stream_type = 2;
        }
        pkt_hdr->size = ts->cache_size;
        pkt_hdr->pts = -1;//do not use this pts;

        bs_info.total_size = 0;
        bs_info.bs_count = 1;
        bs_info.bs_data[0] = (char *)pkt_hdr;
        bs_info.bs_size[0] = sizeof(TSPacketHeader);
        bs_info.total_size += bs_info.bs_size[0];

        bs_info.bs_count++;
        bs_info.bs_data[1] = (char *)ts->ts_read_ptr;
        bs_info.bs_size[1] = ts->cache_size;
        bs_info.total_size += bs_info.bs_size[1];

        bs_info.mode = 1;

        ret = cdx_write2(&bs_info, s->OutStreamHandle);
        if(ret < 0) {
            aloge("mpeg2ts write error");
        }
#else
        cdx_write(ts->ts_read_ptr, 1, ts->cache_size, s->OutStreamHandle);
#endif

		//reset cache offset
		ts->ts_write_ptr = ts->ts_read_ptr = ts->ts_cache_start;
		ts->cache_size = 0;
		ts->cache_page_num = 0;
		ts->cache_size_total = 0;
	}
}

int32_t ts_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    int size = 0;
	uint8_t *buf_tmp;
	int ret;
    int real_strm_idx = pkt->stream_index;

    if(!s->v_strm_flag || !s->a_strm_flag)      // process condition that no video stream exist or no audio stream exist
    {
        if(CODEC_TYPE_TEXT == pkt->stream_index)
        {
            real_strm_idx = pkt->stream_index - 1;
        }
        else if(CODEC_TYPE_AUDIO == pkt->stream_index)
        {
            real_strm_idx = pkt->stream_index - 1;
        } 
    } 
    
    AVStream *st = s->streams[real_strm_idx];
	MpegTSWrite *ts = s->priv_data;
	buf_tmp = s->pes_buffer;

	if(st->codec.codec_id == CODEC_ID_H264) {
		int state = 0;
        int total_size = 0;
		s->pat_pmt_counter++;

		if(s->output_buffer_mode == OUTPUT_M3U8_FILE) {
			if(pkt->flags == 1) {
				s->keyframe_num++;
			}

			if(s->keyframe_num >= MAX_KEY_FRAME_NUM) {
				s->keyframe_num = 0;

				ret = ts_write_trailer(s);
                if(ret != 0)
                {
                    aloge("fatal error! ts write trailer fail!");
                }
				
				//close file
				FSWRITEMODE mode = s->mpFsWriter->mMode;
				destroyFsWriter(s->mpFsWriter);
                s->mpFsWriter = NULL;
                destroy_outstream_handle(s->pb_cache);
                //fclose(s->pb_cache);
				s->pb_cache = NULL;
				st->firstframeflag = 1;
				ts->pat_packet_count = ts->pat_packet_period;

				s->current_segment++;
				sprintf((char*)s->filename, "%s%d.ts", TS_FILE_SAVED_PATH, s->current_segment);

                CedarXDataSourceDesc datasourceDesc;
                memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
                datasourceDesc.ext_fd_desc.fd = open(s->filename, O_RDWR | O_CREAT, 0666);
                datasourceDesc.source_type = CEDARX_SOURCE_FD;
                datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
                s->pb_cache = create_outstream_handle(&datasourceDesc);
                close(datasourceDesc.ext_fd_desc.fd);
                datasourceDesc.ext_fd_desc.fd = -1;
				//s->pb_cache = fopen(s->filename, "wb");
				if(s->pb_cache == NULL) {
					aloge("pRecRenderData->fd == NULL" );
				}
                char *pCache = NULL;
                uint32_t nCacheSize = 0;
                if(FSWRITEMODE_CACHETHREAD == mode)
                {
            		if (s->mCacheMemInfo.mCacheSize > 0 && s->mCacheMemInfo.mpCache != NULL)
            		{
            			mode = FSWRITEMODE_CACHETHREAD;
                        pCache = s->mCacheMemInfo.mpCache;
                        nCacheSize = s->mCacheMemInfo.mCacheSize;
            		}
            		else
            		{
                        aloge("fatal error! not set cacheMemory but set mode FSWRITEMODE_CACHETHREAD! use FSWRITEMODE_DIRECT.");
                        mode = FSWRITEMODE_DIRECT;
            		}
                }
                else if(FSWRITEMODE_SIMPLECACHE == mode)
                {
                    pCache = NULL;
                    nCacheSize = s->mFsSimpleCacheSize;
                }
                s->mpFsWriter = createFsWriter(mode, s->pb_cache, pCache, nCacheSize, s->streams[0]->codec.codec_id);

				if(s->current_segment >= MAX_SEGMENT_IN_M3U8) {
					unsigned int tem_segment;
					char m3u8_writebuf[1024];
					int i;
					
					sprintf((char*)s->filename, "%s.m3u8", M3U8_FILE_SAVED_PATH);
                    CedarXDataSourceDesc datasourceDesc;
                    memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
                    datasourceDesc.ext_fd_desc.fd = open(s->filename, O_RDWR | O_CREAT, 0666);
                    datasourceDesc.source_type = CEDARX_SOURCE_FD;
                    datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
                    s->fd_m3u8 = create_outstream_handle(&datasourceDesc);
                    close(datasourceDesc.ext_fd_desc.fd);
                    datasourceDesc.ext_fd_desc.fd = -1;
					//s->fd_m3u8 = fopen(s->filename, "wb");

					if(s->fd_m3u8 == NULL) {
						aloge("pRecRenderData->fd_m3u8 == NULL" );
					} else {
						alogv("save m3u8 ok, path: %s", s->filename);
					}
					snprintf(m3u8_writebuf, 1024, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n#EXT-X-MEDIA-SEQUENCE:%u\n", MAX_KEY_FRAME_NUM, s->first_segment);	
                    if (cdx_write(m3u8_writebuf, strlen(m3u8_writebuf), 1, s->fd_m3u8) != 1) {
							//fclose(pRecRenderData->fd_m3u8);
							aloge("write m3u8 file failed");
					}

					tem_segment = s->first_segment;

					for (i = 0; i < MAX_SEGMENT_IN_M3U8; i++) {
						snprintf(m3u8_writebuf, 1024, "#EXTINF:%u,\n%u.ts\n", MAX_KEY_FRAME_NUM, tem_segment);
						if (cdx_write(m3u8_writebuf, strlen(m3u8_writebuf), 1, s->fd_m3u8) != 1) {

							aloge("write m3u8 file failed");
						}
						tem_segment ++;
					}

                    destroy_outstream_handle(s->fd_m3u8);
					//fclose(s->fd_m3u8);
					s->fd_m3u8 = NULL;

					if((int)s->first_segment - 3 >= 0) {
						sprintf((char*)s->filename, "%s%u.ts", TS_FILE_SAVED_PATH, s->first_segment - 3);
						remove(s->filename);
					}
					s->first_segment ++;
				}
			}	
		}


        if ((state & 0x1f) != 9)
        { 
        	uint8_t data[6];
			
        	// AUD NAL// state & 0x1f = 1
            put_be32(data, 0x00000001);
            data[4] = 0x09;
			
            //data[5] = 0xe0; // any slice type
		    data[5] = 0x30;
			
			memcpy(buf_tmp, data, 6);
			buf_tmp += 6;
        }

		if(st->firstframeflag) {
			memcpy(buf_tmp, st->vosData, st->vosLen);
			buf_tmp +=st->vosLen;
			st->firstframeflag = 0;
		} 
        else if(/*s->output_buffer_mode == OUTPUT_CALLBACK_BUFFER &&*/ pkt->flags == 1) {
			memcpy(buf_tmp, st->vosData, st->vosLen);
			buf_tmp +=st->vosLen;
			st->firstframeflag = 0;
		}

        total_size = buf_tmp - s->pes_buffer + pkt->size0 + pkt->size1;
        if (s->pes_bufsize < total_size) {
            int offset = buf_tmp - s->pes_buffer;
            alogd("realloc pes_buffer [%d]->[%d]", s->pes_bufsize, total_size+32*1024);

            uint8_t *tmp_ptr = NULL;
            tmp_ptr = (uint8_t*)realloc(s->pes_buffer, total_size+32*1024);
            if (tmp_ptr == NULL) {
                aloge("realloc pes_buffer error!!");
                return 0;
            }
            s->pes_buffer = tmp_ptr;
            s->pes_bufsize = total_size+32*1024;
            buf_tmp = s->pes_buffer + offset;
        }

		if(pkt->size0) {
			memcpy(buf_tmp, pkt->data0, pkt->size0);
			buf_tmp += pkt->size0;
		}

		if(pkt->size1) {
			memcpy(buf_tmp, pkt->data1, pkt->size1);
			buf_tmp += pkt->size1;
		}
		
		size = buf_tmp - s->pes_buffer;

		if(s->first_video_pts == 1) {
			s->first_video_pts = 0;
			s->base_video_pts = pkt->pts;
			alogw("video_base_pts: %lld-%d", s->base_video_pts,pkt->flags);
		}

        s->Video_frame_number ++;
		//alogd("pkt->pts - s->base_video_pts: %lld", pkt->pts - s->base_video_pts);
		pkt->pts = (pkt->pts - s->base_video_pts) * 90;
	} 
    else if(st->codec.codec_id == CODEC_ID_H265)
    {		
        int state = 0;
        int total_size = 0;
		s->pat_pmt_counter++;
        
        if((state & 0x1F) != 9)
        { 
        	uint8_t data[7]; 
            
            put_be32(data, 0x00000001); 
            data[4] = 2*35;
            data[5] = 1;
            data[6] = 0x50; // any slice type (0x4) + rbsp stop one bit
            
			memcpy(buf_tmp, data, 7);
			buf_tmp += 7;
        }
		if(st->firstframeflag || *((char*)pkt->data0+4) == 0x26) {
			memcpy(buf_tmp, st->vosData, st->vosLen);
			buf_tmp +=st->vosLen;
			st->firstframeflag = 0;
		} 
//        else if(/*s->output_buffer_mode == OUTPUT_CALLBACK_BUFFER &&*/ pkt->flags == 1) {
//			memcpy(buf_tmp, st->vosData, st->vosLen);
//			buf_tmp +=st->vosLen;
//			st->firstframeflag = 0;
//		}

        total_size = buf_tmp - s->pes_buffer + pkt->size0 + pkt->size1;
        if (s->pes_bufsize < total_size) {
            int offset = buf_tmp - s->pes_buffer;
            alogd("realloc pes_buffer [%d]->[%d]", s->pes_bufsize, total_size+32*1024);

            uint8_t *tmp_ptr = NULL;
            
             tmp_ptr = (uint8_t*)realloc(s->pes_buffer, total_size+32*1024);
            if (tmp_ptr == NULL) {
                aloge("realloc pes_buffer error!!");
                return 0;
            }
            s->pes_buffer = tmp_ptr;
            s->pes_bufsize = total_size+32*1024;
            buf_tmp = s->pes_buffer + offset;
        }

		if(pkt->size0) {
			memcpy(buf_tmp, pkt->data0, pkt->size0);
			buf_tmp += pkt->size0;
		}

		if(pkt->size1) {
			memcpy(buf_tmp, pkt->data1, pkt->size1);
			buf_tmp += pkt->size1;
		}
		
		size = buf_tmp - s->pes_buffer;

		if(s->first_video_pts == 1) {
			s->first_video_pts = 0;
			s->base_video_pts = pkt->pts;
			alogw("video_base_pts: %lld-%d", s->base_video_pts,pkt->flags);
		}

		//alogd("pkt->pts - s->base_video_pts: %lld", pkt->pts - s->base_video_pts);
		s->Video_frame_number ++;
		pkt->pts = (pkt->pts - s->base_video_pts) * 90; 
        
    }
    else if (st->codec.codec_id == CODEC_ID_AAC) {
		unsigned int channels;
	    unsigned int frame_length;
	    unsigned int obj_type;
		unsigned int num_data_block;
		uint8_t adts_header[ADTS_HEADER_LENGTH];
		int i;
        int total_size = ADTS_HEADER_LENGTH + pkt->size0 + pkt->size1;

        if (s->pes_bufsize < total_size) {
            alogd("realloc pes_buffer [%d]->[%d]", s->pes_bufsize, total_size+32*1024);

            uint8_t *tmp_ptr = NULL;
            tmp_ptr = (uint8_t*)realloc(s->pes_buffer, total_size+32*1024);
            if (tmp_ptr == NULL) {
                aloge("realloc pes_buffer error!!");
                return 0;
            }

            s->pes_buffer  = tmp_ptr;
            s->pes_bufsize = total_size+32*1024;
            buf_tmp = s->pes_buffer;
        }

		obj_type = AAC_PROFILE_LC; //AACLC
	    channels = st->codec.channels;
        if(pkt->size1!=0 || pkt->data1 != NULL)
        {
            alogd("Be careful! audioPkt has two data buf: [%p][%d],[%p][%d]", pkt->data0, pkt->size0, pkt->data1, pkt->size1);
        }
	    frame_length = pkt->size0 + pkt->size1 + ADTS_HEADER_LENGTH;
	    num_data_block = frame_length / 1024;
		
		for(i=0;i<13;i++)
	    {
	        if((unsigned int)st->codec.sample_rate == AudioSampleRate[i])
	        	break;
	    }
#if 1
	    adts_header[0] = 0xFF;
	    //adts_header[1] = 0xF9; //mpeg-4 aac
		adts_header[1] = 0xF1;  //mpeg-2 aac
		
	    adts_header[2] = obj_type << 6;
	    adts_header[2] |= (i << 2);
	    adts_header[2] |= (channels & 0x4) >> 2;
	    adts_header[3] = (channels & 0x3) << 6;
		
	    //adts_header[3] |= (frame_length & 0x1800) >> 15;
		adts_header[3] |= (frame_length & 0x1800) >> 11;
	    adts_header[4] = (frame_length & 0x1FF8) >> 3;
	    adts_header[5] = (frame_length & 0x7) << 5;
	    adts_header[5] |= 0x1F;
		
	    adts_header[6] = 0xFC;// one raw data blocks .
	    adts_header[6] |= num_data_block & 0x03; //Set raw Data blocks.
		
#else
		adts_header[0] = 0xff;
		adts_header[1] = 0xf1;	// b11110001, ID=0, layer=0, protection_absent=1
		adts_header[2] =
			obj_type << 6
			| i << 2
			| ((channels >> 2) & 1);  // private_bit=0

		// original_copy=0, home=0, copyright_id_bit=0, copyright_id_start=0
		adts_header[3] = (channels & 3) << 6 | frame_length >> 11;
		adts_header[4] = (frame_length >> 3) & 0xff;
		adts_header[5] = (frame_length & 7) << 5;

		// adts_buffer_fullness=0, number_of_raw_data_blocks_in_frame=0
		adts_header[6] = 0;

#endif
		memcpy(buf_tmp, adts_header, ADTS_HEADER_LENGTH);

		buf_tmp += ADTS_HEADER_LENGTH;
		memcpy(buf_tmp, pkt->data0, pkt->size0);
        if(pkt->size1>0)
        {
            memcpy(buf_tmp+pkt->size0, pkt->data1, pkt->size1);
        }
        
		if(s->first_audio_pts == 1)
		{
            s->first_audio_pts = 0;
            s->base_audio_pts = pkt->pts;

            aloge("ts_audio_base_pts:%lld",s->base_audio_pts);
		}
        pkt->pts = (pkt->pts - s->base_audio_pts)*90;
//		pkt->pts = s->audio_frame_num*90000*1024/st->codec.sample_rate; //(MAXDECODESAMPLE = 1024, so 1024)
		s->audio_frame_num ++;
		size = frame_length;
	}
	else if(st->codec.codec_id == CODEC_ID_ADPCM)
	{
        int total_size = pkt->size0;

        if (s->pes_bufsize < total_size) {
            alogd("realloc pes_buffer [%d]->[%d]", s->pes_bufsize, total_size+32*1024);

            uint8_t *tmp_ptr = NULL;
            tmp_ptr = (uint8_t*)realloc(s->pes_buffer, total_size+32*1024);
            if (tmp_ptr == NULL) {
                aloge("realloc pes_buffer error!!");
                return 0;
            }
            s->pes_buffer = tmp_ptr;
            s->pes_bufsize = total_size+32*1024;
            buf_tmp = s->pes_buffer;
        }
		memcpy(buf_tmp, pkt->data0, pkt->size0);
		size = pkt->size0;
		
		//alogd("pcm pts: %lld\n", pkt->pts);
		if(s->first_audio_pts == 1)
		{
            s->first_audio_pts = 0;
            s->base_audio_pts = pkt->pts;

            aloge("ts_audio_base_pts:%lld",s->base_audio_pts);
		}
        pkt->pts = (pkt->pts - s->base_audio_pts)*90;
//		pkt->pts = pkt->pts*90;
		s->audio_frame_num ++;
	}
    else if(st->codec.codec_id == CODEC_ID_GGAD)
    {
        int total_size = pkt->size0;
        if (s->pes_bufsize < total_size) {
            alogd("realloc pes_buffer [%d]->[%d]", s->pes_bufsize, total_size+32*1024);

            uint8_t * tmp_ptr = NULL;
            tmp_ptr = (uint8_t*)realloc(s->pes_buffer, total_size+32*1024);
            if (tmp_ptr == NULL) {
                aloge("realloc pes_buffer error!!");
                return 0;
            }
            s->pes_buffer = tmp_ptr;
            s->pes_bufsize = total_size+32*1024;
            buf_tmp = s->pes_buffer;
        }
		memcpy(buf_tmp, pkt->data0, pkt->size0);
		size = pkt->size0; 
    }
	else
	{
		aloge("unsupport codec");
		return 0;
	}

    mpegts_write_pes(s, st, s->pes_buffer, size, pkt->pts, 0, pkt->flags & AVPACKET_FLAG_KEYFRAME, pkt->stream_index);
    return 0;
}

int32_t ts_write_trailer(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    uint32_t i;

    if (s->output_buffer_mode == OUTPUT_CALLBACK_BUFFER) {
        return 0;
    }

    /* flush current packets */
    for(i = ts->cache_page_num*128;i<ts->cache_size_total/TS_PACKET_SIZE;i++)
    {
        if(ts->ts_read_ptr > ts->ts_cache_end)
            ts->ts_read_ptr = ts->ts_cache_start;
        //put_buffer(s->pb_cache, ts->ts_read_ptr, TS_PACKET_SIZE);
        put_buffer_cache(s->mpFsWriter, (char*)ts->ts_read_ptr, TS_PACKET_SIZE);
        ts->ts_read_ptr += TS_PACKET_SIZE;
    }
    flush_payload_cache(s->mpFsWriter);
	ts->ts_write_ptr = ts->ts_read_ptr = ts->ts_cache_start;
    ts->cache_size = 0;
    ts->cache_page_num = 0;
    ts->cache_size_total = 0;

    return 0;
}


