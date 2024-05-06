
#ifndef __MX_WEBSOCKET_H_
#define __MX_WEBSOCKET_H_


#include <stddef.h>
#include <stdbool.h>


#define WS_FIN_OPCODE_IDX       0
#define WS_MASK_LEN_IDX         1
#define WS_EXTLEN_IDX           2


#define WS_FLAGS_SIZE           2
#define WS_MASK_SIZE            4
#define WS_EXTLEN16_SIZE        2
#define WS_EXTLEN64_SIZE        8
#define WS_MIN_HEADER_SIZE      (WS_FLAGS_SIZE)
#define WS_MAX_HEADER_SIZE      (WS_FLAGS_SIZE + WS_EXTLEN64_SIZE + WS_MASK_SIZE)

#define WS_EXTLEN16_MARK        126
#define WS_EXTLEN64_MARK        127

#define WS_FIN_FLAG             0x80
#define WS_MASK_FLAG            0x80
#define WS_OPCODE_MSK           0x0F
#define WS_PAYLOAD_LEN_MSK      0x7F

#define WS_OPCODE_CONTINUE      0x0
#define WS_OPCODE_TEXT          0x1
#define WS_OPCODE_BINARY        0x2
#define WS_OPCODE_CLOSE         0x8
#define WS_OPCODE_PING          0x9
#define WS_OPCODE_PONG          0xA

#define WS_CLOSE_NORMAL             1000
#define WS_CLOSE_GOING_AWAY         1001
#define WS_CLOSE_PROTOCOL_ERROR     1002
#define WS_CLOSE_NOT_ALLOWED        1003
#define WS_CLOSE_RESERVED           1004
#define WS_CLOSE_NO_CODE            1005
#define WS_CLOSE_DIRTY              1006
#define WS_CLOSE_WRONG_TYPE         1007
#define WS_CLOSE_POLICY_VIOLATION   1008
#define WS_CLOSE_MESSAGE_TOO_BIG    1009
#define WS_CLOSE_UNEXPECTED_ERROR   1011







struct ws_frame
{
    unsigned char fin_flag;
    unsigned char opcode;
    unsigned char mask_flag;
    size_t payload_length;

    unsigned int mask_offset;
    unsigned int payload_offset;
};



void ws_apply_mask(unsigned char *data, size_t length, const unsigned char *mask);

bool ws_parse_frame(struct ws_frame *frame, const unsigned char *data, size_t length, size_t *frame_length);
size_t ws_format_frame(unsigned char *buffer, unsigned char flags, const unsigned char *mask, const unsigned char *data, size_t length);

bool ws_calculate_accept_key(char *reqkey, size_t reqkey_len, char *buffer, size_t length);


#endif /* __MX_WEBSOCKET_H_ */
