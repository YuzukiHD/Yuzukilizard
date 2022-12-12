/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviOdmlIndx.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_ODML_INDX_H_
#define _CDX_AVI_ODML_INDX_H_

//typedef cdx_uint32 FOURCC;

struct OdmlIndxTblEntry{
    //absolute file offset of one index chunk.but it points to chunk head.
    //ref to [Fianl Fantasy X-2 - Real Emotion (DivX503).avi]
    cdx_int64   qwOffset;
    cdx_int32   dwSize;     //size of index chunk, two case :
                        //(1)not include chunk head, (2)include chunk head.
                        //we default use case(2), (ref to Open DML AVI file spec,Page9)
    cdx_int32   dwDuration; //time span in stream ticks??
};

typedef struct {
    FOURCC  fcc;                //"ix00", etc
    cdx_int8    bIndexSubType;  //AVI_INDEX_2FILED,etc, bIndexType is verified
                                //for CDX_AVI_INDEX_OF_CHUNKS.
    cdx_int32   wLongsPerEntry; //every index entry size.unit: DWORD(4byte).
    cdx_int32   dwChunkId;      //"##dc", etc, for debug info.

    cdx_int32   size;           //this index chunk's size. we use it for verify this chunk's valid,
                                //not include head.
    cdx_int32   nEntriesInUse;  //effective idx_items in this index chunk.

    cdx_int64   qwBaseOffset;   //for index video ,audio chunk.

    cdx_int32   leftIdxItem;    //left entry total count in flash.
    cdx_int32   bufIdxItem;     //left entry count in file buffer.
    cdx_uint8    *pFileBuffer;  //need dynamic malloc.size = INDEX_CHUNK_BUFFER_SIZE
    cdx_uint8    *pIdxEntry;    //point to filebuffer, type:avistdindex_enty or avifieldindex_enty

    cdx_int32   ckReadEndFlag;  //0:this index chunk is not read end, 1:read end index chunk.
}ODML_INDEX_READER;

typedef struct {
    cdx_int8    bIndexSubType;  //AVI_INDEX_2FILED
    cdx_int32   dwChunkId;      //"##dc",

    cdx_int32   indxTblEntryCnt; //index chunk array total count.
    cdx_int32   indxTblEntryIdx; //index chunk array index.从-1开始
    struct OdmlIndxTblEntry *indxTblEntryArray;//need dynamic malloc.

    CdxStreamT    *idxFp;
    cdx_int64   fpSuperindexTable;  //指向super index table的起始处，其实就是LIST strl->indx的位置，
                                    //在init_psr_indx_table_reader()中被初始化
    cdx_int64   fpPos;              //current fp position
    ODML_INDEX_READER   odmlIdxReader;

    cdx_int32   readEndFlg;         //1:all index chunk read end!

//below variables are for build KeyFrame Table, they are not used in normal play!
//when normal play, we will stat param in AVI_FILE_IN, not this structure.
//before read entry, it is this entry's chunk idx, after read entry,
//it is the next chunk idx, it is a chunk counter.
//以下这些变量都是从chunk index entry读到的，不是从真正对应的chunk读到的
    cdx_int32   chunkCounter;           //读完本chunk之后，就是下一个将要读取的
                                        //chunk的序号(相对于总帧数，从0开始)
    cdx_int32   chunkSize;              //not include chunk head. current readed chunk's size.
    cdx_int64   chunkIxTblEntOffset;    //current chunk's index item's offset.
    cdx_int64   chunkOffset;            //当前读到的indx entry所指示的chunk的chunk_head的位置
                                        //绝对地址
    cdx_int64   totalChunkSize;         //all readed chunk's size, include current chunk,
                                        //audio will use it.
}ODML_SUPERINDEX_READER;  //ref to OpenDML AVI File Format Extensions
//extern cdx_int32 isIndxChunkId(FOURCC fcc);
//extern cdx_int32 load_indx_chunk(ODML_SUPERINDEX_READER *psuper_reader,
// cdx_int32 indxtbl_entry_idx);
//extern cdx_int32 search_next_ODML_index_entry(ODML_SUPERINDEX_READER *psuper_reader,
// AVI_CHUNK_POSITION *pck_pos);
//extern CDX_S16 AVI_build_idx_for_ODML_index_mode(struct FILE_PARSER *p);
//extern cdx_int32 initial_psr_indx_table_reader(ODML_SUPERINDEX_READER *preader,
// AVI_FILE_IN *avi_in, cdx_int32 stream_index);
//extern void deinitial_psr_indx_table_reader(ODML_SUPERINDEX_READER *preader);
#endif  /* _CDX_AVI_ODML_INDX_H_ */

