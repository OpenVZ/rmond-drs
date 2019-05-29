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

#include "host.h"
#include "system.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <sstream>

extern netsnmp_session* main_session;
extern "C"
{
int handle_pdu(netsnmp_agent_session* );
};

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Schema<Host::PROPERTY>

const char* Schema<Host::PROPERTY>::name()
{
	return TOKEN_PREFIX"host";
}

netsnmp_handler_registration* Schema<Host::PROPERTY>::handler(Host::PROPERTY luid_,
				Netsnmp_Node_Handler* handler_, void* my_)
{
	netsnmp_mib_handler* h = netsnmp_create_handler(name(), handler_);
	if (NULL == h)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"error creating handler for %s.%d\n",
				name(), luid_);
		return NULL;
	}
	h->myvoid = my_;
	Oid_type u = Schema<void>::uuid(luid_);
	netsnmp_handler_registration* r = 
		netsnmp_handler_registration_create(name(), h, &u[0], u.size(), HANDLER_CAN_RONLY);
	if (NULL != r)
		return r;

	netsnmp_handler_free(h);
	snmp_log(LOG_ERR, LOG_PREFIX"error creating handler registration for %s.%d\n",
			name(), luid_);
	return NULL;
}

namespace Host
{
namespace
{
VE::Unit* make(PRL_HANDLE h_, const VE::space_type& ves_)
{
	std::string u;
	PRL_VM_TYPE t = PVT_VM;
	PRL_RESULT r = PrlVmCfg_GetVmType(h_, &t);
	if (PRL_SUCCEEDED(r))
	{
		switch (t)
		{
		case PVT_VM:
			u = Sdk::getString(boost::bind(&PrlVmCfg_GetUuid, h_, _1, _2));
			break;
		case PVT_CT:
			u = Sdk::getString(boost::bind(&PrlVmCfg_GetCtId, h_, _1, _2));
			break;
		default:
			break;
		}
	}
	if (u.empty())
	{
		PrlHandle_Free(h_);
		return NULL;
	}
	VE::table_type::key_type k;
	k.put<VE::VEID>(u);
	return new VE::Unit(h_, k, ves_);
}

///////////////////////////////////////////////////////////////////////////////
// struct Proxy

struct Proxy: Value::Composite::Base
{
	explicit Proxy(const Oid_type& name_);

	Value::Provider* snapshot(const Value::Metrix_type& metrix_) const;
private:
	Oid_type m_name;
};

Proxy::Proxy(const Oid_type& name_): m_name(name_)
{
}

Value::Provider* Proxy::snapshot(const Value::Metrix_type& metrix_) const
{
	if (!metrix_.empty() && metrix_.count(m_name) == 0)
		return NULL;

	netsnmp_pdu* u = snmp_pdu_create(SNMP_MSG_GET);
	if (NULL == u)
		return NULL;

	Value::Provider* output = NULL;
	snmp_add_null_var(u, &m_name[0], m_name.size());
	u->flags |= UCD_MSG_FLAG_ALWAYS_IN_VIEW;
	netsnmp_agent_session* s = init_agent_snmp_session(main_session, u);
	snmp_free_pdu(u);
	int e = handle_pdu(s);
	if (SNMP_ERR_NOERROR == e)
	{
		output = new Value::Named(m_name, s->pdu->variables);
		s->pdu->variables = NULL;
	}
	free_agent_snmp_session(s);
	return output;
}

} // namespace

namespace Scalar
{
///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit
{
	explicit Unit(tupleSP_type data_): m_data(data_)
	{
	}

	template<class T, T N>
	static int handle(netsnmp_mib_handler* handler_, netsnmp_handler_registration* ,
		netsnmp_agent_request_info* info_, netsnmp_request_info* requests_);
private:
	tupleSP_type m_data;
};

template<class T, T N>
int Unit::handle(netsnmp_mib_handler* handler_, netsnmp_handler_registration* registration_,
		netsnmp_agent_request_info* info_, netsnmp_request_info* requests_)
{
	DEBUGMSGTL((TOKEN_PREFIX"handle", "Processing request (%d)\n", info_->mode));
	for (; NULL != requests_; requests_ = requests_->next)
	{
		if (0 != requests_->processed)
			continue;

		Unit* u = (Unit*)handler_->myvoid;
		u->m_data->get<T, N>(*(requests_->requestvb));
	}
	return SNMP_ERR_NOERROR;
}

///////////////////////////////////////////////////////////////////////////////
// struct Inject

struct Inject
{
	explicit Inject(tupleSP_type tuple_): m_tuple(tuple_)
	{
	}

	template<class T>
	void operator()(T) const;
private:
	tupleSP_type m_tuple;
};

template<class T>
void Inject::operator()(T) const
{
	std::auto_ptr<Unit> w(new Unit(m_tuple));
	netsnmp_handler_registration* r = Schema<Host::PROPERTY>::handler
					(T::value,
					&Unit::handle<typename T::value_type, T::value>,
					w.get());
	if (NULL == r)
		return;

	int e = netsnmp_register_read_only_scalar(r);
	if (MIB_REGISTERED_OK == e)
	{
		w.release();
		return;
	}
	netsnmp_handler_registration_free(r);
	snmp_log(LOG_ERR, LOG_PREFIX"error registering scalar handler for %s.%d\n",
			Schema<Host::PROPERTY>::name(), T::value);
}

} // namespace Scalar

namespace License
{
///////////////////////////////////////////////////////////////////////////////
// struct Counter

struct Counter
{
	enum
	{
		NOLIMIT = 65535U
	};

	explicit Counter() :
		m_usage(0), m_limit(0)
	{
	}

	explicit Counter(PRL_UINT32 usage_, PRL_UINT32 limit_) :
		m_usage(usage_), m_limit(limit_)
	{
	}

	PRL_UINT32 getLimit() const
	{
		return m_limit;
	}

	PRL_UINT32 getUsage() const
	{
		return m_usage;
	}

	static Counter parse(const char *value_);

private:
	PRL_UINT32 m_usage;
	PRL_UINT32 m_limit;
};

Counter Counter::parse(const char* value_)
{
	PRL_UINT32 l = 0, u = 0;

	if (boost::starts_with(value_, "\"unlimited\""))
		l = NOLIMIT;
	else if (!boost::starts_with(value_, "\"combined\""))
		l = strtoul(value_, NULL, 10);

	const char* const s = strchr(value_, '(');
	if (NULL != s)
		u = strtoul(&s[1], NULL, 10);
	return Counter(u, l);
}

///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit: Value::Storage
{
	Unit(PRL_HANDLE host_, tupleSP_type tuple_):
		m_host(host_), m_tuple(tuple_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:

	PRL_HANDLE m_host;
	tupleWP_type m_tuple;
};


void Unit::refresh(PRL_HANDLE h_)
{
	tupleSP_type t = m_tuple.lock();
	if (NULL == t.get())
		return;

	t->put<LICENSE_CTS>(0);
	t->put<LICENSE_VMS>(0);
	t->put<LICENSE_VES>(0);
	FILE* z = popen("vzlicview -a --class VZSRV", "r");
	if (NULL == z)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot start vzlicview\n");
	}
	else
	{ // read vzlicview
		std::ostringstream e;
		PRL_UINT32 a = 0;
		Counter v, m;
		while (!feof(z) && e.tellp() < 1024)
		{
			char b[128] = {};
			if (NULL == fgets(b, sizeof(b), z))
				continue;
			e << b;
			char* s = strchr(b, '=');
			if (NULL == s)
				continue;
			s[0] = 0;
			if (boost::ends_with(b, "ct_total"))
				v =  Counter::parse(&s[1]);
			else if (boost::ends_with(b, "nr_vms"))
				m = Counter::parse(&s[1]);
			else if (boost::ends_with(b, "servers_total"))
				a = Counter::parse(&s[1]).getLimit();
		}
		int s = pclose(z);
		if (0 == s)
		{
			t->put<LICENSE_CTS>(a ? : v.getLimit());
			t->put<LICENSE_VMS>(a ? : m.getLimit());
			t->put<LICENSE_VES>(a ? : std::min<PRL_UINT32>(
						m.getLimit() + v.getLimit(), Counter::NOLIMIT));
			t->put<LICENSE_CTS_USAGE>(v.getUsage());
			t->put<LICENSE_VMS_USAGE>(m.getUsage());
		}
		else
		{
			snmp_log(LOG_ERR, LOG_PREFIX"vzlicview status %d(%d):\n%s\n",
					WEXITSTATUS(s), s, e.str().c_str());
		}
	} // read vzlicview

	std::string line, temp;

	std::ifstream diskstats ("/proc/diskstats");
	int diskstats_ios_in_process = 0, diskstats_ms_doing_ios = 0;
	while (getline (diskstats, line))
	{
		std::istringstream iss(line);
		for (int i = 0; i < 11; i++)
			iss >> temp;
		iss >> temp;
		diskstats_ios_in_process += atoi(temp.c_str());
		iss >> temp;
		diskstats_ms_doing_ios += atoi(temp.c_str());
	}
	t->put<DISKSTATS_IOS_IN_PROCESS>(diskstats_ios_in_process);
	t->put<DISKSTATS_MS_DOING_OIS>(diskstats_ms_doing_ios);
	diskstats.close();

	std::ifstream interrupts ("/proc/interrupts");
	int int_res = 0;
	while (getline (interrupts, line))
	{
		std::istringstream iss(line);
		iss >> temp;
		if (temp.compare("RES:"))
			continue;
		while(iss >> temp)
			int_res += atoi(temp.c_str());
		break;
	}
	t->put<INT_RES>(int_res);
	interrupts.close();

	std::ifstream meminfo ("/proc/meminfo");
	int meminfo_buffers = 0, meminfo_dirty = 0;
	while (getline (meminfo, line))
	{
		std::istringstream iss(line);
		iss >> temp;
		if (!temp.compare("Buffers:"))
			iss >> meminfo_buffers;
		else if (!temp.compare("Dirty:"))
			iss >> meminfo_dirty;
	}
	t->put<MEMINFO_DIRTY>(meminfo_dirty);
	t->put<MEMINFO_BUFFERS>(meminfo_buffers);
	meminfo.close();

	std::ifstream stat ("/proc/stat");
	enum softirqStatsNames {
		TOTAL,
		HI,
		TIMER,
		NET_TX,
		NET_RX,
		BLOCK,
		BLOCK_IOPOLL,
		TASKLET,
		SCHED,
		HRTIMER,
		RCU,
		_LAST
	};
	int stat_intr_46 = 0;
	int stat_softirq_rcu = 0, stat_softirq_sched = 0, stat_softirq_net_tx = 0;
	int stat_procs_running = 0, stat_procs_blocked = 0, stat_processes = 0;
	while (getline (stat, line))
	{
		std::istringstream iss(line);
		iss >> temp;
		if (!temp.compare("softirq"))
			for (int i = TOTAL; i < _LAST; i++)
			{
				iss >> temp;
				switch(i)
				{
				case RCU:
					stat_softirq_rcu = atoi(temp.c_str());
					break;
				case SCHED:
					stat_softirq_sched = atoi(temp.c_str());
					break;
				case NET_TX:
					stat_softirq_net_tx = atoi(temp.c_str());
					break;
				}
			}
		else if (!temp.compare("intr"))
		{
			iss >> stat_intr_46;
			for (int i = 0; i <= 46; i++)
				iss >> stat_intr_46;
		}
		else if (!temp.compare("procs_blocked"))
			iss >> stat_procs_blocked;
		else if (!temp.compare("procs_running"))
			iss >> stat_procs_running;
		else if (!temp.compare("processes"))
			iss >> stat_processes;
	}
	t->put<STAT_SOFTIRQ_RCU>(stat_softirq_rcu);
	t->put<STAT_SOFTIRQ_SCHED>(stat_softirq_sched);
	t->put<STAT_SOFTIRQ_NET_TX>(stat_softirq_net_tx);
	t->put<STAT_INTR_46>(stat_intr_46);
	t->put<STAT_PROCS_BLOCKED>(stat_procs_blocked);
	t->put<STAT_PROCS_RUNNING>(stat_procs_running);
	t->put<STAT_PROCESSES>(stat_processes);
	stat.close();
}

} // namespace License

///////////////////////////////////////////////////////////////////////////////
// struct Unit

Unit::Unit(PRL_HANDLE host_, const space_type& space_):
	Environment(host_), m_data(space_.get<0>())
{
	PRL_HANDLE j = PrlSrv_SubscribeToPerfStats(host_, "*");
	PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
	(void)e;
	PrlHandle_Free(j);
	// usage
	addQueryUsage(new License::Unit(host_, m_data));
	// report
	addValue(new Value::Composite::Scalar<PROPERTY>(m_data));
	// proxies
	Oid_type o = boost::assign::list_of<oid>(1)(3)(6)(1)(4)(1)(2021)(4)(5)(0);
	addValue(new Proxy(o));
}

Unit::~Unit()
{
	PRL_HANDLE j = PrlSrv_UnsubscribeFromPerfStats(h());
	PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
	(void)e;
	PrlHandle_Free(j);
}

VE::UnitSP Unit::find(const std::string& id_, const VE::space_type& ves_) const
{
	VE::UnitSP output;
	PRL_HANDLE j = PrlSrv_GetVmConfig(h(), id_.c_str(), PGVC_SEARCH_BY_UUID);
	PRL_RESULT e = PrlJob_Wait(j, Sdk::TIMEOUT);
	if (PRL_SUCCEEDED(e))
	{
		PRL_HANDLE r, u;
		e = PrlJob_GetResult(j, &r);
		if (PRL_SUCCEEDED(e))
		{
			e = PrlResult_GetParamByIndex(r, 0, &u);
			if (PRL_SUCCEEDED(e))
				output.reset(make(u, ves_));
			PrlHandle_Free(r);
		}
	}
	PrlHandle_Free(j);
	return output;
}

bool Unit::list(std::list<VE::UnitSP>& dst_, const VE::space_type& ves_) const
{
	PRL_HANDLE j = PrlSrv_GetVmListEx(h(), PGVLF_GET_ONLY_IDENTITY_INFO | PVTF_VM | PVTF_CT);
	if (PRL_INVALID_HANDLE == j)
		return true;

	bool output = true;
	PRL_HANDLE r = PRL_INVALID_HANDLE;
	do
	{
		PRL_RESULT e = PrlJob_Wait(j, Sdk::TIMEOUT);
		if (PRL_FAILED(e))
			break;

		e = PrlJob_GetResult(j, &r);
		if (PRL_FAILED(e))
			break;

		PRL_UINT32 c = 0;
		e = PrlResult_GetParamsCount(r, &c);
		if (PRL_FAILED(e))
			break;

		output = false;
		for (PRL_UINT32 i = 0; i < c; ++i)
		{
			PRL_HANDLE u;
			e = PrlResult_GetParamByIndex(r, i, &u);
			if (PRL_FAILED(e))
			{
				output = true;
				break;
			}
			VE::Unit* x = make(u, ves_);
			if (NULL != x)
				dst_.push_back(VE::UnitSP(x));
		}
	} while(false);
	PrlHandle_Free(r);
	PrlHandle_Free(j);
	return output;
}

void Unit::pullUsage()
{
	PRL_HANDLE r;
	r = Sdk::getAsyncResult(PrlSrv_GetStatistics(h()));
	if (PRL_INVALID_HANDLE != r)
	{
		refresh(r);
		PrlHandle_Free(r);
	}
	r = Sdk::getAsyncResult(PrlSrv_GetLicenseInfo(h()));
	if (PRL_INVALID_HANDLE != r)
	{
		refresh(r);
		PrlHandle_Free(r);
	}
}

bool Unit::inject(space_type& dst_)
{
	tupleSP_type t(new tuple_type);
	mpl::for_each<Details::Names<Host::PROPERTY>::type>(Scalar::Inject(t));
	dst_.get<0>() = t;
	return false;
}

void Unit::ves(unsigned ves_)
{
	m_data->put<LOCAL_VES>(ves_);
	// there is no max_ves limit yet, thus we need to report 'unlimited'
	m_data->put<LIMIT_VES>(License::Counter::NOLIMIT);
}

} // namespace Host
} // namespace Rmond

