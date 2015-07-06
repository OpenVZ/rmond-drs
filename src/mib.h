#ifndef MIB__H
#define MIB__H
#include <vector>
#include "scheduler.h"
#include <prlsdk/Parallels.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

typedef std::vector<oid> Oid_type;

#define TOKEN "RmondMIB"
#define TOKEN_PREFIX TOKEN":"
#define	LOG_PREFIX TOKEN_PREFIX" "

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

	static Oid_type traps();
	static Oid_type product();
	static SchedulerSP scheduler();
	static bool schedule(unsigned timeout_, Scheduler::Queue::job_type job_);
private:
	static Scheduler::UnitSP s_scheduler;
};

} // namespace Rmond

#endif // MIB__H

