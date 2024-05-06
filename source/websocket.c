
#include "mx/websocket.h"

#include "mx/base64.h"
#include "mx/sha1.h"
#include "mx/misc.h"

#define _GNU_SOURCE
#include <string.h>


#define SHA1_HASH_SIZE      20





/**
 * Apply websocket mask
 *
 */
void ws_apply_mask(unsigned char *data, size_t length, const unsigned char *mask)
{
    for (size_t i = 0; i < length; i++)
        data[i] = data[i] ^ mask[i%WS_MASK_SIZE];
}


/**
 * Perform meta-analysis of the data and find websocket frame
 *
 */
bool ws_parse_frame(struct ws_frame *frame, const unsigned char *data, size_t length, size_t *frame_length)
{
    size_t expected_frame_lenght = WS_MIN_HEADER_SIZE;

    if (length < expected_frame_lenght) {
        if (frame_length)
            *frame_length = expected_frame_lenght;
        return false;   // Wait for basic header
    }

    frame->mask_offset = WS_MIN_HEADER_SIZE;
    frame->payload_offset = WS_MIN_HEADER_SIZE;

    frame->fin_flag = (data[WS_FIN_OPCODE_IDX] & WS_FIN_FLAG) == WS_FIN_FLAG ? 1 : 0;
    frame->mask_flag = (data[WS_MASK_LEN_IDX] & WS_MASK_FLAG) == WS_MASK_FLAG ? 1 : 0;
    if (frame->mask_flag) {
        expected_frame_lenght += WS_MASK_SIZE;
        frame->payload_offset += WS_MASK_SIZE;
    }

    frame->opcode = data[WS_FIN_OPCODE_IDX] & WS_OPCODE_MSK;
    frame->payload_length = data[WS_MASK_LEN_IDX] & WS_PAYLOAD_LEN_MSK;
    if (frame->payload_length == WS_EXTLEN16_MARK) {
        expected_frame_lenght += WS_EXTLEN16_SIZE;
        frame->payload_offset += WS_EXTLEN16_SIZE;
        frame->mask_offset += WS_EXTLEN16_SIZE;
    }
    else if (frame->payload_length == WS_EXTLEN64_MARK) {
        expected_frame_lenght += WS_EXTLEN64_SIZE;
        frame->payload_offset += WS_EXTLEN64_SIZE;
        frame->mask_offset += WS_EXTLEN64_SIZE;
    }

    if (length < expected_frame_lenght) {
        if (frame_length)
            *frame_length = expected_frame_lenght;
        return false;   // Wait for full header
    }

    if (frame->payload_length == WS_EXTLEN16_MARK) {
        frame->payload_length = ( (size_t)data[WS_EXTLEN_IDX  ] << 8 |
                                  (size_t)data[WS_EXTLEN_IDX+1] );
    }
    else if (frame->payload_length == WS_EXTLEN64_MARK) {
        frame->payload_length = ( (size_t)data[WS_EXTLEN_IDX  ] << 56 |
                                  (size_t)data[WS_EXTLEN_IDX+1] << 48 |
                                  (size_t)data[WS_EXTLEN_IDX+2] << 40 |
                                  (size_t)data[WS_EXTLEN_IDX+3] << 32 |
                                  (size_t)data[WS_EXTLEN_IDX+4] << 24 |
                                  (size_t)data[WS_EXTLEN_IDX+5] << 16 |
                                  (size_t)data[WS_EXTLEN_IDX+6] <<  8 |
                                  (size_t)data[WS_EXTLEN_IDX+7] );
    }

    expected_frame_lenght += frame->payload_length;
    if (frame_length)
        *frame_length = expected_frame_lenght;

    if (length < expected_frame_lenght) {
        return false;   // Wait for full message
    }

    return true;
}


/**
 * Format websocket frame
 *
 * Buffer should be WS_MAX_HEADER_SIZE longer than data length
 *
 */
size_t ws_format_frame(unsigned char *buffer, unsigned char flags, const unsigned char *mask, const unsigned char *data, size_t length)
{
    unsigned char mask_flag = mask ? WS_MASK_FLAG : 0;
    size_t frame_offset = WS_MIN_HEADER_SIZE;

    buffer[WS_FIN_OPCODE_IDX] = flags;

    if (length > 0xFFFF) {
        buffer[WS_MASK_LEN_IDX] = WS_EXTLEN64_MARK | mask_flag;
        buffer[WS_EXTLEN_IDX  ] = (unsigned char)((length >> 56) & 0xFF);
        buffer[WS_EXTLEN_IDX+1] = (unsigned char)((length >> 48) & 0xFF);
        buffer[WS_EXTLEN_IDX+2] = (unsigned char)((length >> 40) & 0xFF);
        buffer[WS_EXTLEN_IDX+3] = (unsigned char)((length >> 32) & 0xFF);
        buffer[WS_EXTLEN_IDX+4] = (unsigned char)((length >> 24) & 0xFF);
        buffer[WS_EXTLEN_IDX+5] = (unsigned char)((length >> 16) & 0xFF);
        buffer[WS_EXTLEN_IDX+6] = (unsigned char)((length >>  8) & 0xFF);
        buffer[WS_EXTLEN_IDX+7] = (unsigned char)((length      ) & 0xFF);
        frame_offset += WS_EXTLEN64_SIZE;
    }
    else if (length >= 126) {
        buffer[WS_MASK_LEN_IDX] = WS_EXTLEN16_MARK | mask_flag;
        buffer[WS_EXTLEN_IDX  ] = (unsigned char)((length >>  8) & 0xFF);
        buffer[WS_EXTLEN_IDX+1] = (unsigned char)((length      ) & 0xFF);
        frame_offset += WS_EXTLEN16_SIZE;
    }
    else {
        buffer[WS_MASK_LEN_IDX] = length | mask_flag;
        // No additional offset
    }

    if (mask) {
        memcpy(buffer + frame_offset, mask, WS_MASK_SIZE);
        frame_offset += WS_MASK_SIZE;
    }

    if (length) {
        memcpy(buffer + frame_offset, data, length);
        if (mask)
            ws_apply_mask(buffer + frame_offset, length, mask);
    }

    return frame_offset + length;
}


/**
 * Calculate websocket accept key
 *
 */
bool ws_calculate_accept_key(char *reqkey, size_t reqkey_len, char *buffer, size_t length)
{
    // Received HTTP request
    const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // Concatenate key and Websocket GUID
    char keyguid[reqkey_len + strlen(GUID) + 1];
    memset(keyguid, 0, sizeof(keyguid));
    strncat(keyguid, reqkey, reqkey_len);
    strcat(keyguid, GUID);

    // Calculate SHA1 of the string
    unsigned char hash[SHA1_HASH_SIZE];
    SHA1((char*)hash, keyguid, strlen(keyguid));

    // Encode hash
    if (base64_encode(hash, sizeof(hash), (unsigned char*)buffer, length, false) == 0)
        return false;

    return true;
}
