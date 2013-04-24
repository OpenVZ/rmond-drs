#include "mib.h"
#include "system.h"
#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>

namespace Rmond
{
namespace Scheduler
{
typedef boost::tuple<boost::weak_ptr<State> > argv_type;

///////////////////////////////////////////////////////////////////////////////
// struct Queue

Queue::~Queue()
{
}

///////////////////////////////////////////////////////////////////////////////
// struct State

struct State
{
	typedef std::multimap<boost::system_time, Queue::job_type> queue_type;

	queue_type queue;
	ConditionalVariable condvar;
};

///////////////////////////////////////////////////////////////////////////////
// struct Unit

pthread_mutex_t Unit::s_mutex = PTHREAD_MUTEX_INITIALIZER;

Unit::Unit(): m_consumer(0)
{
}

Unit::~Unit()
{
	stop();
}

bool Unit::go()
{
	Lock g(s_mutex);
	if (0 != m_consumer)
		return true;

	boost::shared_ptr<State> a(new State);
	std::auto_ptr<argv_type> v(new argv_type);
	v->get<0>() = a;
	int e = pthread_create(&m_consumer, NULL, &Unit::consume, v.get());
	if (0 != e)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot start the scheduler thread: 0x%x\n", e);
		return true;
	}
	m_state = a;
	v.release();
	return false;
}

void Unit::stop()
{
	Lock g(s_mutex);
	if (0 == m_consumer)
		return;

	pthread_t x = m_consumer;
	m_consumer = 0;
	if (pthread_self() == x)
	{
		m_state.reset();
		pthread_detach(x);
	}
	else
	{
		m_state->condvar.signal();
		m_state.reset();
		g.leave();
		pthread_join(x, NULL);
	}
}

void* Unit::consume(void* argv_)
{
	argv_type* v = (argv_type* )argv_;
	boost::weak_ptr<State> w = v->get<0>();
	delete v;
	while (true)
	{
		boost::shared_ptr<State> z = w.lock();
		if (NULL == z.get())
			break;

		Lock g(s_mutex);
		if (z.unique())
			break;
		if (z->queue.empty())
		{
			z->condvar.wait(s_mutex);
			continue;
		}
		State::queue_type::iterator p = z->queue.begin();
		State::queue_type::value_type x = *p;
		z->queue.erase(p);
		if (z->condvar.wait(s_mutex, x.first))
			z->queue.insert(x);
		else if (z.unique())
			break;
		else
		{
			g.leave();
			x.second();
			g.enter();
		}
	}
	pthread_exit(NULL);
}

bool Unit::push(unsigned when_, const job_type& job_)
{
	Lock g(s_mutex);
	if (0 == m_consumer)
		return true;

	boost::system_time w = boost::get_system_time() + boost::posix_time::seconds(when_);
	m_state->queue.insert(std::make_pair(w, job_));
	m_state->condvar.signal();
	return false;
}

} // namespace Scheduler
} // namespace Rmond

