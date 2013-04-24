#ifndef RMOND_H
#define RMOND_H

#include "value.h"
#include "scheduler.h"

namespace Rmond
{
struct Server;
typedef boost::weak_ptr<Server> ServerWP;
typedef boost::shared_ptr<Server> ServerSP;

///////////////////////////////////////////////////////////////////////////////
// struct Central

struct Central
{
	static bool init();
	static void fini();

	static SchedulerSP scheduler();
private:
	static SchedulerSP s_scheduler;
};

} // namespace Rmond

#endif // RMOND_H

