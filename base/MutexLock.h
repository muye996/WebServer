#pragma once
#include "nocopyable.h"
#include <pthread.h>
#include <cstdio>

class MutexLock:nocopyable
{
	friend class Condition;
public:
	MutexLock()
	{
		pthread_mutex_init(&mutex_,NULL);
	}
	~MutextLock()
	{
		pthread_mutex_lock(&mutex_);
		pthread_mutex_destroy(&mutex_);
	}

	void Lock()
	{
		pthread_mutex_lock(&mutex_);
	}
	void Unlock()
	{
		pthread_mutex_unlock(&mutex_);
	}
	pthread_mutex_t* Get()
	{
		return &mutex_;
	}
private:
	pthread_mutex_t mutex_;
};
