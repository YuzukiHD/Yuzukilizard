#ifndef __TXZ_ENGINE_H__
#define __TXZ_ENGINE_H__
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------*/
/*   Error codes                                       */
/*-----------------------------------------------------*/
#define CMD_CODE_NOVALID_LICENSE -5
#define CMD_CODE_ERROR_PARAMETER -4
#define CMD_CODE_ERROR_NULLPTR   -3
#define CMD_CODE_ERROR_GENER     -2
#define CMD_CODE_ERROR_UNINIT    -1
#define CMD_CODE_NORMAL           0
#define CMD_CODE_PROCESSEND       1
typedef unsigned char cmd_uint8_t;
typedef signed char cmd_int8_t;

typedef unsigned short cmd_uint16_t;
typedef signed short cmd_int16_t;

typedef unsigned int cmd_uint32_t;
typedef signed int cmd_int32_t;

typedef float cmd_float32_t;

cmd_int32_t txzEngineCheckLicense(void* channel, const char *token, const char* dir_txz_save, const char* dir_usb_push);

/**
*  This function get keyword name.
*
* @param
*      index      -I  : Keyword index.
*      name       -IO : Pointer pointer to keyword name (not has copy operation) or NULL.
* @return
*      Returns CMD_CODE_NORMAL if success or other error code.
*/

cmd_int32_t txzEngineGetName(cmd_uint16_t index, const char** name);

/**
 * Allocates the memory needed by the CMD.
 *
 * @param
 *      cmdInst -IO: Pointer to the created object or NULL with error.
 *
 */
void txzEngineCreate(void** cmdInst);

/**
 *  Initializes the CMD instance.
 *
 * @param
 *      cmdInst    -IO : Pointer to the CMD instance.
 * @return
 *      Returns CMD_CODE_NORMAL if success or other error code.
 */
cmd_int32_t txzEngineCmdInit(void* cmdInst);

/**
 *  Reset the CMD instance.
 *
 * @param
 *      cmdInst    -IO : Pointer to the CMD instance.
 * @return
 *      Returns CMD_CODE_NORMAL if success or other error code.
 */
cmd_int32_t txzEngineReset(void* cmdInst);

/**
* Search keyword on one frame of data.
*
* @param
*      cmdInst        -IO : Pointer to the CMD instance.
*      audio          -I  : In buffer containing one frame of recording signal (Number of samples must be 160).
*                           If "audio" set NULL means end of process.
*      samples        -I  : Number of samples in audio buffer, must be 160 now.
*      keyIndex       -IO : Index greater than 0 means keyword has been searched, call txzEngineGetName() to review the keyword name.
* @return
 *      Returns CMD_CODE_NORMAL or CMD_CODE_PROCESSEND if success or other error code
*/
cmd_int32_t  txzEngineProcess(void* cmdInst, const cmd_int16_t* audio, cmd_uint16_t samples, cmd_uint16_t* keyIndex, float* confidence);

/**
 * This function releases the memory allocated by txzEngineCreate().
 *
 * @param
 *     uvoiceCMD -IO: Pointer to the CMD instance.
 */
void txzEngineDesrtoy(void** cmdInst);
#ifdef __cplusplus
}
#endif

#endif
