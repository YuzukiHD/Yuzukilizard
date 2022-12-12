#ifndef	__RAWMUXER_H
#define	__RAWMUXER_H

#include <encoder_type.h>
#include <record_writer.h>
#include <cedarx_stream.h>

#define offset_t int
#define RAW_SYNC_HEADER 0x00005A00
#define RAW_BYTEIO_BUFFER_SIZE 256
#define RAW_MAX_STREAM_COUNT 8
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))


typedef struct RAWCODECINFO {
	enum CodecType  codec_type;
	enum CodecID	codec_id;			/* see CODEC_TYPE_xxx */

    int			bit_rate;
	int			width, height;
	int         frame_rate;

    int			sample_rate;		// samples per second
    int			channels;
    unsigned int			bits_per_sample;

	unsigned int			frame_size;
	unsigned int	        byte_per_frame;

	unsigned char*         extradata;
	unsigned int         extradata_size;
} RAWCODECINFO;

typedef struct RAWTRACKHEADER{
	int  Length;
	char Version;
	char Flags;
	char StreamType[4];
}RAWTRACKHEADER;

typedef struct RAWTRACK {
	int PacketLength;
	unsigned char  StreamIndex;
	unsigned int Duration;
	unsigned char  PacketFlags;

	int64_t TimeStamp;
	unsigned char  Identifier[4];
	unsigned int CRC;

	unsigned char  *PacketData;
	RAWCODECINFO CodecInfo;
	RAWTRACKHEADER TrackHeader;
} RAWTRACK;

typedef struct RAWCONTEXT {
	RawPacketHeader RawPacketHdr;
	unsigned char PacketNumInStream;
	unsigned short SequenceNumInPacket;
	int TrackCount;
	//int VideoStreamIndex;
	int AudioStreamIndex;
	RAWTRACK *Track[RAW_MAX_STREAM_COUNT];

	struct cdx_stream_info *OutStreamHandle;
	CedarXDataSourceDesc datasource_desc;
} RAWCONTEXT;

#endif

