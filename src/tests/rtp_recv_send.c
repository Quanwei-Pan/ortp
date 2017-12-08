#ifdef __cplusplus
extern "C" {
#endif
/*==================================================================================================

	Module Name: rtp_recv_send.c

	General Description: Use ortp-lib to recieve and send streams.

====================================================================================================

                               Xiaomi Confidential Proprietary
                        (c) Copyright Xiaomi 2017, All Rights Reserved


Revision History:
                            Modification
Author                          Date        Description of Changes
-------------------------   ------------    -------------------------------------------
Quanwei Pan                  11/15/2017     Initial version
====================================================================================================
                                        INCLUDE FILES
==================================================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include "rtp_recv_send.h"

/*==================================================================================================
										CONSTANTS
==================================================================================================*/
#define TS_INC              		(3600)
#define PAYLOAD_TYPE        		(32)
#define SEND_QUEUE_SIZE				(1024*1024) //1Mbyte

/*==================================================================================================
						Static variables / structure and other definations
==================================================================================================*/
send_session_t send_session_handler;
recv_session_t recv_session_handler;

pthread_t rtp_send_thread_id;
pthread_mutex_t rtp_send_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rtp_send_queue_lock = PTHREAD_MUTEX_INITIALIZER;
sem_t rtp_send_sem_signal;

pthread_t rtp_recv_thread_id;
pthread_mutex_t rtp_recv_thread_lock= PTHREAD_MUTEX_INITIALIZER;
sem_t rtp_recv_sem_signal;

/*==================================================================================================
                                      STATIC FUNCTIONS
==================================================================================================*/
static RTP_ReturnVal_t CreateQueue(PQUEUE Q, unsigned int maxsize)
{
	Q->pBase = (char *) malloc(maxsize);
	if (NULL == Q->pBase)
	{
		fprintf(stderr, "%s: Memory allocate failed for %d bytes \n", __func__, maxsize);
		return RTP_RETURN_ERR;
	}
	memset(Q->pBase, 0, maxsize);
	Q->read = 0;
	Q->write = 0;
	Q->maxsize = maxsize;
	return RTP_RETURN_OK;
}

static RTP_ReturnVal_t FreeQueue(QUEUE *Q)
{
	if (Q->pBase != NULL)
	{
		free(Q->pBase);
		Q->pBase = NULL;
	}
	return RTP_RETURN_OK;
}

static RTP_ReturnVal_t EnQueue(PQUEUE Q, char val)
{
	Q->pBase[Q->write] = val;
	Q->write = (Q->write + 1) % Q->maxsize;
	return RTP_RETURN_OK;
}

static RTP_ReturnVal_t DeQueue(PQUEUE Q, char *val)
{
	*val = Q->pBase[Q->read];
	Q->read = (Q->read + 1) % Q->maxsize;
	return RTP_RETURN_OK;
}

static int QueryStageQueue(PQUEUE Q)
{
	return (Q->write - Q->read + Q->maxsize) % Q->maxsize;
}

/*==================================================================================================
                                      GLOBAL FUNCTIONS
==================================================================================================*/
void sys_usec_time()
{
	struct timeval tv;
	struct timezone tz;
	struct tm *p;
	gettimeofday(&tv, &tz);
	p = localtime(&tv.tv_sec);
	printf("time_now:%d-%d-%dT%d:%d:%d.%ld\n", 1900+p->tm_year, 1+p->tm_mon, \
	p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec);
}

static RTP_ReturnVal_t rtp_send_thread_destory()
{
	void *status;
	int ret;
	if(rtp_send_thread_id != -1)
	{
		if((ret = pthread_cancel(rtp_send_thread_id)) != 0)
		{
			fprintf(stderr, "%s : pthread_cancel failed\n", __func__);
			return RTP_RETURN_ERR;
		}

		if((ret = pthread_join(rtp_send_thread_id, &status)) != 0)
		{
			fprintf(stderr, "%s : pthread_join failed\n", __func__);
			return RTP_RETURN_ERR;
		}
	}

	sem_destroy(&rtp_send_sem_signal);
	return RTP_RETURN_OK;
}

static void *rtp_send_thread(void *arg)
{
	bool status;
	int count;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);
	while(1)
	{	
		/* Wait for post signal */
		sem_wait(&rtp_send_sem_signal);
		pthread_mutex_lock(&rtp_send_thread_lock);
		status = send_session_handler.status;
		pthread_mutex_unlock(&rtp_send_thread_lock);
		if(status == true )
		{	
			/* Send data */
			while(1) 
			{
				for (int i = 0; i < send_session_handler.channels; i++) 
				{
					/* add the session to the set */
					session_set_set(send_session_handler.set, send_session_handler.session[i]);
				}

				/* suspend the process by selecting() */
				session_set_select(NULL, s_session->set, NULL);

				for (int i = 0; i < s_session->channels; i++) 
				{
					if (session_set_is_set(s_session->set, s_session->session[i])) 
					{
						rtp_session_send_with_ts(m_Session[k], m_pBuffer, nCounter, m_nUser_Timestamp);
					}
				}
					m_nUser_Timestamp += m_nTimestamp_Inc;
			}
			pthread_mutex_lock(&rtp_send_thread_lock);
			send_session_handler.status = FALSE;
			pthread_mutex_unlock(&rtp_send_thread_lock);
		}
		else
			continue;
	}
	rtp_send_thread_destory();
	rtp_send_thread_id = -1;
	pthread_exit(0);
	return NULL;
}


static RTP_ReturnVal_t rtp_send_thread_create()
{
	int ret;
	sem_init(&rtp_send_sem_signal, 0, 0);
	pthread_mutex_init(&rtp_send_thread_lock, NULL);
	ret = pthread_create((pthread_t *) &rtp_send_thread_id, NULL, \
		(void *) rtp_send_thread, &send_session_handler);
	if (ret != 0)
	{
		fprintf(stderr, "%s : create thread failed\n", __func__);
		return RTP_RETURN_ERR;
	}
	return RTP_RETURN_OK;
}


RTP_ReturnVal_t rtp_send_init(int channels, int ssrc, char ip[16], int port)
{
	int ret = 0;

    send_session_handler.ip = ip;
    send_session_handler.ssrc = ssrc;
    send_session_handler.port = port;
    send_session_handler.channels = channels;
    send_session_handler.ts = 0;
    send_session_handler.ts_inc = TS_INC;

	ortp_init();
	ortp_scheduler_init();
    
    /* initialize sessions */
    for (int i = 0; i < send_session_handler.channels; i++)
	{
		send_session_handler.session[i] = rtp_session_new(RTP_SESSION_SENDONLY);	
		rtp_session_set_scheduling_mode(send_session_handler.session[i], TRUE);
		rtp_session_set_blocking_mode(send_session_handler.session[i], FALSE);
        rtp_session_enable_rtcp(send_session_handler.session[i], FALSE);
		ret = rtp_session_set_remote_addr(send_session_handler.session[i], \
			send_session_handler.ip, send_session_handler.port);
        if(ret != 0)
        {
            printf(stderr,"Cannot connect to remote host, please check the ip address and port!\n");
            return RTP_RETURN_ERR;
        }
		rtp_session_set_send_payload_type(send_session_handler.session[i], PAYLOAD_TYPE);
        rtp_session_set_ssrc(send_session_handler.session[i], ssrc);
		send_session_handler.port += 2;
	}

	/* create a set */
	send_session_handler.set = session_set_new();

	CreateQueue(send_session_handler.queue, SEND_QUEUE_SIZE);

	rtp_send_thread_create();

    return RTP_RETURN_OK;
}

RTP_ReturnVal_t rtp_recv_init(pRecv_session_t r_session, int channels, int port)
{
	int ret = 0;

	r_session->status = TRUE;
    r_session->ip = "0.0.0.0";
    r_session->port = port;
    r_session->channels = channels;
    r_session->ts = 0;
    r_session->ts_inc = TS_INC;

	ortp_init();
	ortp_scheduler_init();

    
    /* initialize sessions */
    for (int i = 0; i < r_session->channels; i++)
	{
		r_session->session[i]=rtp_session_new(RTP_SESSION_RECVONLY);	
		rtp_session_set_scheduling_mode(r_session->session[i], TRUE);
		rtp_session_set_blocking_mode(r_session->session[i], FALSE);
        rtp_session_enable_rtcp(r_session->session[i], FALSE);
		ret = rtp_session_set_local_addr(r_session->session[i], r_session->ip, r_session->port, r_session->port + 1);
        if(ret != 0)
        {
            fprintf(stderr,"Cannot set local host, please check the ip address and port!\n");
            return RTP_RETURN_ERR;
        }
		rtp_session_set_send_payload_type(r_session->session[i], PAYLOAD_TYPE);
		rtp_session_enable_adaptive_jitter_compensation(r_session->session[i], TRUE);
		rtp_session_set_recv_buf_size(r_session->session[i], 256);
		r_session->port += 2;
	}

	/* create a set */
	r_session->set = session_set_new();



    return RTP_RETURN_OK;
}

void rtp_close(void)
{
	assert(send_session_handler.status);
	if()
    for (int i = 0; i < s_session->channels; i++) 
    {
		rtp_session_destroy(s_session->session[i]);
	}

	session_set_destroy(s_session->set);

    ortp_exit();
	ortp_global_stats_display();
}

RTP_ReturnVal_t rtp_recv_destory(pRecv_session_t r_session)
{
    for (int i = 0; i < r_session->channels; i++) 
    {
		rtp_session_destroy(r_session->session[i]);
	}

	session_set_destroy(r_session->set);

    ortp_exit();
	ortp_global_stats_display();

    return RTP_RETURN_OK;
}

RTP_ReturnVal_t rtp_send_data(char *buffer, int send_size)
{
	bool status;

	pthread_mutex_lock(&rtp_send_thread_lock);
	send_session_handler.send_size = send_size;
	send_session_handler.status = TRUE;
	pthread_mutex_unlock(&rtp_send_thread_lock);

	for(int i= 0; i < send_size; i++)
	{
		pthread_mutex_lock(&rtp_send_queue_lock);
		EnQueue(send_session_handler.queue++, *(buffer++));
		pthread_mutex_unlock(&rtp_send_queue_lock);
	}
	
	/* Wake up send thread */
	sem_post(&rtp_send_sem_signal);	
	
	/* Wait for sending ends */
	usleep(500);
	while(1)
	{
		pthread_mutex_lock(&rtp_send_thread_lock);
		status =  send_session_handler.status;
		pthread_mutex_unlock(&rtp_send_thread_lock);
		if(status == FALSE)
			break;
		else
			usleep(1000);
	}

	return  RTP_RETURN_OK;
}

int rtp_receive_data()
{

}

#ifdef __cplusplus
}
#endif
