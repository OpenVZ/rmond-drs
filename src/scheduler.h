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

