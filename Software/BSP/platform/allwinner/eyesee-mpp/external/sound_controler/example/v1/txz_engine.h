#ifndef _TXZ_ENGINE_H_
#define _TXZ_ENGINE_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*-----------------------------------------------------*/
/*   Error codes                                       */
/*-----------------------------------------------------*/
#define CMD_CODE_ERROR_PARAMETER -4
#define CMD_CODE_ERROR_NULLPTR   -3
#define CMD_CODE_ERROR_GENER     -2
#define CMD_CODE_ERROR_UNINIT    -1
#define CMD_CODE_NORMAL           0
#define CMD_CODE_PROCESSEND       1

/*--------------------------------------------------------------------------------------*/
/*   Type definitions                                                                       */
/*--------------------------------------------------------------------------------------*/
typedef unsigned char cmd_uint8_t;
typedef signed char cmd_int8_t;

typedef unsigned short cmd_uint16_t;
typedef signed short cmd_int16_t;

typedef unsigned int cmd_uint32_t;
typedef signed int cmd_int32_t;

typedef float cmd_float32_t;

typedef enum cmd_paramterId_e {
    cmd_shiftFrames,   /* cmd_uint16_t  : 1 (0~10) */
    cmd_smoothFrames,  /* cmd_uint16_t  : 30 (0~30) */
    cmd_lockFrames,    /* cmd_uint16_t  : 60 (0~100) */
    cmd_postMaxFrames, /* cmd_uint16_t  : 100 (0~100) */
    cmd_thresHold,     /* cmd_float32_t : 0.60f (0.0f~1.0f) */
} cmd_paramterId;

/*-----------------------------------------------------------------------------*/
/*    Function prototypes                                                   */
/*-----------------------------------------------------------------------------*/
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

#ifdef ANDROID
void* txzEngineSetPrintFunc(void* this_print_func);
#endif

/**
*  This function get all keywords.
*
* @return
*      Returns keywords.
*/
const char* txzEngineGetKeywords(void);

/**
*  This function get version.
*
* @return
*      Returns version name.
*/
const char* txzEngineGetVersion(void);

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
cmd_int32_t txzEngineInit(void* cmdInst);

/**
 *   This function enables the user to set certain parameters on-the-fly.
 *
 * @param
 *      cmdInst    -IO : Pointer to the CMD instance.
 *      parm_id    -I  : Parameter id.
*       data       -I  : Pointer to the parameter data.
 * @return
 *      Returns CMD_CODE_NORMAL if success or other error code.
 */
cmd_int32_t txzEngineSetConfig(void* cmdInst, cmd_paramterId parmId, const void* data);

/**
 *   This function enables the user to get certain parameters on-the-fly.
 *
 * @param
 *      cmdInst    -IO : Pointer to the CMD instance.
 *      parm_id    -I  : Parameter id.
*       data       -I  : Pointer to the parameter data.
 * @return
 *      Returns CMD_CODE_NORMAL if success or other error code.
 */
cmd_int32_t txzEngineGetConfig(void* cmdInst, cmd_paramterId parmId, void* data);

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
*      keyIndex       -IO : Index greater than 0 means keyword has been searched, call cmdGetName() to review the keyword name.
* @return
 *      Returns CMD_CODE_NORMAL or CMD_CODE_PROCESSEND if success or other error code
*/
cmd_int32_t txzEngineProcess(void* cmdInst, const cmd_int16_t* audio, cmd_uint16_t samples, cmd_uint16_t* keyIndex, float* confidence);

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
