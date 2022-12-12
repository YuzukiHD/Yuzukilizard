/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : Envelope.h
 * Description : Envelope
 * Author  : xuqi <xuqi@allwinnertech.com>
 * Comment : 创建初始版本
 *
 */

#include <CdxTypes.h>
#include <CdxStream.h>
#include <drmmanager.h>

/* difference between Env_* and Drm_*:
 * Env_* use (CdxStreamT *) as file handle,
 * Drm_* use (Oem_File_Open) to open file.
 */
DRM_API DRM_RESULT DRM_CALL Env_Envelope_Open(
    __in           DRM_APP_CONTEXT            *f_poAppContext,
    __in_opt       DRM_VOID                   *f_pOEMContext,
    __in_z     const CdxStreamT               *f_pStream,
    __out          DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile );

DRM_API DRM_RESULT DRM_CALL Env_Envelope_InitializeRead(
    __in DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __in DRM_DECRYPT_CONTEXT        *f_pDecrypt );


DRM_API DRM_RESULT DRM_CALL Env_Envelope_Close(
    __in DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile );


DRM_API DRM_RESULT DRM_CALL Env_Envelope_GetSize(
    __in  DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __out DRM_DWORD                  *f_pcbFileSize );

DRM_API DRM_RESULT DRM_CALL Env_Envelope_Read(
    __in                                              DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __out_ecount_part( f_cbToRead, *f_pcbBytesRead )  DRM_BYTE                   *f_pbBuffer,
    __in                                              DRM_DWORD                   f_cbToRead,
    __out                                             DRM_DWORD                  *f_pcbBytesRead );

DRM_API DRM_RESULT DRM_CALL Env_Envelope_Seek(
    __in  DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __in  DRM_LONG                    f_lDistanceToMove,
    __in  DRM_DWORD                   f_dwMoveMethod,
    __out DRM_DWORD                  *f_pdwNewFilePointer );
