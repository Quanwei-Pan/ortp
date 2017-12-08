#include <ortp/ortp.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

static const char *help = "usage: rtpsend	filename dest_ip4addr dest_port\n";

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

int main(int argc, char *argv[]) {
	RtpSession *session;
	unsigned char buffer[36000];
	int i;
	FILE *infile;
	uint32_t user_ts = 0;
	struct timeval start_time, end_time;

	if (argc < 4) {
		printf("%s", help);
		return -1;
	}

	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_MESSAGE);
	session = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_enable_rtcp(session, FALSE);
	rtp_session_set_scheduling_mode(session, 1);
	rtp_session_set_blocking_mode(session, 1);
	rtp_session_set_connected_mode(session, TRUE);
	rtp_session_set_remote_addr(session, argv[2], atoi(argv[3]));
	rtp_session_set_payload_type(session, 32);

	infile = fopen(argv[1], "rb");
	if (infile == NULL) {
		perror("Cannot open file");
		return -1;
	}
	gettimeofday(&start_time,NULL);
	while ((i = fread(buffer, 1, 36000, infile)) > 0) {
		sysUsecTime();
		rtp_session_send_with_ts(session, buffer, i, user_ts);
		user_ts += 3600;
	}

	gettimeofday(&end_time,NULL);
	printf("init time is %f ms\n", ((end_time.tv_sec - start_time.tv_sec) * 1000000L + (end_time.tv_usec - start_time.tv_usec))/ 1000.0f);
	fclose(infile);
	rtp_session_destroy(session);
	ortp_exit();

	ortp_global_stats_display();

	return 0;
}