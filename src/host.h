/*
 * Copyright (c) 2016 Parallels IP Holdings GmbH
 * Copyright (c) 2017-2019 Virtuozzo International GmbH. All rights reserved.
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
 * Our contact details: Virtuozzo IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 */

#ifndef HOST_H
#define HOST_H

#include "ve.h"

namespace Rmond
{
namespace Host
{
enum PROPERTY
{
	LOCAL_VES = 101,
	LIMIT_VES,
	LICENSE_VES,
	LICENSE_CTS,
	LICENSE_VMS,
	LICENSE_CTS_USAGE,
	LICENSE_VMS_USAGE,

	DISKSTATS_IOS_IN_PROCESS,
	DISKSTATS_MS_DOING_OIS,
	INT_RES,
	MEMINFO_BUFFERS,
	MEMINFO_DIRTY,
	STAT_INTR_46,
	STAT_PROCESSES,
	STAT_PROCS_BLOCKED,
	STAT_PROCS_RUNNING,
	STAT_SOFTIRQ_NET_TX,
	STAT_SOFTIRQ_RCU,
	STAT_SOFTIRQ_SCHED,
};

} // namespace Host

///////////////////////////////////////////////////////////////////////////////
// struct Schema<Host::PROPERTY>

template<>
struct Schema<Host::PROPERTY>: mpl::vector<
			Declaration<Host::PROPERTY, Host::LOCAL_VES, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::LIMIT_VES, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::LICENSE_VES, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::LICENSE_CTS, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::LICENSE_VMS, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::LICENSE_CTS_USAGE, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::LICENSE_VMS_USAGE, ASN_INTEGER>,

			Declaration<Host::PROPERTY, Host::DISKSTATS_IOS_IN_PROCESS, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::DISKSTATS_MS_DOING_OIS, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::INT_RES, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::MEMINFO_BUFFERS, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::MEMINFO_DIRTY, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_INTR_46, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_PROCESSES, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_PROCS_BLOCKED, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_PROCS_RUNNING, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_SOFTIRQ_NET_TX, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_SOFTIRQ_RCU, ASN_INTEGER>,
			Declaration<Host::PROPERTY, Host::STAT_SOFTIRQ_SCHED, ASN_INTEGER> >

{
	static const char* name();
	static netsnmp_handler_registration* handler(Host::PROPERTY luid_,
					Netsnmp_Node_Handler* handler_, void* my_);
};

namespace Host
{
typedef Table::Tuple::Data<PROPERTY> tuple_type;
typedef boost::weak_ptr<tuple_type> tupleWP_type;
typedef boost::shared_ptr<tuple_type> tupleSP_type;
typedef boost::tuple<tupleSP_type> space_type;

///////////////////////////////////////////////////////////////////////////////
// struct Host

struct Unit: Environment
{
	Unit(PRL_HANDLE h_, const space_type& space_);
	~Unit();

	void pullState()
	{
		Environment::pullState();
	}
	void pullUsage();
	void ves(unsigned ves_);
	bool list(std::list<VE::UnitSP>& dst_, const VE::space_type& ves_) const;
	VE::UnitSP find(const std::string& id_, const VE::space_type& ves_) const;

	static bool inject(space_type& dst_);
private:
	tupleSP_type m_data;
};
typedef boost::shared_ptr<Unit> UnitSP;

} // namespace Host
} // namespace Rmond

#endif // HOST_H

