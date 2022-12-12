/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : Envelope.c
 * Description : Envelope
 * Author  : xuqi <xuqi@allwinnertech.com>
 * Comment : 创建初始版本
 *
 */

#include <Envelope.h>
#include <CdxTypes.h>
#include <CdxStream.h>

#include <drmcertcache.h>
#include <drmmanager.h>
#include <drmsecureclock.h>
#include <drmlicreason.h>
#include <drmxmlparser.h>
#include <drmutf.h>
#include <drmembedding.h>
#include <drmrevocation.h>
#include <drmcontract.h>
#include <drmxmlhash.h>
#include <drmsoapxmlutility.h>
#include <drmwmdrmnet.h>
#include <drmmeterapi.h>
#include <drmhdsblockheadercache.h>
#include <drmmanagerimpl.h>
#include <drmcleanstore.h>
#include <drmperformance.h>
#include <drmactivationimp.h>
#include <drmprofile.h>
#include <drmconstants.h>
#include <drmversionconstants.h>
#include <drmlicgen.h>
#include <drmactivation.h>
#include <drmantirollbackclock.h>
#include <drmmathsafe.h>
#include <drmdomainstore.h>
#include <drmblackboxtypes.h>
#include <drmchain.h>
#include <drmest.h>
#include <drmsecurecore.h>
#include <drmbytemanip.h>
#include <drmprndprotocol.h>
#include <drmmodulesupport.h>
#include <oemcocktail.h>
#include <drmlastinclude.h>

static DRM_RESULT _LoadEnvelopeHeader(
    __inout OEM_FILEHDL           f_oFileHandle,
    __out   DRM_ENVELOPE_HEADER  *f_poEnvelopeHeader )
{
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  cbRead = 0;
    /* Temporary buffer to read the envelope header into. Due to slack in the struct, the
    ** actual envelope header could potentially be smaller than sizeof( DRM_ENVELOPE_HEADER )
    ** but should never be larger
    */
    DRM_BYTE rgbHeaderBuffer[sizeof( DRM_ENVELOPE_HEADER )] = {0};

    /*
    ** Seek to the beginning of the file.
       */
    /*ChkBOOL( Oem_File_SetFilePointer( f_oFileHandle,
                                      0,
                                      OEM_FILE_BEGIN,
                                      NULL ), DRM_E_FILE_SEEK_ERROR );*/

    ChkBOOL((CdxStreamSeek(f_oFileHandle, 0, SEEK_SET) == 0), DRM_E_FILE_SEEK_ERROR);

    /*ChkBOOL( Oem_File_Read( f_oFileHandle,
                            rgbHeaderBuffer,
                            DRM_ENVELOPE_MINIMUM_HEADER_SIZE,
                           &cbRead ), DRM_E_FILE_READ_ERROR );

    ChkBOOL( cbRead == DRM_ENVELOPE_MINIMUM_HEADER_SIZE, DRM_E_FILE_READ_ERROR );*/

    ChkBOOL((CdxStreamRead(f_oFileHandle,
                             rgbHeaderBuffer,
                             DRM_ENVELOPE_MINIMUM_HEADER_SIZE) == DRM_ENVELOPE_MINIMUM_HEADER_SIZE),
                             DRM_E_FILE_READ_ERROR );

    /*
    ** Load the header from the byte buffer to the struct. Data isn't read directly into the
    ** struct with ReadFile due to the potential for different packing on varying compilers
    */
    cbRead = 0;
    LITTLEENDIAN_BYTES_TO_DWORD( f_poEnvelopeHeader->dwFileSignature, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_DWORD );

    LITTLEENDIAN_BYTES_TO_DWORD( f_poEnvelopeHeader->cbHeaderSize, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_DWORD );

    LITTLEENDIAN_BYTES_TO_DWORD( f_poEnvelopeHeader->dwFileDataOffset, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_DWORD );

    LITTLEENDIAN_BYTES_TO_WORD( f_poEnvelopeHeader->wFormatVersion, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_WORD );

    LITTLEENDIAN_BYTES_TO_WORD( f_poEnvelopeHeader->wCompatibleVersion, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_WORD );

    LITTLEENDIAN_BYTES_TO_DWORD( f_poEnvelopeHeader->dwCipherType, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_DWORD );

    DRM_BYT_CopyBytes( f_poEnvelopeHeader->rgbCipherData, 0, rgbHeaderBuffer, cbRead,
                    DRM_ENVELOPE_CIPHER_DATA_SIZE );
    cbRead += DRM_ENVELOPE_CIPHER_DATA_SIZE;

    LITTLEENDIAN_BYTES_TO_WORD( f_poEnvelopeHeader->cbOriginalFilename, rgbHeaderBuffer, cbRead );
    cbRead += sizeof( DRM_WORD );

    LITTLEENDIAN_BYTES_TO_DWORD( f_poEnvelopeHeader->cbDrmHeaderLen, rgbHeaderBuffer, cbRead );

ErrorExit:
    return dr;
}

DRM_API DRM_RESULT DRM_CALL Env_Envelope_Open(
    __in           DRM_APP_CONTEXT            *f_poAppContext,
    __in_opt       DRM_VOID                   *f_pOEMContext,
    __in_z     const CdxStreamT               *f_pStream,
    __out          DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile )
{
    DRM_RESULT                dr                   = DRM_SUCCESS;
    DRM_APP_CONTEXT_INTERNAL *poAppContextInternal =
                           DRM_REINTERPRET_CAST( DRM_APP_CONTEXT_INTERNAL, f_poAppContext );
    DRM_DWORD                 cbRead               = 0;

    CDX_UNUSE(f_pOEMContext);
    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_APP, PERF_FUNC_Drm_Envelope_Open );

    ChkArg( f_pStream != NULL );
    ChkArg( f_pEnvFile != NULL );
    ChkArg( f_poAppContext != NULL );

    ZEROMEM( f_pEnvFile, sizeof( DRM_ENVELOPED_FILE_CONTEXT ) );

    f_pEnvFile->pEnvFileHandle = (OEM_FILEHDL)f_pStream;

    /*
    ** Read the file header.
    */
    ChkDR( _LoadEnvelopeHeader( f_pEnvFile->pEnvFileHandle, &f_pEnvFile->oEnvHeader ) );

    /*
    ** Check the file signature,
    make sure reported header size is sane, and check that the version is OK.
    */
    ChkBOOL( f_pEnvFile->oEnvHeader.dwFileSignature ==
                                   DRM_ENVELOPE_FILE_SIGNATURE, DRM_E_ENVELOPE_CORRUPT );
    ChkBOOL( f_pEnvFile->oEnvHeader.cbHeaderSize >=
                                 DRM_ENVELOPE_MINIMUM_HEADER_SIZE, DRM_E_ENVELOPE_CORRUPT );
    ChkBOOL( f_pEnvFile->oEnvHeader.wCompatibleVersion <=
                     DRM_ENVELOPE_CURRENT_FORMAT_VERSION, DRM_E_ENVELOPE_FILE_NOT_COMPATIBLE );
    ChkBOOL( f_pEnvFile->oEnvHeader.dwFileDataOffset >=
                                f_pEnvFile->oEnvHeader.cbHeaderSize, DRM_E_ENVELOPE_CORRUPT );

    /*
    ** Check that the header length is valid.
    */
    ChkBOOL( f_pEnvFile->oEnvHeader.cbDrmHeaderLen <=
                            poAppContextInternal->cbDRMHeaderData, DRM_E_BUFFERTOOSMALL );
    ChkBOOL( f_pEnvFile->oEnvHeader.cbDrmHeaderLen % sizeof( DRM_WCHAR ) ==
                                                       0, DRM_E_ENVELOPE_CORRUPT );

    /*
    ** Skip over the original filename.
    */
    /*ChkBOOL( Oem_File_SetFilePointer( f_pEnvFile->pEnvFileHandle,
                                      f_pEnvFile->oEnvHeader.cbOriginalFilename,
                                      OEM_FILE_CURRENT,
                                      NULL ), DRM_E_FILE_SEEK_ERROR );*/
    ChkBOOL((CdxStreamSeek(f_pEnvFile->pEnvFileHandle, f_pEnvFile->oEnvHeader.cbOriginalFilename,
                           SEEK_CUR) == 0), DRM_E_FILE_SEEK_ERROR);


    /*
    ** Read the RM content header itself.
    */
    /*ChkBOOL( Oem_File_Read(  f_pEnvFile->pEnvFileHandle,
                             poAppContextInternal->pbDRMHeaderData,
                             f_pEnvFile->oEnvHeader.cbDrmHeaderLen,
                            &cbRead ), DRM_E_FILE_READ_ERROR );
    ChkBOOL( f_pEnvFile->oEnvHeader.cbDrmHeaderLen == cbRead, DRM_E_FILE_READ_ERROR );*/

    ChkBOOL((CdxStreamRead(f_pEnvFile->pEnvFileHandle,
                             poAppContextInternal->pbDRMHeaderData,
                             f_pEnvFile->oEnvHeader.cbDrmHeaderLen) ==
                                         (cdx_int32)f_pEnvFile->oEnvHeader.cbDrmHeaderLen),
                             DRM_E_FILE_READ_ERROR );

    poAppContextInternal->eHeaderInContext = DRM_CSP_HEADER_NOT_SET;
    ChkDRMap( Drm_Content_SetProperty( f_poAppContext,
                               DRM_CSP_AUTODETECT_HEADER,
                               poAppContextInternal->pbDRMHeaderData,
                               f_pEnvFile->oEnvHeader.cbDrmHeaderLen ),
                               DRM_E_INVALIDARG, DRM_E_CH_INVALID_HEADER );

    /*
    ** Parse the cipher intiailization values.
    */
    switch ( f_pEnvFile->oEnvHeader.dwCipherType )
    {
        case eDRM_AES_COUNTER_CIPHER:
            /* Copy the initial AES counter */
            MEMCPY( ( DRM_BYTE * )&f_pEnvFile->qwInitialCipherCounter,
                    f_pEnvFile->oEnvHeader.rgbCipherData,
                    sizeof( DRM_UINT64 ) );

            FIX_ENDIAN_QWORD( f_pEnvFile->qwInitialCipherCounter );
            break;

        case eDRM_RC4_CIPHER:
            break;

        default:
            ChkDR( DRM_E_ENVELOPE_FILE_NOT_COMPATIBLE );
    }

    f_pEnvFile->dwFileDataStart = f_pEnvFile->oEnvHeader.dwFileDataOffset;

    /*
    ** Seek to the beginning of the file data.
    */
    if ( f_pEnvFile->dwFileDataStart > f_pEnvFile->oEnvHeader.cbHeaderSize )
    {
        DRM_LONG lFileStart = 0;
        ChkDR( DRM_DWordToLong( f_pEnvFile->dwFileDataStart, &lFileStart ) );
        /*ChkBOOL( Oem_File_SetFilePointer( f_pEnvFile->pEnvFileHandle,
                                          lFileStart,
                                          OEM_FILE_BEGIN, NULL ), DRM_E_FILE_SEEK_ERROR );*/
        ChkBOOL((CdxStreamSeek(f_pEnvFile->pEnvFileHandle, lFileStart, SEEK_SET) == 0),
                DRM_E_FILE_SEEK_ERROR);

    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    ChkECC( ECC_Drm_Envelope_Open, dr );
    return dr;
}

DRM_API DRM_RESULT DRM_CALL Env_Envelope_InitializeRead(
    __in DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __in DRM_DECRYPT_CONTEXT        *f_pDecrypt )
{
    DRM_RESULT dr                                       = DRM_SUCCESS;
    DRM_BYTE   rgbLast15[ DRM_CPHR_CB_FOR_INITDECRYPT ] = { 0 };
    DRM_DWORD  dwFileSize                               = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_APP, PERF_FUNC_Drm_Envelope_InitializeRead );

    ChkArg( f_pEnvFile != NULL );
    ChkArg( f_pDecrypt != NULL );
    ChkArg( f_pEnvFile->pEnvFileHandle != OEM_INVALID_HANDLE_VALUE );
    ChkArg( f_pEnvFile->dwFileDataStart != 0 );

    ChkDR( DRM_SECURECORE_DuplicateDecryptContext( &f_pEnvFile->oDecrypt,
                             DRM_REINTERPRET_CAST( DRM_CIPHER_CONTEXT, f_pDecrypt ) ) );

    ChkDR( Env_Envelope_GetSize( f_pEnvFile, &dwFileSize ) );

    /*
    ** For RC4 we need to grab the last 15 bytes.
    */
    if ( f_pEnvFile->oDecrypt.eCipherType == eDRM_RC4_CIPHER )
    {
        DRM_DWORD cbLenToRead      = DRM_CPHR_CB_FOR_INITDECRYPT;
        DRM_DWORD cbRead           = 0;
        DRM_DWORD dwOldSeekPointer = 0;

        if ( dwFileSize < cbLenToRead )
        {
            cbLenToRead = dwFileSize;
        }

        /* Save the seek pointer. */
        /*ChkBOOL( Oem_File_SetFilePointer(  f_pEnvFile->pEnvFileHandle,
                                           0,
                                           OEM_FILE_CURRENT,
                                          &dwOldSeekPointer ), DRM_E_FILE_SEEK_ERROR );*/
        ChkBOOL((dwOldSeekPointer = CdxStreamTell(f_pEnvFile->pEnvFileHandle)) !=
                                           (DRM_DWORD)-1, DRM_E_FILE_SEEK_ERROR);


        /* Go to the end and grab the last 15 bytes. */
        /*ChkBOOL( Oem_File_SetFilePointer( f_pEnvFile->pEnvFileHandle,
                                          -DRM_CPHR_CB_FOR_INITDECRYPT,
                                          OEM_FILE_END,
                                          NULL ), DRM_E_FILE_SEEK_ERROR );*/
        ChkBOOL((CdxStreamSeek(f_pEnvFile->pEnvFileHandle, -DRM_CPHR_CB_FOR_INITDECRYPT,
                  SEEK_END) == 0), DRM_E_FILE_SEEK_ERROR);


        /*ChkBOOL( Oem_File_Read(  f_pEnvFile->pEnvFileHandle,
                                 rgbLast15,
                                 cbLenToRead,
                                &cbRead ), DRM_E_FILE_READ_ERROR );

        ChkBOOL( cbLenToRead == cbRead, DRM_E_FILE_READ_ERROR );*/
        ChkBOOL((CdxStreamRead(f_pEnvFile->pEnvFileHandle,
                                 rgbLast15,
                                 cbLenToRead) == (cdx_int32)cbLenToRead),
                                 DRM_E_FILE_READ_ERROR );

        /* Restore the seek pointer. */
        {
            DRM_LONG lOldSeekPointer = 0;
            ChkDR( DRM_DWordToLong( dwOldSeekPointer, &lOldSeekPointer ) );
            /*ChkBOOL( Oem_File_SetFilePointer( f_pEnvFile->pEnvFileHandle,
                                              lOldSeekPointer,
                                              OEM_FILE_BEGIN, NULL ), DRM_E_FILE_SEEK_ERROR );*/
            ChkBOOL((CdxStreamSeek(f_pEnvFile->pEnvFileHandle, lOldSeekPointer, SEEK_SET) == 0),
                                    DRM_E_FILE_SEEK_ERROR);

        }
        ChkDR( Drm_Cocktail_InitDecrypt( DRM_REINTERPRET_CAST( DRM_DECRYPT_CONTEXT,
                                      &f_pEnvFile->oDecrypt ), rgbLast15, dwFileSize ) );
    }
    else
    {
        ChkBOOL( f_pEnvFile->oDecrypt.eCipherType == eDRM_AES_COUNTER_CIPHER,
                 DRM_E_UNSUPPORTED_ALGORITHM );
    }

ErrorExit:
    if( DRM_FAILED( dr ) )
    {
        if( f_pEnvFile != NULL
         && f_pEnvFile->oDecrypt.fInited == TRUE )
        {
            ChkVOID( DRM_SECURECORE_CloseDecryptContext( &f_pEnvFile->oDecrypt ) );
        }
    }

    DRM_PROFILING_LEAVE_SCOPE;
    ChkECC( ECC_Drm_Envelope_InitializeRead, dr );
    return dr;
}

DRM_API DRM_RESULT DRM_CALL Env_Envelope_Close(
    __in DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_APP, PERF_FUNC_Drm_Envelope_Close );

    ChkArg( f_pEnvFile != NULL );
    ChkArg( f_pEnvFile->pEnvFileHandle != OEM_INVALID_HANDLE_VALUE );
    ChkArg( f_pEnvFile->dwFileDataStart != 0 );

    f_pEnvFile->pEnvFileHandle = OEM_INVALID_HANDLE_VALUE;

    ChkVOID( DRM_SECURECORE_CloseDecryptContext( &f_pEnvFile->oDecrypt ) );

ErrorExit:

    DRM_PROFILING_LEAVE_SCOPE;

    ChkECC( ECC_Drm_Envelope_Close, dr );

    return dr;
}

DRM_API DRM_RESULT DRM_CALL Env_Envelope_GetSize(
    __in  DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __out DRM_DWORD                  *f_pcbFileSize )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_APP, PERF_FUNC_Drm_Envelope_GetSize );

    ChkArg( f_pEnvFile != NULL );
    ChkArg( f_pcbFileSize != NULL );
    ChkArg( f_pEnvFile->pEnvFileHandle != OEM_INVALID_HANDLE_VALUE );
    ChkArg( f_pEnvFile->dwFileDataStart != 0 );

    /*ChkBOOL( Oem_File_GetSize( f_pEnvFile->pEnvFileHandle, f_pcbFileSize ),
                              DRM_E_FILE_SEEK_ERROR );*/
    *f_pcbFileSize = CdxStreamSize(f_pEnvFile->pEnvFileHandle);

    /* Subtract off the size of our header. */
    ChkBOOL( *f_pcbFileSize >= f_pEnvFile->dwFileDataStart, DRM_E_ENVELOPE_CORRUPT );

    *f_pcbFileSize -= f_pEnvFile->dwFileDataStart;

ErrorExit:

    DRM_PROFILING_LEAVE_SCOPE;

    ChkECC( ECC_Drm_Envelope_GetSize, dr );

    return dr;
}

DRM_API DRM_RESULT DRM_CALL Env_Envelope_Read(
    __in                                              DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __out_ecount_part( f_cbToRead, *f_pcbBytesRead )  DRM_BYTE                   *f_pbBuffer,
    __in                                              DRM_DWORD                   f_cbToRead,
    __out                                             DRM_DWORD                  *f_pcbBytesRead )
{
    DRM_RESULT                    dr          = DRM_SUCCESS;
    DRM_AES_COUNTER_MODE_CONTEXT  ctrContext  = { 0 };
    DRM_AES_COUNTER_MODE_CONTEXT *pCtrContext = NULL;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_APP, PERF_FUNC_Drm_Envelope_Read );

    ChkArg( f_pEnvFile != NULL );
    ChkArg( f_pcbBytesRead != NULL );
    ChkArg( f_pbBuffer != NULL );
    ChkArg( f_pEnvFile->pEnvFileHandle != OEM_INVALID_HANDLE_VALUE );
    ChkArg( f_pEnvFile->dwFileDataStart != 0 );

    *f_pcbBytesRead = 0;

    /* For AES counter mode get the file offset. */
    if ( f_pEnvFile->oDecrypt.eCipherType == eDRM_AES_COUNTER_CIPHER )
    {
        DRM_DWORD dwFilePosition = 0;

        /*ChkBOOL( Oem_File_SetFilePointer(  f_pEnvFile->pEnvFileHandle,
                                           0,
                                           OEM_FILE_CURRENT,
                                          &dwFilePosition ), DRM_E_FILE_SEEK_ERROR );*/
        ChkBOOL((dwFilePosition = CdxStreamTell(f_pEnvFile->pEnvFileHandle)) !=
                 (DRM_DWORD)-1, DRM_E_FILE_SEEK_ERROR);


        dwFilePosition -= f_pEnvFile->dwFileDataStart;

        ctrContext.qwBlockOffset          = DRM_UI64( dwFilePosition / DRM_AES_BLOCKLEN );
        ctrContext.bByteOffset            = ( DRM_BYTE )( dwFilePosition % DRM_AES_BLOCKLEN );
        ctrContext.qwInitializationVector = f_pEnvFile->qwInitialCipherCounter;
        pCtrContext                       = &ctrContext;
    }

    /*ChkBOOL( Oem_File_Read( f_pEnvFile->pEnvFileHandle,
                            f_pbBuffer,
                            f_cbToRead,
                            f_pcbBytesRead ), DRM_E_FILE_READ_ERROR );

    ChkBOOL( *f_pcbBytesRead <= f_cbToRead, DRM_E_FILE_READ_ERROR );*/
    ChkBOOL(((*f_pcbBytesRead = CdxStreamRead(f_pEnvFile->pEnvFileHandle,
                             f_pbBuffer,
                             f_cbToRead)) <= f_cbToRead),
                             DRM_E_FILE_READ_ERROR );


    if ( *f_pcbBytesRead > 0 )
    {
        /* Decrypt it. */
        ChkDR( DRM_SECURECORE_DecryptContentLegacy( &f_pEnvFile->oDecrypt, pCtrContext,
                                                   *f_pcbBytesRead, f_pbBuffer ) );
    }

ErrorExit:

    if ( DRM_FAILED( dr ) && f_pcbBytesRead != NULL )
    {
        *f_pcbBytesRead = 0;
    }

    DRM_PROFILING_LEAVE_SCOPE;

    ChkECC( ECC_Drm_Envelope_Read, dr );

    return dr;
}

DRM_API DRM_RESULT DRM_CALL Env_Envelope_Seek(
    __in  DRM_ENVELOPED_FILE_CONTEXT *f_pEnvFile,
    __in  DRM_LONG                    f_lDistanceToMove,
    __in  DRM_DWORD                   f_dwMoveMethod,
    __out DRM_DWORD                  *f_pdwNewFilePointer )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_APP, PERF_FUNC_Drm_Envelope_Seek );

    ChkArg( f_pEnvFile != NULL );
    ChkArg( f_pEnvFile->pEnvFileHandle != OEM_INVALID_HANDLE_VALUE );
    ChkArg( f_pEnvFile->dwFileDataStart != 0 );

    /* Seeking in an RC4 encrypted envelope file is not supported. */
    ChkBOOL( f_pEnvFile->oDecrypt.eCipherType != eDRM_RC4_CIPHER, DRM_E_NOTIMPL );

    /* Add the file header size when moving relative to the start of the file. */
    if ( f_dwMoveMethod == OEM_FILE_BEGIN )
    {
        ChkArg( f_lDistanceToMove >= 0 );

        f_lDistanceToMove += f_pEnvFile->dwFileDataStart;
    }
    else if ( f_dwMoveMethod == OEM_FILE_END )
    {
        ChkArg( f_lDistanceToMove <= 0 );
    }

    /*ChkBOOL( Oem_File_SetFilePointer( f_pEnvFile->pEnvFileHandle,
                                      f_lDistanceToMove,
                                      f_dwMoveMethod,
                                      f_pdwNewFilePointer ), DRM_E_FILE_SEEK_ERROR );*/
    ChkBOOL((CdxStreamSeek(f_pEnvFile->pEnvFileHandle, f_lDistanceToMove, f_dwMoveMethod) == 0),
                           DRM_E_FILE_SEEK_ERROR);
    ChkBOOL((*f_pdwNewFilePointer = CdxStreamTell(f_pEnvFile->pEnvFileHandle)) != (DRM_DWORD)-1,
                                                DRM_E_FILE_SEEK_ERROR);

    *f_pdwNewFilePointer -= f_pEnvFile->dwFileDataStart;

ErrorExit:

    DRM_PROFILING_LEAVE_SCOPE;

    ChkECC( ECC_Drm_Envelope_Seek, dr );

    return dr;
}
