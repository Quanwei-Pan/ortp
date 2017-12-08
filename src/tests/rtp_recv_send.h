#ifndef _RRP_RECV_SEND_H
#define _RRP_RECV_SEND_H

#include <ortp/ortp.h>

#ifdef __cplusplus
extern "C"{
#endif

#define STREAMS_COUNT              (1000)

typedef enum{
    RTP_RETURN_OK = 0,
    RTP_RETURN_ERR =1,
} RTP_ReturnVal_t;

typedef struct
{
	char *pBase;
	int read;
	int write;
	unsigned int maxsize;
}QUEUE, *PQUEUE;

typedef struct{
    RtpSession *session[STREAMS_COUNT];
	int ts;
    int ts_inc;
    int port;
    char *ip;
    int channels;
    int ssrc;
    SessionSet *set;
    bool status;
    char *buffer;
    PQUEUE queue;
    int send_size;
}send_session_t, *pSend_session_t;

typedef struct{
    RtpSession *session[STREAMS_COUNT];
	int ts;
    int ts_inc;
    int port;
    char *ip;
    int channels;
    SessionSet *set;
    bool status;
}recv_session_t, *pRecv_session_t;


/*
* @brief : init rtp send handler
*/
RTP_ReturnVal_t rtp_send_init();

/*
* @brief : use rtp send session to send data 
*/
RTP_ReturnVal_t rtp_send_data(char *buffer, int send_size);

/*
*@ brief : init rtp receive handler
*/
RTP_ReturnVal_t rtp_recv_init();

/*
* @brief : use rtp receive session to receive data 
*/
RTP_ReturnVal_t rtp_recv_data();

/*
* @brief : exit rtp session
*/
void rtp_close();

#ifdef __cplusplus
}
#endif

#endif  /* _RRP_RECV_SEND_H */