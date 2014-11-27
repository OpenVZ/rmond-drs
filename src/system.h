#ifndef SYSTEM_H
#define SYSTEM_H
#include "mib.h"

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Sdk

struct Sdk
{
	enum
	{
		TIMEOUT = 15000
	};
	static std::string getIssuerId(PRL_HANDLE event_);
	static std::string getString(const boost::function2<PRL_RESULT, PRL_STR, PRL_UINT32*>& sdk_);
	static PRL_HANDLE getAsyncResult(PRL_HANDLE job_);
};

///////////////////////////////////////////////////////////////////////////////
// struct Lock

struct Lock
{
	explicit Lock(pthread_mutex_t& mutex_);
	~Lock()
	{
		leave();
	}

	bool enter();
	bool leave();
private:
	bool m_idle;
	pthread_mutex_t* m_mutex;
};

///////////////////////////////////////////////////////////////////////////////
// struct ConditionalVariable

struct ConditionalVariable: boost::noncopyable
{
	ConditionalVariable();
	~ConditionalVariable();

	bool wait(pthread_mutex_t& mutex_);
	bool wait(pthread_mutex_t& mutex_, timespec barrier_);
	void signal();
private:
	pthread_cond_t m_impl;
};

} // namespace Rmond

#endif // SYSTEM_H

