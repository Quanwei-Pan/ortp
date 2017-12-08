#include <ortp/ortp.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

int cond = 1;

void stop_handler(int signum) {
	cond = 0;
}

void sysUsecTime()
{
	struct timeval tv;
	struct timezone tz;
	struct tm *p;
	gettimeofday(&tv, &tz);
	p = localtime(&tv.tv_sec);
	printf("time_now:%d-%d-%dT%d:%d:%d.%ld\n", 1900+p->tm_year, 1+p->tm_mon, \
	p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec);
}

static char *help = "usage: rtprecv  filename loc_port\n";

int main(int argc, char *argv[]) {
	RtpSession *session;
	unsigned char buffer[36000];
	int err;
	uint32_t ts = 0;
	int stream_received = 0;
	FILE *outfile;
	int local_port;
	int have_more;
	int i;
	struct timeval start_time, end_time;
	/* init the lib */
	if (argc < 3) {
		printf("%s", help);
		return -1;
	}
	local_port = atoi(argv[2]);
	if (local_port <= 0) {
		printf("%s", help);
		return -1;
	}

	outfile = fopen(argv[1], "wb");
	if (outfile == NULL) {
		perror("Cannot open file for writing");
		return -1;
	}
	gettimeofday(&start_time,NULL);
	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_MESSAGE);
	signal(SIGINT, stop_handler);
	session = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_enable_rtcp(session, FALSE);
	rtp_session_set_scheduling_mode(session, 1);
	rtp_session_set_blocking_mode(session, 1);
	rtp_session_set_local_addr(session, "0.0.0.0", atoi(argv[2]), -1);
	rtp_session_set_connected_mode(session, TRUE);
	rtp_session_set_symmetric_rtp(session, TRUE);
	rtp_session_set_payload_type(session, 32);
	rtp_session_signal_connect(session, "ssrc_changed", (RtpCallback)rtp_session_reset, 0);
	gettimeofday(&end_time,NULL);
	printf("init time is %f ms\n", ((end_time.tv_sec - start_time.tv_sec) * 1000000L + (end_time.tv_usec - start_time.tv_usec))/ 1000.0f);
	while (cond) {
		have_more = 1;
		while (have_more) {
			err = rtp_session_recv_with_ts(session, buffer, 36000, ts, &have_more);
			if (err > 0)
				stream_received = 1;
			if ((stream_received) && (err > 0)) {
				printf("%d\n",err);
				size_t ret = fwrite(buffer, 1, err, outfile);
			}
		}
			ts += 2;
	}

	fclose(outfile);
	rtp_session_destroy(session);
	ortp_exit();
	ortp_global_stats_display();

	return 0;
}