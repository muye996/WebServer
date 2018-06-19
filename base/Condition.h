#pragma once
#include "nocopyable.h"
#include "MutexLock.h"
#include <pthread.h>
#include <errno.h>
#include <cstdint>
#include <time.h>

class Condition:nocopyable
{
public:
	explicit Condition(MutexLock &mutex)
		:mutex(mutex)
	{
		pthread_cond_init(&cond,NULL);
	}
	~Condition()
	{
		pthread_cond_destroy(&cond);
	}

	void CondWait()
	{
		pthread_cond_wait(&cond,mutex.Get());
	}
	void CondSignal()
	{
		pthread_cond_signal(&cond);
	}
	void CondBroadcast()
	{
		pthread_cond_Broadcast(&cond);
	}
	int CondTimeOut(int seconds)
	{
		struct timespec sp;
		clock_gettime(CLOCK_REALTIME,&sp);
		sp.tv_sec += static_cast<time_t>(seconds);
		return pthread_cond_timeout(&cond,mutex.Get(),&sp);
	}
private:
	MutexLock &mutex;
	pthread_cond_t cond;
};
