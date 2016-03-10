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

