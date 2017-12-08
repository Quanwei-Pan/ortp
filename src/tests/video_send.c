#include <ortp/ortp.h>
#include <string.h>

extern "C"{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
};

struct RtpSessionMgr
{
	RtpSession *rtp_session;
	uint32_t timestamp_inc;
	uint32_t cur_timestamp;
};

RtpSessionMgr rtp_session_mgr;

const char g_ip[] = "127.0.0.1";
const int g_port = 8008;
const uint32_t timestamp_inc = 3600; // 90000 / 25

const int image_width = 704;
const int image_height = 576;
const int frame_rate = 25;
static int frame_count, wrap_size;

AVCodecContext *video_cc;

AVFrame *picture;

/** 帧包头的标识长度 */ 
#define CMD_HEADER_LEN 10

/** 帧包头的定义 */
static uint8_t CMD_HEADER_STR[CMD_HEADER_LEN] = { 0xAA,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xFF };

/** 帧的包头信息 */
typedef struct _sFrameHeader
{
	/** 命令名称标识 */ 
	unsigned char cmdHeader[CMD_HEADER_LEN];

	/** 采集的通道号 0~7*/
	unsigned char chId;

	/** 数据类型，音频 或者 视频*/
	unsigned char dataType; 

	/** 缓冲区的数据长度 */ 
	uint32_t len;

	/** 时间戳 */
	uint32_t timestamp;

}FrameHeader;

// set frame header 
FrameHeader frameHeader;

void rtpInit()
{
	char *m_SSRC;

	ortp_init();
	ortp_scheduler_init();
	printf("Scheduler initialized\n");

	rtp_session_mgr.rtp_session = rtp_session_new(RTP_SESSION_SENDONLY);

	rtp_session_set_scheduling_mode(rtp_session_mgr.rtp_session, 1);
	rtp_session_set_blocking_mode(rtp_session_mgr.rtp_session, 1);
	rtp_session_set_remote_addr(rtp_session_mgr.rtp_session, g_ip, g_port);
	rtp_session_set_send_payload_type(rtp_session_mgr.rtp_session, 34); // 34 is for H.263 video frame

	m_SSRC	= getenv("SSRC");
	if (m_SSRC != NULL) 
	{
		rtp_session_set_ssrc(rtp_session_mgr.rtp_session, atoi(m_SSRC));
	}

	rtp_session_mgr.cur_timestamp = 0;
	rtp_session_mgr.timestamp_inc = timestamp_inc;

	printf("rtp init success!\n");
}

int rtpSend(unsigned char *send_buffer, int frame_len)
{
	FrameHeader *fHeader = (FrameHeader *)send_buffer;
	fHeader->chId      = 0;
	fHeader->dataType  = 0; // SESSION_TYPE_VIDEO
	fHeader->len       = frame_len;
	fHeader->timestamp = 0;

	printf("frame header len = %d\n", fHeader->len);

	int wrapLen;
	wrapLen = frame_len + sizeof(FrameHeader);

	int sended_bytes;
	sended_bytes = rtp_session_send_with_ts(rtp_session_mgr.rtp_session, 
											(uint8_t *)send_buffer, 
											wrapLen,
											rtp_session_mgr.cur_timestamp);

	rtp_session_mgr.cur_timestamp += rtp_session_mgr.timestamp_inc;

	return sended_bytes;
}

void createCodecContext()
{
	video_cc =  avcodec_alloc_context();
	if (!video_cc)
	{
		fprintf(stderr, "alloc avcodec context failed\n");
		exit(1);
	}

	video_cc->codec_id = (CodecID)CODEC_ID_H264;
	video_cc->codec_type = AVMEDIA_TYPE_VIDEO;

	video_cc->me_range = 16;  
	video_cc->max_qdiff = 4;  
	video_cc->qmin = 10;  
	video_cc->qmax = 51;  
	video_cc->qcompress = 0.6f;  

	/* put sample parameters */
	video_cc->bit_rate = 400000;

	/* resolution must be a multiple of two */
	video_cc->width = image_width;
	video_cc->height = image_height;

	/* time base: this is the fundamental unit of time (in seconds) in terms
	of which frame timestamps are represented. for fixed-fps content,
	timebase should be 1/framerate and timestamp increments should be
	identically 1. */
	video_cc->time_base.den = frame_rate;
	video_cc->time_base.num = 1;

	video_cc->gop_size = 12; /* emit one intra frame every twelve frames at most */
	video_cc->pix_fmt = PIX_FMT_YUV420P;
}

AVFrame *allocPicture(int pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;

	picture = avcodec_alloc_frame();
	if (!picture)
		return NULL;
	
	size = avpicture_get_size((PixelFormat)pix_fmt, width, height);
	picture_buf = (uint8_t *)av_malloc(size);
	if (!picture_buf) {
		av_free(picture);
		return NULL;
	}
	avpicture_fill((AVPicture *)picture, picture_buf, (PixelFormat)pix_fmt, width, height);
	return picture;
}

void openVideo()
{
	AVCodec *video_codec;

	/* find the video encoder */
	video_codec = avcodec_find_encoder(video_cc->codec_id);
	if (!video_codec) {
		fprintf(stderr, "codec not found\n");
		exit(1);
	}

	/* open the codec */
	if (avcodec_open(video_cc, video_codec) < 0) {
		fprintf(stderr, "could not open video codec\n");
		exit(1);
	}

	/* allocate the encoded raw picture */
	picture = allocPicture(video_cc->pix_fmt, video_cc->width, video_cc->height);
	if (!picture) {
		fprintf(stderr, "Could not allocate picture\n");
		exit(1);
	}
}

/* prepare a dummy image */
void fill_yuv_image(AVFrame *pict, int frame_index, int width, int height)
{
	int x, y, i;

	i = frame_index;

	/* Y */
	for(y=0;y<height;y++) {
		for(x=0;x<width;x++) {
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
		}
	}

	/* Cb and Cr */
	for(y=0;y<height/2;y++) {
		for(x=0;x<width/2;x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

void ffmpegInit()
{
	// initialize libavcodec, and register all codecs and formats
	av_register_all();

	// create a codec context
	createCodecContext();

	// open H.264 codec
	openVideo();
}

void getEncodedFrame(unsigned char *buffer, int& len)
{
	int out_size;

	fill_yuv_image(picture, frame_count, video_cc->width, video_cc->height);

	// encode the frame
	out_size = avcodec_encode_video(video_cc, buffer, wrap_size-sizeof(FrameHeader), picture);

	len = out_size;
	frame_count++;
}

int main()
{
	unsigned char *send_outbuf;
	unsigned char *video_part;

	frame_count = 0;
	wrap_size = 20000;
	send_outbuf = (unsigned char *)malloc(wrap_size);

	// copy cmdHeader to frameInfo
	memcpy(frameHeader.cmdHeader,CMD_HEADER_STR,CMD_HEADER_LEN);

	memcpy(send_outbuf, &frameHeader, sizeof(FrameHeader));
	video_part = send_outbuf + sizeof(FrameHeader);

	ffmpegInit();
	rtpInit();

	while (1)
	{
		int frame_len;
		// get encode frame
		getEncodedFrame(video_part, frame_len);

		printf("encodecFrame length is : %d\n", frame_len);

		if (frame_len > 0)
		{
			rtpSend(send_outbuf, frame_len);
		}
	}

	rtp_session_destroy(rtp_session_mgr.rtp_session);

	free(send_outbuf);

	// Give us some time
	Sleep(250);

	ortp_exit();
}