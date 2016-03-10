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

