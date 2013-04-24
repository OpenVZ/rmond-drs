#include "system.h"

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Sdk

std::string Sdk::getString(const boost::function2<PRL_RESULT, PRL_STR, PRL_UINT32*>& sdk_)
{
	PRL_UINT32 cb = 0;
	PRL_RESULT e = sdk_(0, &cb);
	if (PRL_FAILED(e))
		return std::string();

	if (2 > cb)
		return std::string();

	std::string output(cb - 1, 0);
	e = sdk_(&output[0], &cb);
	if (PRL_FAILED(e))
		return std::string();

	return output;
}

PRL_HANDLE Sdk::getAsyncResult(PRL_HANDLE job_)
{
	if (PRL_INVALID_HANDLE == job_)
		return PRL_INVALID_HANDLE;

	PRL_HANDLE output = PRL_INVALID_HANDLE;
	PRL_RESULT e = PrlJob_Wait(job_, UINT_MAX);
	if (PRL_SUCCEEDED(e))
	{
		PRL_HANDLE r;
		e = PrlJob_GetResult(job_, &r);
		if (PRL_SUCCEEDED(e))
		{
			e = PrlResult_GetParam(r, &output);
			PrlHandle_Free(r);
		}
	}
	PrlHandle_Free(job_);
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Lock

Lock::Lock(pthread_mutex_t& mutex_): m_mutex(&mutex_)
{
	if (enter())
		m_mutex = NULL;
}

bool Lock::enter()
{
	if (NULL == m_mutex)
		return true;

	int e = pthread_mutex_lock(m_mutex);
	if (0 != e)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot enter the critical section: 0x%x\n", e);
		return true;
	}
	return false;
}

bool Lock::leave()
{
	if (NULL == m_mutex)
		return true;

	int e = pthread_mutex_unlock(m_mutex);
	if (0 != e)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot leave the critical section: 0x%x\n", e);
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// struct ConditionalVariable

ConditionalVariable::ConditionalVariable()
{
	pthread_cond_init(&m_impl, NULL);
}

ConditionalVariable::~ConditionalVariable()
{
	int e = pthread_cond_destroy(&m_impl);
	if (0 != e)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot destroy the conditional "
					"variable: 0x%x\n", e);
	}
}

bool ConditionalVariable::wait(pthread_mutex_t& mutex_)
{
	int e = pthread_cond_wait(&m_impl, &mutex_);
	if (0 != e)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot wait for the conditional "
					"variable: 0x%x\n", e);
		return true;
	}
	return false;
}

bool ConditionalVariable::wait(pthread_mutex_t& mutex_, const boost::system_time& barrier_)
{
	struct timespec b = {0, 0};
	boost::posix_time::time_duration const u = barrier_ - boost::posix_time::from_time_t(0);
	b.tv_sec = u.total_seconds();
	b.tv_nsec = (long)(u.fractional_seconds()*(1000000000l/u.ticks_per_second()));
	int e = pthread_cond_timedwait(&m_impl, &mutex_, &b);
	if (ETIMEDOUT == e)
		return false;
	if (0 != e)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot time wait for the conditional "
					"variable: 0x%x\n", e);
	}
	return true;
}

void ConditionalVariable::signal()
{
	pthread_cond_broadcast(&m_impl);
}

} // namespace Rmond

