/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

#ifndef CEDARX_STREAM_FILE_H_
#define CEDARX_STREAM_FILE_H_
#include <cedarx_stream.h>

int cdx_seek_stream_file(struct cdx_stream_info *stream, cdx_off_t offset, int whence);
cdx_off_t cdx_tell_stream_file(struct cdx_stream_info *stream);
ssize_t cdx_read_stream_file(void *ptr, size_t size, size_t nmemb, struct cdx_stream_info *stream);
ssize_t cdx_write_stream_file(const void *ptr, size_t size, size_t nmemb, struct cdx_stream_info *stream);
long long cdx_get_stream_size_file(struct cdx_stream_info *stream);
int cdx_truncate_stream_file(struct cdx_stream_info *stream, cdx_off_t length);
int cdx_fallocate_stream_file(struct cdx_stream_info *stream, int mode, int64_t offset, int64_t len);
int create_outstream_handle_file(struct cdx_stream_info *stm_info, CedarXDataSourceDesc *datasource_desc);
int file_create_instream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stm_info);

//add by weihongqiang, handle files with file descriptor.
int cdx_seek_fd_file(struct cdx_stream_info *stream, cdx_off_t offset, int whence);
cdx_off_t cdx_tell_fd_file(struct cdx_stream_info *stream);
ssize_t cdx_read_fd_file(void *ptr, size_t size, size_t nmemb, struct cdx_stream_info *stream);
ssize_t cdx_write_fd_file(const void *ptr, size_t size, size_t nmemb, struct cdx_stream_info *stream);
long long cdx_get_fd_size_file(struct cdx_stream_info *stream);
int cdx_truncate_fd_file(struct cdx_stream_info *stream, cdx_off_t length);
int cdx_fallocate_fd_file(struct cdx_stream_info *stream, int mode, int64_t offset, int64_t len);

#endif
