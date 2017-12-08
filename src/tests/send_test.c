
#include <ortp/ortp.h>
#include <signal.h>
#include <stdlib.h>


int runcond=1;

void stophandler(int signum)
{
	runcond=0;
}

static const char *help="usage: rtpsend	filename dest_ip4addr dest_port\n";

int main(int argc, char *argv[])
{
	RtpSession *session;
	unsigned char buffer[160];
	int i;
	FILE *infile;
	char *ssrc;
	uint32_t user_ts=0;
	
	if (argc<4){
		printf("%s", help);
		return -1;
	}
	
	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR);	
	session=rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_enable_rtcp(session,FALSE);
	rtp_session_set_scheduling_mode(session,1);
	rtp_session_set_blocking_mode(session,1);
	rtp_session_set_connected_mode(session,TRUE);
	rtp_session_set_remote_addr(session,argv[2],atoi(argv[3]));
	rtp_session_set_payload_type(session,0);
	
	ssrc=getenv("SSRC");
	if (ssrc!=NULL) {
		printf("using SSRC=%i.\n",atoi(ssrc));
		rtp_session_set_ssrc(session,atoi(ssrc));
	}

	infile=fopen(argv[1],"rb");
	if (infile==NULL) {
		perror("Cannot open file");
		return -1;
	}

	signal(SIGINT,stophandler);
	while( ((i=fread(buffer,1,160,infile))>0) && (runcond) )
	{
		rtp_session_send_with_ts(session,buffer,i,user_ts);
		user_ts+=160;
	}

	fclose(infile);
	rtp_session_destroy(session);
	ortp_exit();
	ortp_global_stats_display();

	return 0;
}