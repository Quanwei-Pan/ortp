#include <string.h>
#include "ortp/ortp.h"
#include <stdbool.h>
#include <signal.h>

bool m_bExit = FALSE;

typedef struct 
{
	RtpSession *rtp_session;
	int timestamp;
}RtpSessionMgr;


RtpSessionMgr rtp_session_mgr;
const int timestamp_inc = 3600;

const char recv_ip[] = "0.0.0.0";
const int recv_port = 5647;
const int recv_bufsize = 40000;
unsigned char *recv_buf;

typedef struct _sFrameHeader
{
	uint32_t len;
	uint32_t timestamp;
}FrameHeader;

int cond = 1;

void stop_handler(int signum) {
	cond = 0;
}

void rtpInit()
{
	int ret;

	ortp_init();
	ortp_scheduler_init();

	rtp_session_mgr.rtp_session = rtp_session_new(RTP_SESSION_RECVONLY);

	rtp_session_set_scheduling_mode(rtp_session_mgr.rtp_session, TRUE);
	rtp_session_set_blocking_mode(rtp_session_mgr.rtp_session, TRUE);
	rtp_session_set_local_addr(rtp_session_mgr.rtp_session, recv_ip, recv_port, -1);
    rtp_session_enable_rtcp(rtp_session_mgr.rtp_session, FALSE);
	rtp_session_enable_adaptive_jitter_compensation(rtp_session_mgr.rtp_session, TRUE);
	rtp_session_set_jitter_compensation(rtp_session_mgr.rtp_session, 40);
	rtp_session_set_payload_type(rtp_session_mgr.rtp_session, 32);
	rtp_session_set_recv_buf_size(rtp_session_mgr.rtp_session, 20 * 1024 * 1024);

	rtp_session_mgr.timestamp = timestamp_inc;
}

int rtp2disk(FILE *fp)
{
	int err;
	int havemore = 1;

	while (havemore)
	{
		err = rtp_session_recv_with_ts(rtp_session_mgr.rtp_session, (uint8_t *)recv_buf, recv_bufsize, rtp_session_mgr.timestamp, &havemore);
		if (havemore) 
			printf("==> Warning: havemore=1!\n");

		if (err > 0)
		{
			fwrite(recv_buf, 1, err, fp);
            printf("read size is %d \n",err);
		}
        rtp_session_mgr.timestamp += timestamp_inc;
        
	}
	return 0;
}

int main(int argc, char *argv[])
{
    FILE *outfile;
	recv_buf = (uint8_t *)malloc(recv_bufsize);
    signal(SIGINT, stop_handler);
	rtpInit();
    outfile = fopen(argv[1], "wb");
	if (outfile == NULL) {
		perror("Cannot open file for writing");
		return -1;
	}

	printf("==> RTP Receiver started\n");

	while (cond)
	{
		rtp2disk(outfile);
	}

	printf("==> Exiting\n");

	free(recv_buf);
    fclose(outfile);
	rtp_session_destroy(rtp_session_mgr.rtp_session);
	ortp_exit();
}