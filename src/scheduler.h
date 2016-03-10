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

#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <pthread.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition.hpp>

namespace Rmond
{
namespace Scheduler
{
///////////////////////////////////////////////////////////////////////////////
// struct Queue

struct Queue
{
	typedef boost::function0<void> job_type;

	virtual ~Queue();

	bool push(const job_type& job_)
	{
		return push(0, job_);
	}
	virtual bool push(unsigned when_, const job_type& job_) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// struct State

struct State;

///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit: Queue
{
	Unit();
	~Unit();

	bool go();
	void stop();
	bool push(unsigned when_, const job_type& job_);
	using Queue::push;
private:
	static void* consume(void* argv_);

	boost::shared_ptr<State> m_state;
	pthread_t m_consumer;

	static pthread_mutex_t s_mutex;
};
typedef boost::shared_ptr<Unit> UnitSP;

} // namespace Scheduler

typedef boost::shared_ptr<Scheduler::Queue> SchedulerSP;

} // namespace Rmond

#endif // SCHEDULER_H

