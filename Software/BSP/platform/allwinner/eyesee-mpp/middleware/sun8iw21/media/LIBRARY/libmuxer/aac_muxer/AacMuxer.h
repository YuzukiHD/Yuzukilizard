#ifndef _AAC_MUXER_H
#define _AAC_MUXER_H

typedef struct
{
    uint16_t syncword;
    uint8_t id;
    uint8_t layer;
    uint8_t protection_absent;
    uint8_t profile;
    uint8_t sf_index;
    uint8_t private_bit;
    uint8_t channel_configuration;
    uint8_t original;
    uint8_t home;
    uint8_t emphasis;
    uint8_t copyright_identification_bit;
    uint8_t copyright_identification_start;
    uint16_t aac_frame_length;
    uint16_t adts_buffer_fullness;
    uint8_t no_raw_data_blocks_in_frame;
    uint16_t crc_check;

    /* control param */
    uint8_t old_format;
} adts_header;

#endif /* _AAC_MUXER_H */


