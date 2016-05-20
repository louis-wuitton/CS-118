#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#define MAX_PCK_LEN 1024
#define MAX_SEQ_NO 30720
#define INI_CONG_WIN_SIZE 1024
#define INI_SS_THRES 30720

int ret_timeout = 500000;

struct HeaderPacket
{
    uint16_t m_seq;
    uint16_t m_ack;
    uint16_t m_window;
    uint16_t m_flags;
    char payload[MAX_PCK_LEN];
};


#endif