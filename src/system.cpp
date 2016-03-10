/*
 * Copyright (c) 2016 Parallels IP Holdings GmbH
 *
 * This file is part of OpenVZ. OpenVZ is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#include "system.h"
#include <boost/bind.hpp>

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
	PRL_RESULT e = PrlJob_Wait(job_, TIMEOUT);
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

std::string Sdk::getIssuerId(PRL_HANDLE event_)
{
	return getString(boost::bind(&PrlEvent_GetIssuerId, event_, _1, _2));
}

///////////////////////////////////////////////////////////////////////////////
// struct Lock

Lock::Lock(pthread_mutex_t& mutex_): m_idle(true), m_mutex(&mutex_)
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
	return m_idle = false;
}

bool Lock::leave()
{
	if (NULL == m_mutex)
		return true;

	if (!m_idle)
	{
		int e = pthread_mutex_unlock(m_mutex);
		if (0 != e)
		{
			snmp_log(LOG_ERR, LOG_PREFIX"cannot leave the critical section: 0x%x\n", e);
			return true;
		}
		m_idle = true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// struct ConditionalVariable

ConditionalVariable::ConditionalVariable()
{
	pthread_condattr_t a;
	pthread_condattr_init(&a);
	int e = pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
	if (0 != e)
		snmp_log(LOG_ERR, LOG_PREFIX"cannot set the clock: 0x%x\n", e);

	pthread_cond_init(&m_impl, &a);
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

bool ConditionalVariable::wait(pthread_mutex_t& mutex_, timespec barrier_)
{
	int e = pthread_cond_timedwait(&m_impl, &mutex_, &barrier_);
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

