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

#include "ve.h"
#include "system.h"
#include "handler.h"
#include <cstring>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_array.hpp>
#include <boost/unordered_map.hpp>
#include <prlsdk/PrlApiVm.h>
#include <prlsdk/PrlPerfCounters.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/functional/hash/hash.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <fstream>

namespace
{
///////////////////////////////////////////////////////////////////////////////
// struct Iterator

template<class P, class H, class D>
struct Iterator: boost::iterator_facade<Iterator<P, H, D>, D,
				boost::forward_traversal_tag>
{
	Iterator(): m_h(), m_p(0)
	{
	}
	explicit Iterator(H h_): m_h(h_), m_p(P::count(h_))
	{
		increment();
	}
private:
	friend class boost::iterator_core_access;

	void increment()
	{
		while(0 < m_p)
		{
			m_value.reset(P::take(m_h, --m_p));
			if (NULL != m_value.get())
				return;
		}
		m_h = H();
	}

	bool equal(const Iterator& other_) const
	{
		return m_h == other_.m_h && other_.m_p == m_p;
	}

	D& dereference() const
	{
		return *m_value;
	}

	H m_h;
	PRL_UINT32 m_p;
	std::auto_ptr<D> m_value;
};

} // namespace

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::TABLE>

Oid_type Schema<VE::TABLE>::uuid()
{
	return Schema<void>::uuid(55);
}

const char* Schema<VE::TABLE>::name()
{
	return TOKEN_PREFIX"ves";
}

netsnmp_handler_registration* Schema<VE::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<VE::TABLE>(handler_, my_, HANDLER_CAN_RONLY);
}

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::Disk::TABLE>

Oid_type Schema<VE::Disk::TABLE>::uuid()
{
	return Schema<void>::uuid(56);
}

const char* Schema<VE::Disk::TABLE>::name()
{
	return TOKEN_PREFIX"vhds";
}

netsnmp_handler_registration*
	Schema<VE::Disk::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<VE::Disk::TABLE>(handler_, my_, HANDLER_CAN_RONLY);
}

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::Network::TABLE>

Oid_type Schema<VE::Network::TABLE>::uuid()
{
	return Schema<void>::uuid(57);
}

const char* Schema<VE::Network::TABLE>::name()
{
	return TOKEN_PREFIX"veths";
}

netsnmp_handler_registration*
	Schema<VE::Network::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<VE::Network::TABLE>(handler_, my_, HANDLER_CAN_RONLY);
}

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::CPU::TABLE>

Oid_type Schema<VE::CPU::TABLE>::uuid()
{
	return Schema<void>::uuid(58);
}

const char* Schema<VE::CPU::TABLE>::name()
{
	return TOKEN_PREFIX"vcpus";
}

netsnmp_handler_registration*
	Schema<VE::CPU::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<VE::CPU::TABLE>(handler_, my_, HANDLER_CAN_RONLY);
}

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::Counters::Linux::TABLE>

Oid_type Schema<VE::Counters::Linux::TABLE>::uuid()
{
	Oid_type output = Schema<void>::uuid(59);
	output.push_back(1);
	return output;
}

const char* Schema<VE::Counters::Linux::TABLE>::name()
{
	return TOKEN_PREFIX"Counters::Linux";
}

netsnmp_handler_registration*
	Schema<VE::Counters::Linux::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<VE::Counters::Linux::TABLE>(handler_, my_, HANDLER_CAN_RONLY);
}

namespace VE
{
typedef boost::unordered_map<std::string, struct ConnectionToVM*> uuidConnectionMap;
uuidConnectionMap uuid2Connection;

struct ConnectionToVM {
private:
	PRL_HANDLE hLogin;
	PRL_HANDLE hResult;
	PRL_HANDLE hVmGuest;
	PRL_HANDLE hConnect;
	PRL_HANDLE hArgs;
	PRL_HANDLE hEnvs;
	PRL_HANDLE hExecJob;
	int vmPipe[2];
	PRL_HANDLE m_veHandle;
	const static int readSize = 1024;
	const static int bufferSize = 4*readSize;
	int readBytes;
	char buffer[bufferSize];
	const static int maxFaults = 3;
	int errors;
public:
	ConnectionToVM(const char *cmd, PRL_HANDLE veHandle);
	~ConnectionToVM();
	int *getVmPipe() {return vmPipe;};
	int getErrors() {return errors;};
	int jobAlive() {return PrlJob_Wait(hExecJob, 0) == PRL_ERR_TIMEOUT;};
	int lostSignal() {return getErrors() == maxFaults;};
	char *getLastLine();
};

ConnectionToVM::ConnectionToVM(const char *cmd, PRL_HANDLE veHandle):
		hLogin(PRL_INVALID_HANDLE),
		hResult(PRL_INVALID_HANDLE),
		hVmGuest(PRL_INVALID_HANDLE),
		hConnect(PRL_INVALID_HANDLE),
		hArgs(PRL_INVALID_HANDLE),
		hEnvs(PRL_INVALID_HANDLE),
		hExecJob(PRL_INVALID_HANDLE),
		m_veHandle(veHandle)
{
	int ret;
	PRL_UINT32 nFlags = PFD_STDOUT | PRPM_RUN_PROGRAM_ENTER;
	vmPipe[0] = vmPipe[1] = -1;
	errors = 0;
	readBytes = 0;
	hLogin = PrlVm_LoginInGuest(m_veHandle, PRL_PRIVILEGED_GUEST_OS_SESSION, 0, 0);
	if (hLogin == PRL_INVALID_HANDLE)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlVm_LoginInGuest error\n");
		throw std::exception();
	}
	if ((ret = PrlJob_Wait(hLogin, 1000)) != PRL_ERR_SUCCESS) //1sec timeout
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlJob_Wait error %d\n", ret);
		throw std::exception();
	}
	if ((ret = PrlJob_GetResult(hLogin, &hResult)) != PRL_ERR_SUCCESS)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlJob_GetResult error %d\n", ret);
		throw std::exception();
	}
	if ((ret = PrlResult_GetParam(hResult, &hVmGuest)) != PRL_ERR_SUCCESS)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlResult_GetParam error %d\n", ret);
		throw std::exception();
	}
	hConnect = PrlVm_Connect(m_veHandle, PDCT_LOW_QUALITY_WITHOUT_COMPRESSION);
	if (hConnect == PRL_INVALID_HANDLE)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlVm_Connect error\n");
		throw std::exception();
	}
	if ((ret = PrlJob_Wait(hConnect, 1000)) != PRL_ERR_SUCCESS) //1sec timeout
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlJob_Wait error %d\n", ret);
		throw std::exception();
	}
	if ((ret = PrlApi_CreateStringsList(&hArgs)) != PRL_ERR_SUCCESS)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlApi_CreateStringsList error %d\n", ret);
		throw std::exception();
	}
	if ((ret = PrlApi_CreateStringsList(&hEnvs)) != PRL_ERR_SUCCESS)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlApi_CreateStringsList error %d\n", ret);
		throw std::exception();
	}

	if (pipe(vmPipe) != 0)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"pipe error %s\n", strerror(errno));
		throw std::exception();
	}

	hExecJob = PrlVmGuest_RunProgram(hVmGuest, cmd, hArgs, hEnvs, nFlags,
						PRL_INVALID_FILE_DESCRIPTOR,
						vmPipe[1],
						PRL_INVALID_FILE_DESCRIPTOR);
//	close(vmPipe[1]);
	if (hExecJob == PRL_INVALID_HANDLE)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlVmGuest_RunProgram error\n");
		throw std::exception();
	}
//	ret = PrlJob_Wait(hExecJob, 1000); //1sec timeout
/*	if ((ret = PrlJob_Wait(hExecJob, 1000)) != PRL_ERR_SUCCESS) //1sec timeout
	{
		snmp_log(LOG_ERR, LOG_PREFIX"PrlJob_Wait error %d\n", ret);
		throw std::exception();
	}
*/
}

ConnectionToVM::~ConnectionToVM()
{
	PrlVm_Disconnect(m_veHandle);
	PrlHandle_Free(PrlVmGuest_Logout(hVmGuest, 0));
	PrlHandle_Free(hExecJob);
	if (!vmPipe[0])
		close(vmPipe[0]);
	if (!vmPipe[1])
		close(vmPipe[1]);
	PrlHandle_Free(hEnvs);
	PrlHandle_Free(hArgs);
	PrlHandle_Free(hConnect);
	PrlHandle_Free(hVmGuest);
	PrlHandle_Free(hResult);
	PrlHandle_Free(hLogin);
}

char *ConnectionToVM::getLastLine()
{
	int faults = 0;

	while (faults != maxFaults)
	{
		assert(readBytes < bufferSize - readSize);
		int n = read(vmPipe[0], buffer + readBytes, readSize);
		if (n == -1)
		{
			snmp_log(LOG_ERR, LOG_PREFIX"read error %s\n", strerror(errno));
			errors++;
			return NULL;
		}
		readBytes += n;
		buffer[readBytes] = '\0';
		char *last = strrchr(buffer, '\n');
		if (!last)
		{
			// unlikely case of incomplete string
incomplete:
			sleep(0);
			faults++;
			continue;
		}
		if (last[1] != '\0')
		{
			memmove(buffer, last + 1, buffer + readBytes - last - 1);
			readBytes = buffer + readBytes - last - 1;
			goto incomplete;
		}

		*last = '\0';
		char * before = strrchr(buffer, '\n');

		if (before)
			memmove(buffer, before + 1, buffer + readBytes - before);
		errors = 0;
		readBytes = 0;
		return buffer;
	}
	snmp_log(LOG_ERR, LOG_PREFIX"could not read whole line\n");
	errors++;
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// struct Name

struct Name: Value::Storage
{
	explicit Name(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Name::refresh(PRL_HANDLE h_)
{
	tupleSP_type y = m_data.lock();
	if (NULL == y.get())
		return;

	std::string x;
	x = Sdk::getString(boost::bind(&PrlVmCfg_GetName, h_, _1, _2));
	if (!x.empty())
		y->put<NAME>(x);
	x = Sdk::getString(boost::bind(&PrlVmCfg_GetUuid, h_, _1, _2));
	if (!x.empty())
		y->put<UUID>(x);
}

///////////////////////////////////////////////////////////////////////////////
// struct Type

struct Type: Value::Storage
{
	explicit Type(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
	static bool extract_type(PRL_HANDLE h_, PRL_VM_TYPE& dst_);
	static bool extract_os_type(PRL_HANDLE h_, PRL_UINT32& dst_);
private:
	tupleWP_type m_data;
};

void Type::refresh(PRL_HANDLE h_)
{
	tupleSP_type y = m_data.lock();
	if (NULL != y.get())
	{
		PRL_VM_TYPE x = PVT_VM;
		extract_type(h_, x);
		y->put<TYPE>(x);
		PRL_UINT32 os_type;
		extract_os_type(h_, os_type);
		y->put<OS_TYPE>(os_type);
	}
}

bool Type::extract_type(PRL_HANDLE h_, PRL_VM_TYPE& dst_)
{
	PRL_VM_TYPE t = PVT_VM;
	PRL_RESULT e = PrlVmCfg_GetVmType(h_, &t);
	if (PRL_FAILED(e))
		return true;

	dst_ = t;
	return false;
}

bool Type::extract_os_type(PRL_HANDLE h_, PRL_UINT32& dst_)
{
	PRL_UINT32 t = -13;
	PRL_RESULT e = PrlVmCfg_GetOsType(h_, &t);
	if (PRL_FAILED(e))
		return true;

	dst_ = t;
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// struct State

struct State: Value::Storage
{
	State(PRL_HANDLE ve_, tupleSP_type data_): m_ve(ve_), m_data(data_)
	{
	}

	void extract(PRL_HANDLE h_);
	void refresh(PRL_HANDLE h_);
private:
	void put(VIRTUAL_MACHINE_STATE value_);

	PRL_HANDLE m_ve;
	tupleWP_type m_data;
};

void State::put(VIRTUAL_MACHINE_STATE value_)
{
	tupleSP_type y = m_data.lock();
	if (NULL == y.get())
		return;

	if (VMS_RUNNING == value_ && value_ != y->get<STATE>())
	{
		PRL_HANDLE j = PrlVm_SubscribeToPerfStats(m_ve, "*");
		PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
		(void)e;
		PrlHandle_Free(j);
	}
	y->put<STATE>(value_);
}

void State::extract(PRL_HANDLE h_)
{
	PRL_HANDLE p = PRL_INVALID_HANDLE;
	PRL_RESULT r = PrlEvent_GetParamByName(h_, "vminfo_vm_state", &p);
	if (PRL_SUCCEEDED(r))
	{
		PRL_UINT32 v = 0;
		r = PrlEvtPrm_ToUint32(p, &v);
		put((VIRTUAL_MACHINE_STATE)v);
		PrlHandle_Free(p);
	}
}

void State::refresh(PRL_HANDLE h_)
{
	PRL_HANDLE r = Sdk::getAsyncResult(PrlVm_GetState(m_ve));
	if (PRL_INVALID_HANDLE == r)
		return;

	VIRTUAL_MACHINE_STATE s;
	PRL_RESULT e = PrlVmInfo_GetState(r, &s);
	if (PRL_SUCCEEDED(e))
		put(s);

	PrlHandle_Free(r);
}

///////////////////////////////////////////////////////////////////////////////
// struct Provenance

struct Provenance: Value::Storage
{
	explicit Provenance(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	static FILE* shaman(PRL_HANDLE h_, tupleSP_type ve_);

	tupleWP_type m_data;
};

FILE* Provenance::shaman(PRL_HANDLE h_, tupleSP_type ve_)
{
	PRL_VM_TYPE t = PVT_VM;
	if (Type::extract_type(h_, t))
		return NULL;

	std::string x;
	switch (t)
	{
	case PVT_VM:
		x.append("shaman get-last-node vm-").append(ve_->get<NAME>());
		break;
	case PVT_CT:
		x.append("shaman get-last-node ct-").append(ve_->get<VEID>());
		break;
	default:
		snmp_log(LOG_ERR, LOG_PREFIX"unsupported ve type %d\n", t);
		return NULL;
	}
	FILE* output = popen(x.c_str(), "r");
	if (NULL == output)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot start command line %s\n",
			x.c_str());
	}
	return output;
}

void Provenance::refresh(PRL_HANDLE h_)
{
	tupleSP_type y = m_data.lock();
	if (NULL == y.get())
		return;

	y->put<PERFECT_NODE>(std::string(""));
	FILE* p = shaman(h_, y);
	if (NULL == p)
		return;

	std::ostringstream e;
	static const char MARK[] = "Resource last node ID :";
	while (!feof(p) && e.tellp() < 1024)
	{
		char b[128] = {};
		if (NULL == fgets(b, sizeof(b), p))
			continue;
		e << b;
		if (boost::starts_with(b, MARK))
		{
			std::string n = &b[sizeof(MARK) - 1];
			boost::trim(n);
			y->put<PERFECT_NODE>(n);
		}
	}
	int s = pclose(p);
	if (0 != s)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"shaman status %d(%d):\n%s\n",
				WEXITSTATUS(s), s, e.str().c_str());
	}
}

///////////////////////////////////////////////////////////////////////////////
// struct Perspective

template<class T>
struct Perspective
{
	typedef Table::Unit<T> table_type;

	Perspective(tupleSP_type parent_, boost::weak_ptr<table_type> table_);

	Value::Composite::Base* value() const
	{
		return new Value::Composite::Range<T>(m_parent, m_table);
	}
	template<class F>
	void merge(F flavor_, PRL_HANDLE update_);
	template<class F>
	typename table_type::tupleSP_type tuple(F flavor_);
private:
	Oid_type m_parent;
	boost::weak_ptr<table_type> m_table;
	typename table_type::key_type m_uuid;
};

template<class T>
Perspective<T>::Perspective(tupleSP_type parent_, boost::weak_ptr<table_type> table_):
	m_table(table_)
{
	const netsnmp_index& k = parent_->key();
	m_parent.assign(k.oids, k.oids + k.len);
	m_uuid.template put<VE::TABLE, VE::VEID>(parent_->get<VE::VEID>());
}

template<class T>
template<class F>
typename Perspective<T>::table_type::tupleSP_type Perspective<T>::tuple(F flavor_)
{
	boost::shared_ptr<table_type> z = m_table.lock();
	if (NULL == z.get())
		return typename table_type::tupleSP_type();

	typename table_type::key_type k = flavor_.key(m_uuid);
	typename table_type::tupleSP_type output = z->find(k);
	if (NULL == output.get())
	{
		output = flavor_.tuple(m_uuid);
		if (z->insert(output))
			output.reset();
	}
	return output;
}

template<class T>
template<class F>
void Perspective<T>::merge(F flavor_, PRL_HANDLE update_)
{
	boost::shared_ptr<table_type> z = m_table.lock();
	if (NULL == z.get())
		return;

	flavor_.fill(m_uuid, update_);
	BOOST_FOREACH(typename table_type::tupleSP_type d, z->range(m_parent))
	{
		if (flavor_.apply(*d))
			z->erase(*d);
	}
	BOOST_FOREACH(typename table_type::tupleSP_type r, flavor_.rest())
	{
		z->insert(r);
	}
}

namespace Memory
{
///////////////////////////////////////////////////////////////////////////////
// struct Query

struct Query: Value::Storage
{
	explicit Query(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Query::refresh(PRL_HANDLE h_)
{
	tupleSP_type y = m_data.lock();
	if (NULL == y.get())
		return;

	PRL_UINT64 x;
	PRL_RESULT r = PrlStat_GetTotalRamSize(h_, &x);
	if (PRL_SUCCEEDED(r))
		y->put<MEMORY_TOTAL>(x);

	r = PrlStat_GetUsageRamSize(h_, &x);
	if (PRL_SUCCEEDED(r))
		y->put<MEMORY_USAGE>(x);

	r = PrlStat_GetTotalSwapSize(h_, &x);
	if (PRL_SUCCEEDED(r))
		y->put<SWAP_TOTAL>(x);

	r = PrlStat_GetUsageSwapSize(h_, &x);
	if (PRL_SUCCEEDED(r))
		y->put<SWAP_USAGE>(x);
}

///////////////////////////////////////////////////////////////////////////////
// struct Event

struct Event: Value::Storage
{
	explicit Event(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Event::refresh(PRL_HANDLE h_)
{
	std::string n = Sdk::getString(boost::bind(&PrlEvtPrm_GetName, h_, _1, _2));
	if (0 != n.compare(PRL_GUEST_RAM_USAGE_PTRN))
		return;

	table_type::tupleSP_type t = m_data.lock();
	if (NULL == t.get())
		return;

	PRL_UINT64 u = 0;
	PRL_RESULT r = PrlEvtPrm_ToUint64(h_, &u);
	if (PRL_FAILED(r) || u == 0)
		return;

	t->put<MEMORY_USAGE>(u);
}

} // namespace Memory

namespace Counters
{

namespace Linux
{

namespace Virtual
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
typedef table_type::tupleSP_type tupleSP_type;
typedef boost::weak_ptr<table_type::tuple_type> tupleWP_type;

///////////////////////////////////////////////////////////////////////////////
// struct Flavor

struct Flavor
{
	explicit Flavor(PRL_HANDLE veHandle_, VE::tupleWP_type ve_):
		m_veHandle(veHandle_), m_ve(ve_)
	{
	}

	tupleSP_type tuple(const table_type::key_type& uuid_) const;
	tupleSP_type dataFromCT(const tupleSP_type output,
			const std::string& ctid, const std::string& uuid) const;
	tupleSP_type dataFromLinVM(const tupleSP_type output,
			const std::string& uuid) const;

private:
	PRL_HANDLE m_veHandle;
	VE::tupleWP_type m_ve;
};

tupleSP_type Flavor::dataFromCT(const tupleSP_type output,
		const std::string& ctid, const std::string& uuid) const
{
	FILE* z = popen(("vzlist -o laverage " + ctid).c_str(), "r");
	if (NULL == z)
		snmp_log(LOG_ERR, LOG_PREFIX"cannot start vzlist %s\n", ctid.c_str());
	else
	{ // read vzlist
		std::ostringstream e;
		while (!feof(z) && e.tellp() < 1024)
		{
			char b[128] = {};
			if (NULL == fgets(b, sizeof(b), z))
				continue;
			e << b;
			char* s = strchr(b, '/');
			if (!s)
				continue;
			s += 1;
			s = strchr(s, '/');
			s += 1;
			output->put<LOADAVG_15>((int)(strtod(s, NULL)*100+0.5));
		}
		pclose(z);
	} // read vzlist

	std::string line, temp, envId;
	envId = Sdk::getString(boost::bind(&PrlVmCfg_GetCtId, m_veHandle, _1, _2));
	std::ifstream meminfo (("/proc/bc/" + envId + "/meminfo").c_str());;
	int meminfo_writeback = 0, meminfo_dirty = 0, meminfo_sunreclaim = 0;
	while (getline(meminfo, line))
	{
		std::istringstream iss(line);
		iss >> temp;
		if (!temp.compare("Writeback:"))
		{
			iss >> temp;
			meminfo_writeback = atoi(temp.c_str());
		}
		else if (!temp.compare("Dirty:"))
		{
			iss >> temp;
			meminfo_dirty = atoi(temp.c_str());
		}
		else if (!temp.compare("SUnreclaim:"))
		{
			iss >> temp;
			meminfo_sunreclaim = atoi(temp.c_str());
		}
	}
	output->put<MEMINFO_DIRTY>(meminfo_dirty);
	output->put<MEMINFO_WRITEBACK>(meminfo_writeback);
	output->put<MEMINFO_SUNRECLAIM>(meminfo_sunreclaim);
	meminfo.close();

	return output;
}

tupleSP_type Flavor::dataFromLinVM(const tupleSP_type output,
		const std::string& uuid) const
{
	struct ConnectionToVM *connection;
	if (uuid2Connection.find(uuid) == uuid2Connection.end())
	{
		connect:
		try
		{
			connection = new ConnectionToVM("/usr/bin/drs-transport", m_veHandle);
			uuid2Connection[uuid] = connection;
		}
		catch(...)
		{
			;
		}
		return output;
	}
	else
		connection = uuid2Connection.at(uuid);
	char *b = connection->getLastLine();
	if (!b && (!connection->jobAlive() || connection->lostSignal()))
	{
		snmp_log(LOG_ERR, LOG_PREFIX"reconnecting to %s\n", uuid.c_str());
		delete uuid2Connection.at(uuid);
		uuid2Connection.erase(uuid);
		goto connect;
	}
	while (b)
	{
		#define READ_TO_PROPERTY(string, property) { \
			if (boost::starts_with(b, string)) { \
				char *temp = strchr(b, ':'); \
				if (temp) \
					output->put<property>(strtoul(temp + 1, NULL, 10)); \
			} \
		}
		READ_TO_PROPERTY("loadavg_15",LOADAVG_15);
		READ_TO_PROPERTY("loadavg_currExisting",LOADAVG_CURRENT_EXISTING);
		READ_TO_PROPERTY("diskstats_ios_in_process",DISKSTATS_IOS_IN_PROCESS);
		READ_TO_PROPERTY("diskstats_ms_writing",DISKSTATS_MS_WRITING);
		READ_TO_PROPERTY("meminfo_PageTables",MEMINFO_PAGETABLES);
		READ_TO_PROPERTY("meminfo_Mapped",MEMINFO_MAPPED);
		READ_TO_PROPERTY("meminfo_Dirty",MEMINFO_DIRTY);
		READ_TO_PROPERTY("meminfo_SUnreclaim",MEMINFO_SUNRECLAIM);
		READ_TO_PROPERTY("meminfo_Writeback",MEMINFO_WRITEBACK);
		b = strchr(b, ' ');
		if (b)
			b += 1;
		else
			break;
		#undef READ_TO_PROPERTY
	}
	return output;
}

tupleSP_type Flavor::tuple(const table_type::key_type& uuid_) const
{
	tupleSP_type output(new table_type::tuple_type(uuid_));
	VE::tupleSP_type x = m_ve.lock();

	if (x->get<TYPE>() == PVT_CT && x->get<STATE>() == VMS_RUNNING)
		dataFromCT(output, x->get<NAME>(), x->get<UUID>());
	if (x->get<TYPE>() == PVT_VM && x->get<STATE>() == VMS_RUNNING
			&& x->get<OS_TYPE>() == PVS_GUEST_TYPE_LINUX)
		dataFromLinVM(output, x->get<UUID>());
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Update

struct Update
{
	explicit Update(PRL_HANDLE veHandle_, VE::tupleSP_type ve_):
		m_ve(ve_), m_veHandle(veHandle_)
	{
	}

	std::list<tupleSP_type> rest();
	bool apply(table_type::tuple_type& dst_);
	void fill(const table_type::key_type& uuid_, PRL_HANDLE h_);
private:
	typedef std::map<unsigned, tupleSP_type> map_type;

	tupleSP_type m_sp;
	VE::tupleWP_type m_ve;
	PRL_HANDLE m_veHandle;
};

std::list<tupleSP_type> Update::rest()
{
	std::list<tupleSP_type> output;
	output.push_front(m_sp);
	return output;
}

bool Update::apply(table_type::tuple_type& to_)
{
	to_.put<LOADAVG_15>(m_sp->get<LOADAVG_15>());
	to_.put<LOADAVG_CURRENT_EXISTING>(m_sp->get<LOADAVG_CURRENT_EXISTING>());
	to_.put<DISKSTATS_IOS_IN_PROCESS>(m_sp->get<DISKSTATS_IOS_IN_PROCESS>());
	to_.put<DISKSTATS_MS_WRITING>(m_sp->get<DISKSTATS_MS_WRITING>());
	to_.put<MEMINFO_PAGETABLES>(m_sp->get<MEMINFO_PAGETABLES>());
	to_.put<MEMINFO_MAPPED>(m_sp->get<MEMINFO_MAPPED>());
	to_.put<MEMINFO_DIRTY>(m_sp->get<MEMINFO_DIRTY>());
	to_.put<MEMINFO_SUNRECLAIM>(m_sp->get<MEMINFO_SUNRECLAIM>());
	to_.put<MEMINFO_WRITEBACK>(m_sp->get<MEMINFO_WRITEBACK>());
	return false;
}

void Update::fill(const table_type::key_type& uuid_, PRL_HANDLE )
{
	VE::tupleSP_type x = m_ve.lock();
	if (NULL == x.get())
		return;
	m_sp = Flavor(m_veHandle, m_ve).tuple(uuid_);
}

} // namespace Virtual

///////////////////////////////////////////////////////////////////////////////
// struct Query

struct Query: Value::Storage
{
	Query(PRL_HANDLE ve_, tupleSP_type data_, const Perspective<TABLE>& system_):
		m_data(data_), m_system(system_), m_ve(ve_)
	{
	}

	void refresh(PRL_HANDLE h_)
	{
		tupleSP_type x = m_data.lock();
		if (NULL != x.get())
			m_system.merge(Virtual::Update(m_ve, x), h_);
	}
private:
	tupleWP_type m_data;
	Perspective<TABLE> m_system;
	PRL_HANDLE m_ve;
};

} // namespace Linux

} // namespace Counters

namespace CPU
{
///////////////////////////////////////////////////////////////////////////////
// struct Number

struct Number: Value::Storage
{
	explicit Number(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Number::refresh(PRL_HANDLE h_)
{
	PRL_UINT32 n = 0;
	int e = PrlVmCfg_GetCpuCount(h_, &n);
	if (PRL_FAILED(e))
		return;

	tupleSP_type x = m_data.lock();
	if (NULL != x.get())
		x->put<CPU_NUMBER>(n);
}

///////////////////////////////////////////////////////////////////////////////
// struct Limit

struct Limit: Value::Storage
{
	explicit Limit(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Limit::refresh(PRL_HANDLE h_)
{
	PRL_UINT32 n = 0;
	int e = PrlVmCfg_GetCpuLimit(h_, &n);
	if (PRL_FAILED(e))
		return;

	tupleSP_type x = m_data.lock();
	if (NULL != x.get())
		x->put<CPU_LIMIT>(n);
}

///////////////////////////////////////////////////////////////////////////////
// struct Units

struct Units: Value::Storage
{
	explicit Units(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Units::refresh(PRL_HANDLE h_)
{
	PRL_UINT32 n = 0;
	int e = PrlVmCfg_GetCpuUnits(h_, &n);
	if (PRL_FAILED(e))
		return;

	tupleSP_type x = m_data.lock();
	if (NULL != x.get())
		x->put<CPU_UNITS>(n);
}

namespace Virtual
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
typedef table_type::tupleSP_type tupleSP_type;
typedef boost::weak_ptr<table_type::tuple_type> tupleWP_type;

///////////////////////////////////////////////////////////////////////////////
// struct Flavor

struct Flavor
{
	explicit Flavor(unsigned ordinal_): m_ordinal(ordinal_)
	{
	}

	tupleSP_type tuple(const table_type::key_type& uuid_) const;
	table_type::key_type key(const table_type::key_type& uuid_) const;

private:
	unsigned m_ordinal;
};

tupleSP_type Flavor::tuple(const table_type::key_type& uuid_) const
{
	return tupleSP_type(new table_type::tuple_type(key(uuid_)));
}

table_type::key_type Flavor::key(const table_type::key_type& uuid_) const
{
	table_type::key_type output = uuid_;
	output.put<ORDINAL>(m_ordinal);
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Update

struct Update
{
	explicit Update(VE::tupleSP_type ve_): m_ve(ve_)
	{
	}

	std::list<tupleSP_type> rest();
	bool apply(table_type::tuple_type& dst_);
	void fill(const table_type::key_type& uuid_, PRL_HANDLE h_);
private:
	typedef std::map<unsigned, tupleSP_type> map_type;

	map_type m_map;
	VE::tupleWP_type m_ve;
};

std::list<tupleSP_type> Update::rest()
{
	std::list<tupleSP_type> output;
	if (!m_map.empty())
	{
		std::transform(m_map.begin(), m_map.end(), std::back_inserter(output),
			boost::bind(&map_type::value_type::second, _1));
		m_map.clear();
	}
	return output;
}

bool Update::apply(table_type::tuple_type& to_)
{
	map_type::iterator p = m_map.find(to_.get<ORDINAL>());
	if (m_map.end() == p)
		return true;

	VE::tupleSP_type x = m_ve.lock();
	if (NULL != x.get() && x->get<STATE>() != VMS_RUNNING)
		to_.put<TIME>(0);

	m_map.erase(p);
	return false;
}

void Update::fill(const table_type::key_type& uuid_, PRL_HANDLE )
{
	unsigned n = 0;
	VE::tupleSP_type x = m_ve.lock();
	if (NULL != x.get())
	{
		n = x->get<CPU_NUMBER>();
		if (PRL_CPU_UNLIMITED == n)
			return;

		m_map.clear();
	}
	for (unsigned i = 0; i < n; ++i)
	{
		m_map[i] = Flavor(i).tuple(uuid_);
	}
}

///////////////////////////////////////////////////////////////////////////////
// struct Event

struct Event: Value::Storage
{
	explicit Event(const Perspective<TABLE>& system_): m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);

private:
	Perspective<TABLE> m_system;
};

void Event::refresh(PRL_HANDLE h_)
{
	std::string n = Sdk::getString(boost::bind(&PrlEvtPrm_GetName, h_, _1, _2));
	if (n.empty())
		return;

	static const char m[] = "guest.vcpu";
	if (!boost::starts_with(n, m))
		return;

	PRL_UINT32 i = strtoul(&n[sizeof(m) - 1], NULL, 10);
	if ((std::numeric_limits<PRL_UINT32>::max)() == i)
		return;

	table_type::tupleSP_type t = m_system.tuple(Flavor(i));
	if (NULL == t.get())
		return;

	PRL_UINT64 u;
	PRL_RESULT r = PrlEvtPrm_ToUint64(h_, &u);
	if (PRL_FAILED(r))
		return;
	if (boost::ends_with(n, ".time"))
		t->put<TIME>(u);
}

} // namespace Virtual

///////////////////////////////////////////////////////////////////////////////
// struct Usage

struct Usage: Value::Storage
{
	Usage(tupleSP_type data_, const Perspective<TABLE>& system_):
		m_data(data_), m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
	Perspective<TABLE> m_system;
};

void Usage::refresh(PRL_HANDLE h_)
{
	PRL_UINT32 n = 0;
	PRL_RESULT r = PrlStat_GetCpusStatsCount(h_, &n);
	if (PRL_FAILED(r) || n == 0)
		return;
	PRL_HANDLE h;
	r = PrlStat_GetCpuStat(h_, 0, &h);
	if (PRL_FAILED(r))
		return;

	tupleSP_type x = m_data.lock();
	if (NULL != x.get())
	{
		PRL_UINT64 y;
		r = PrlStatCpu_GetSystemTime(h, &y);
		if (PRL_SUCCEEDED(r))
			x->put<CPU_SYSTEM>(y);

		r = PrlStatCpu_GetUserTime(h, &y);
		if (PRL_SUCCEEDED(r))
			x->put<CPU_USER>(y);

		m_system.merge(Virtual::Update(x), h_);
	}
	PrlHandle_Free(h);
}

} // namespace CPU

///////////////////////////////////////////////////////////////////////////////
// struct Devices

template<PRL_DEVICE_TYPE T, class F>
struct Devices
{
	typedef boost::shared_array<PRL_HANDLE> list_type;
	typedef std::pair<list_type, size_t> value_type;

	static value_type make(PRL_HANDLE h_);
	static PRL_UINT32 count(const value_type& value_)
	{
		return value_.second;
	}
	static F* take(const value_type& value_, PRL_UINT32 n_)
	{
		if (value_.second <= n_)
			return NULL;

		PRL_DEVICE_TYPE t;
		PRL_RESULT r = PrlVmDev_GetType(value_.first[n_], &t);
		if (PRL_FAILED(r) || t != T)
			return NULL;

		return F::yield(value_.first[n_]);
	}
private:
	struct Sweeper
	{
		explicit Sweeper(size_t count_): m_count(count_)
		{
		}

		void operator()(PRL_HANDLE* p_) const
		{
			for (unsigned i = m_count; i-- > 0;)
			{
				PrlHandle_Free(p_[i]);
			}
			delete[] p_;
		}
	private:
		size_t m_count;
	};
};

template<PRL_DEVICE_TYPE T, class F>
typename Devices<T, F>::value_type Devices<T, F>::make(PRL_HANDLE h_)
{
	PRL_UINT32 n = 0;
	PRL_RESULT r = PrlVmCfg_GetDevsCount(h_, &n);
	if (PRL_FAILED(r))
		return value_type();

	value_type output(list_type(new PRL_HANDLE[n], Sweeper(n)), n);
	r = PrlVmCfg_GetDevsList(h_, output.first.get(), &n);
	if (PRL_FAILED(r))
		return value_type();

	return output;
}

namespace Disk
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
typedef table_type::tupleSP_type tupleSP_type;
typedef boost::weak_ptr<table_type::tuple_type> tupleWP_type;

namespace List
{
///////////////////////////////////////////////////////////////////////////////
// struct Device

struct Device
{
	std::string name() const;
	PRL_UINT32 index() const;
	PRL_MASS_STORAGE_INTERFACE_TYPE type() const;

	static Device* yield(PRL_HANDLE h_)
	{
		return new Device(h_);
	}
private:
	explicit Device(PRL_HANDLE h_): m_h(h_), m_emulation(PDT_ANY_TYPE)
	{
		(void)PrlVmDev_GetEmulatedType(m_h, &m_emulation);
	}

	PRL_HANDLE m_h;
	PRL_VM_DEV_EMULATION_TYPE m_emulation;
};

std::string Device::name() const
{
	switch (m_emulation)
	{
	case PDT_ANY_TYPE:
		return std::string();
	case PDT_USE_FILE_SYSTEM:
		// container device
		return Sdk::getString(boost::bind(&PrlVmDev_GetFriendlyName, m_h, _1, _2));
	default:
		return Sdk::getString(boost::bind(&PrlVmDev_GetSysName, m_h, _1, _2));
	}
}

PRL_UINT32 Device::index() const
{
	PRL_UINT32 output = 0;
	PRL_RESULT e = PrlVmDev_GetStackIndex(m_h, &output);
	(void)e;
	return output;
}

PRL_MASS_STORAGE_INTERFACE_TYPE Device::type() const
{
	PRL_RESULT e;
	PRL_MASS_STORAGE_INTERFACE_TYPE output;
	switch (m_emulation)
	{
	case PDT_ANY_TYPE:
		return PMS_UNKNOWN_DEVICE;
	case PDT_USE_FILE_SYSTEM:
		// container device
		return PMS_IDE_DEVICE;
	default:
		e = PrlVmDev_GetIfaceType(m_h, &output);
		(void)e;
		return output;
	}
}

} // namespace List

namespace Usage
{
///////////////////////////////////////////////////////////////////////////////
// struct Device

struct Device
{
	explicit Device(PRL_HANDLE h_): m_h(h_)
	{
	}
	~Device()
	{
		PrlHandle_Free(m_h);
	}

	std::string name() const;
	PRL_UINT64 getFreeBytes() const;
	PRL_UINT64 getUsedBytes() const;
private:
	PRL_HANDLE m_h;
};

std::string Device::name() const
{
	return Sdk::getString(boost::bind(&PrlStatDisk_GetSystemName, m_h, _1, _2));
}

PRL_UINT64 Device::getFreeBytes() const
{
	PRL_UINT64 output = 0;
	PRL_RESULT e = PrlStatDisk_GetFreeDiskSpace(m_h, &output);
	(void)e;
	return output;
}

PRL_UINT64 Device::getUsedBytes() const
{
	PRL_UINT64 output = 0;
	PRL_RESULT e = PrlStatDisk_GetUsageDiskSpace(m_h, &output);
	(void)e;
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Policy

struct Policy
{
	static PRL_UINT32 count(PRL_HANDLE h_)
	{
		PRL_UINT32 output = 0;
		PRL_RESULT e = PrlStat_GetDisksStatsCount(h_, &output);
		(void)e;
		return output;
	}
	static Device* take(PRL_HANDLE h_, PRL_UINT32 n_)
	{
		PRL_HANDLE h;
		PRL_RESULT e = PrlStat_GetDiskStat(h_, n_, &h);
		if (PRL_FAILED(e))
			return NULL;

		return new Device(h);
	}
};

} // namespace Usage

///////////////////////////////////////////////////////////////////////////////
// struct Flavor

struct Flavor
{
	explicit Flavor(const std::string& name_): m_name(name_)
	{
	}

	tupleSP_type tuple(const table_type::key_type& uuid_) const;
	tupleSP_type tuple(const table_type::key_type& uuid_,
			const Usage::Device& usage_) const;
	table_type::key_type key(const table_type::key_type& uuid_) const;

	static Flavor* determine(PRL_HANDLE ve_, const std::string& counter_);
private:
	std::string m_name;
};

tupleSP_type Flavor::tuple(const table_type::key_type& uuid_) const
{
	table_type::key_type k = key(uuid_);
	tupleSP_type output(new table_type::tuple_type(k));
	output->put<NAME>(m_name);
	return output;
}

tupleSP_type Flavor::tuple(const table_type::key_type& uuid_,
			const Usage::Device& usage_) const
{
	tupleSP_type output = tuple(uuid_);
	output->put<USAGE>(usage_.getUsedBytes());
	output->put<TOTAL>(usage_.getUsedBytes() + usage_.getFreeBytes());
	return output;
}

table_type::key_type Flavor::key(const table_type::key_type& uuid_) const
{
	table_type::key_type output = uuid_;
	size_t h = 0xDEADF00D;
	boost::hash_combine(h, output.get<VE::TABLE, VE::VEID>());
	boost::hash_combine(h, m_name);
	output.put<HASH1>(h);
	output.put<HASH2>(h >> 32);
	return output;
}

Flavor* Flavor::determine(PRL_HANDLE ve_, const std::string& counter_)
{
	PRL_MASS_STORAGE_INTERFACE_TYPE a;
	PRL_UINT32 b = (std::numeric_limits<PRL_UINT32>::max)();
	typedef std::pair<std::string, PRL_MASS_STORAGE_INTERFACE_TYPE>
		map_type;
	map_type x[] = {map_type("devices.ide", PMS_IDE_DEVICE),
			map_type("devices.sata", PMS_SATA_DEVICE),
			map_type("devices.scsi", PMS_SCSI_DEVICE)};
	BOOST_FOREACH(const map_type& m, x)
	{
		if (boost::starts_with(counter_, m.first))
		{
			a = m.second;
			b = strtoul(&counter_[m.first.size()], NULL, 10);
			break;
		}
	}
	if ((std::numeric_limits<PRL_UINT32>::max)() == b)
		return NULL;

	typedef Devices<PDE_HARD_DISK, List::Device> policy_type;
	typedef Iterator<policy_type, policy_type::value_type,
			List::Device> iterator_type;
	iterator_type p(policy_type::make(ve_)), e;
	for (; p != e; ++p)
	{
		if (a == p->type() && p->index() == b)
			return new Flavor(p->name());
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// struct Update

struct Update
{
	std::list<tupleSP_type> rest();
	bool apply(table_type::tuple_type& dst_);
	void fill(const table_type::key_type& uuid_, PRL_HANDLE h_);
private:
	typedef std::map<std::string, tupleSP_type> map_type;

	map_type m_map;
};

std::list<tupleSP_type> Update::rest()
{
	std::list<tupleSP_type> output;
	if (!m_map.empty())
	{
		std::transform(m_map.begin(), m_map.end(), std::back_inserter(output),
			boost::bind(&map_type::value_type::second, _1));
		m_map.clear();
	}
	return output;
}

bool Update::apply(table_type::tuple_type& to_)
{
	map_type::iterator p = m_map.find(to_.get<NAME>());
	if (m_map.end() == p)
		return true;

	to_.put<TOTAL>(p->second->get<TOTAL>());
	to_.put<USAGE>(p->second->get<USAGE>());
	m_map.erase(p);
	return false;
}

void Update::fill(const table_type::key_type& uuid_, PRL_HANDLE h_)
{
	m_map.clear();
	Iterator<Usage::Policy, PRL_HANDLE, Usage::Device> e, p(h_);
	for (; p != e; ++p)
	{
		std::string n = p->name();
		if (!n.empty())
			m_map[n] = Flavor(n).tuple(uuid_, *p);
	}
}

///////////////////////////////////////////////////////////////////////////////
// struct Space

struct Space: Value::Storage
{
	explicit Space(const Perspective<TABLE>& system_): m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_)
	{
		m_system.merge(Update(), h_);
	}
private:
	Perspective<TABLE> m_system;
};

///////////////////////////////////////////////////////////////////////////////
// struct Io

struct Io: Value::Storage
{
	Io(PRL_HANDLE ve_, const Perspective<TABLE>& system_):
		m_ve(ve_), m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	PRL_HANDLE m_ve;
	Perspective<TABLE> m_system;
};

void Io::refresh(PRL_HANDLE h_)
{
	std::string n = Sdk::getString(boost::bind(&PrlEvtPrm_GetName, h_, _1, _2));
	if (n.empty())
		return;

	std::auto_ptr<Flavor> f(Flavor::determine(m_ve, n));
	if (NULL == f.get())
		return;
	table_type::tupleSP_type t = m_system.tuple(*f);
	if (NULL == t.get())
		return;

	PRL_UINT64 u;
	PRL_RESULT r = PrlEvtPrm_ToUint64(h_, &u);
	if (PRL_FAILED(r))
		return;
	if (boost::ends_with(n, ".read_requests"))
		t->put<READ_REQUESTS>(u);
	if (boost::ends_with(n, ".write_requests"))
		t->put<WRITE_REQUESTS>(u);
	if (boost::ends_with(n, ".read_total"))
		t->put<READ_BYTES>(u);
	if (boost::ends_with(n, ".write_total"))
		t->put<WRITE_BYTES>(u);
}

} // namespace Disk

namespace Network
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
typedef table_type::tupleSP_type tupleSP_type;
typedef boost::weak_ptr<table_type::tuple_type> tupleWP_type;

namespace Usage
{
///////////////////////////////////////////////////////////////////////////////
// struct Device

struct Device
{
	explicit Device(PRL_HANDLE h_): m_h(h_)
	{
	}
	~Device()
	{
		PrlHandle_Free(m_h);
	}

	std::string name() const;
	PRL_UINT64 getInBytes() const;
	PRL_UINT64 getOutBytes() const;
	PRL_UINT64 getInPackets() const;
	PRL_UINT64 getOutPackets() const;
private:
	PRL_HANDLE m_h;
};

std::string Device::name() const
{
	return Sdk::getString(boost::bind(&PrlStatIface_GetSystemName, m_h, _1, _2));
}

PRL_UINT64 Device::getInBytes() const
{
	PRL_UINT64 output = 0;
	PRL_RESULT e = PrlStatIface_GetInDataSize(m_h, &output);
	(void)e;
	return output;
}

PRL_UINT64 Device::getOutBytes() const
{
	PRL_UINT64 output = 0;
	PRL_RESULT e = PrlStatIface_GetOutDataSize(m_h, &output);
	(void)e;
	return output;
}

PRL_UINT64 Device::getInPackets() const
{
	PRL_UINT64 output = 0;
	PRL_RESULT e = PrlStatIface_GetInPkgsCount(m_h, &output);
	(void)e;
	return output;
}

PRL_UINT64 Device::getOutPackets() const
{
	PRL_UINT64 output = 0;
	PRL_RESULT e = PrlStatIface_GetOutPkgsCount(m_h, &output);
	(void)e;
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Policy

struct Policy
{
	static PRL_UINT32 count(PRL_HANDLE h_)
	{
		PRL_UINT32 output = 0;
		PRL_RESULT e = PrlStat_GetIfacesStatsCount(h_, &output);
		(void)e;
		return output;
	}
	static Device* take(PRL_HANDLE h_, PRL_UINT32 n_)
	{
		PRL_HANDLE h;
		PRL_RESULT e = PrlStat_GetIfaceStat(h_, n_, &h);
		if (PRL_FAILED(e))
			return NULL;

		return new Device(h);
	}
};

} // namespace Usage

namespace Device
{
///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit
{
	std::string mac() const
	{
		return Sdk::getString(boost::bind(&PrlVmDevNet_GetMacAddress, m_h, _1, _2));
	}
	std::string name() const
	{
		return Sdk::getString(boost::bind(&PrlVmDev_GetSysName, m_h, _1, _2));
	}
	PRL_UINT32 index() const
	{
		PRL_UINT32 output = 0;
		PRL_RESULT e = PrlVmDev_GetIndex(m_h, &output);
		(void)e;
		return output;
	}
	static Unit* yield(PRL_HANDLE h_)
	{
		return new Unit(h_);
	}
private:
	explicit Unit(PRL_HANDLE h_): m_h(h_)
	{
	}

	PRL_HANDLE m_h;
};

///////////////////////////////////////////////////////////////////////////////
// struct List

struct List
{
	explicit List(PRL_HANDLE ve_);

	Unit* find(const std::string& name_);
	Unit* determine(const std::string& counter_);
private:
	typedef Devices<PDE_GENERIC_NETWORK_ADAPTER, Unit> policy_type;
	typedef policy_type::value_type data_type;
	typedef Iterator<policy_type, data_type, Unit> iterator_type;

	data_type m_data;	
};

List::List(PRL_HANDLE ve_): m_data(policy_type::make(ve_))
{
}

Unit* List::find(const std::string& name_)
{
	iterator_type p(m_data), e;
	if (e == p)
		return NULL;
	for (; p != e; ++p)
	{
		if (name_ == p->name())
			return new Unit(*p);
	}
	return new Unit(*iterator_type(m_data));
}

Unit* List::determine(const std::string& counter_)
{
	static const char m[] = "net.nic";
	if (!boost::starts_with(counter_, m))
		return NULL;

	PRL_UINT32 i = strtoul(&counter_[sizeof(m) - 1], NULL, 10);
	if ((std::numeric_limits<PRL_UINT32>::max)() == i)
		return NULL;

	iterator_type p(m_data), e;
	for (; p != e; ++p)
	{
		if (i == p->index())
			return new Unit(*p);
	}
	return NULL;
}

} // namespace Device

///////////////////////////////////////////////////////////////////////////////
// struct Flavor

struct Flavor
{
	explicit Flavor(const Device::Unit& device_): m_device(&device_)
	{
	}

	tupleSP_type tuple(const table_type::key_type& uuid_) const;
	tupleSP_type tuple(const table_type::key_type& uuid_,
				const Usage::Device& usage_) const;
	table_type::key_type key(const table_type::key_type& uuid_) const;
private:
	const Device::Unit* m_device;
};

tupleSP_type Flavor::tuple(const table_type::key_type& uuid_) const
{
	table_type::key_type k = key(uuid_);
	tupleSP_type output(new table_type::tuple_type(k));
	output->put<MAC>(m_device->mac());
	return output;
}

tupleSP_type Flavor::tuple(const table_type::key_type& uuid_,
			const Usage::Device& usage_) const
{
	tupleSP_type output = tuple(uuid_);
	output->put<IN_BYTES>(usage_.getInBytes());
	output->put<OUT_BYTES>(usage_.getOutBytes());
	output->put<IN_PACKETS>(usage_.getInPackets());
	output->put<OUT_PACKETS>(usage_.getOutPackets());
	return output;
}

table_type::key_type Flavor::key(const table_type::key_type& uuid_) const
{
	table_type::key_type output = uuid_;
	output.put<NAME>(m_device->name());
	return output;
}

namespace Traffic
{
///////////////////////////////////////////////////////////////////////////////
// struct Event

struct Event: Value::Storage
{
	Event(PRL_HANDLE ve_, const Perspective<TABLE>& system_):
		m_ve(ve_), m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	PRL_HANDLE m_ve;
	Perspective<TABLE> m_system;
};

void Event::refresh(PRL_HANDLE h_)
{
	std::string n = Sdk::getString(boost::bind(&PrlEvtPrm_GetName, h_, _1, _2));
	if (n.empty())
		return;

	Device::List a(m_ve);
	std::auto_ptr<Device::Unit> d(a.determine(n));
	if (NULL == d.get())
		return;
	table_type::tupleSP_type t = m_system.tuple(Flavor(*d));
	if (NULL == t.get())
		return;

	PRL_UINT64 u;
	PRL_RESULT r = PrlEvtPrm_ToUint64(h_, &u);
	if (PRL_FAILED(r))
		return;
	if (boost::ends_with(n, ".pkts_in"))
		t->put<IN_PACKETS>(u);
	if (boost::ends_with(n, ".pkts_out"))
		t->put<OUT_PACKETS>(u);
	if (boost::ends_with(n, ".bytes_in"))
		t->put<IN_BYTES>(u);
	if (boost::ends_with(n, ".bytes_out"))
		t->put<OUT_BYTES>(u);
}

///////////////////////////////////////////////////////////////////////////////
// struct Update

struct Update
{
	explicit Update(PRL_HANDLE ve_): m_devices(ve_)
	{
	}

	std::list<tupleSP_type> rest();
	bool apply(table_type::tuple_type& dst_);
	void fill(const table_type::key_type& uuid_, PRL_HANDLE h_);
private:
	typedef std::map<std::string, tupleSP_type> map_type;

	map_type m_map;
	Device::List m_devices;
};

std::list<tupleSP_type> Update::rest()
{
	std::list<tupleSP_type> output;
	if (!m_map.empty())
	{
		std::transform(m_map.begin(), m_map.end(), std::back_inserter(output),
			boost::bind(&map_type::value_type::second, _1));
		m_map.clear();
	}
	return output;
}

bool Update::apply(table_type::tuple_type& to_)
{
	map_type::iterator p = m_map.find(to_.get<NAME>());
	if (m_map.end() == p)
		return true;

	to_.put<IN_BYTES>(p->second->get<IN_BYTES>());
	to_.put<OUT_BYTES>(p->second->get<OUT_BYTES>());
	to_.put<IN_PACKETS>(p->second->get<IN_PACKETS>());
	to_.put<OUT_PACKETS>(p->second->get<OUT_PACKETS>());
	m_map.erase(p);
	return false;
}

void Update::fill(const table_type::key_type& uuid_, PRL_HANDLE h_)
{
	m_map.clear();
	Iterator<Usage::Policy, PRL_HANDLE, Usage::Device> e, p(h_);
	for (; p != e; ++p)
	{
		std::auto_ptr<Device::Unit> d(m_devices.find(p->name()));
		if (NULL != d.get())
			m_map[d->name()] = Flavor(*d).tuple(uuid_, *p);
	}
}

///////////////////////////////////////////////////////////////////////////////
// struct Query

struct Query: Value::Storage
{
	Query(PRL_HANDLE ve_, const Perspective<TABLE>& system_):
		m_ve(ve_), m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_)
	{
		m_system.merge(Update(m_ve), h_);
	}
private:
	PRL_HANDLE m_ve;
	Perspective<TABLE> m_system;
};

} // namespace Traffic
} // namespace Network

///////////////////////////////////////////////////////////////////////////////
// struct Unit

Unit::Unit(PRL_HANDLE ve_, const table_type::key_type& key_, const space_type& space_):
	Environment(ve_), m_state(NULL), m_tuple(new table_type::tuple_type(key_)),
	m_table(space_.get<0>())
{
	tableSP_type t = m_table.lock();
	if (NULL == t.get() || t->insert(m_tuple))
		m_tuple.reset();
	else
	{
		Perspective<CPU::TABLE> c(m_tuple, space_.get<3>());
		Perspective<Disk::TABLE> d(m_tuple, space_.get<1>());
		Perspective<Network::TABLE> n(m_tuple, space_.get<2>());
		Perspective<Counters::Linux::TABLE> f(m_tuple, space_.get<4>());
		// state
		m_state = new State(ve_, m_tuple);
		addState(m_state);
		addState(new Type(m_tuple));
		addState(new Name(m_tuple));
		addState(new Provenance(m_tuple));
		addState(new CPU::Number(m_tuple));
		addState(new CPU::Limit(m_tuple));
		addState(new CPU::Units(m_tuple));
		// usage
		addQueryUsage(new Memory::Query(m_tuple));
		addEventUsage(new Memory::Event(m_tuple));
		addEventUsage(new Disk::Io(ve_, d));
		addQueryUsage(new Disk::Space(d));
		addQueryUsage(new Network::Traffic::Query(ve_, n));
		addEventUsage(new Network::Traffic::Event(ve_, n));
		addQueryUsage(new CPU::Usage(m_tuple, c));
		addEventUsage(new CPU::Virtual::Event(c));
		addQueryUsage(new Counters::Linux::Query(ve_, m_tuple, f));
		// report
		const netsnmp_index& k = m_tuple->key();
		addValue(new Value::Composite::Range<TABLE>(
				Oid_type(k.oids, k.oids + k.len), m_table));
		addValue(c.value());
		addValue(d.value());
		addValue(n.value());
		addValue(f.value());
	}
}

Unit::~Unit()
{
	PRL_HANDLE j = PrlVm_UnsubscribeFromPerfStats(h());
	PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
	(void)e;
	PrlHandle_Free(j);
	tableSP_type t = m_table.lock();
	if (NULL != t.get() && m_tuple.get() != NULL)
		t->erase(*m_tuple);
}

bool Unit::uuid(std::string& dst_) const
{
	dst_ = Sdk::getString(boost::bind(&PrlVmCfg_GetUuid, h(), _1, _2));
	return dst_.empty();
}

void Unit::pullState()
{
	PRL_HANDLE j = PrlVm_RefreshConfig(h());
	if (PRL_INVALID_HANDLE == j)
		return;

	PRL_RESULT e = PrlJob_Wait(j, Sdk::TIMEOUT);
	if (PRL_SUCCEEDED(e))
		Environment::pullState();

	PrlHandle_Free(j);
}

void Unit::pullUsage()
{
//	PRL_HANDLE r = Sdk::getAsyncResult(PrlVm_GetStatisticsEx(h(), PVMSF_HOST_DISK_SPACE_USAGE_ONLY));
	PRL_HANDLE r = Sdk::getAsyncResult(PrlVm_GetStatistics(h()));
	if (PRL_INVALID_HANDLE != r)
	{
		refresh(r);
		PrlHandle_Free(r);
	}
}

void Unit::state(PRL_HANDLE event_)
{
	if (NULL != m_state)
		m_state->extract(event_);
}

bool Unit::inject(space_type& dst_)
{
	typedef Table::Handler::ReadOnly<TABLE> handler_type;
	typedef Table::Handler::ReadOnly<CPU::TABLE> vcpuHandler_type;
	typedef Table::Handler::ReadOnly<Disk::TABLE> diskHandler_type;
	typedef Table::Handler::ReadOnly<Network::TABLE> networkHandler_type;
	typedef Table::Handler::ReadOnly<Counters::Linux::TABLE> linCounterHandler_type;

	tableSP_type v(new table_type);
	if (v->attach(new handler_type(v)))
		return true;

	vcpuHandler_type::tableSP_type c(new vcpuHandler_type::tableSP_type::element_type());
	if (c->attach(new vcpuHandler_type(c)))
		return true;

	diskHandler_type::tableSP_type d(new diskHandler_type::tableSP_type::element_type());
	if (d->attach(new diskHandler_type(d)))
		return true;

	networkHandler_type::tableSP_type n(new networkHandler_type::tableSP_type::element_type());
	if (n->attach(new networkHandler_type(n)))
		return true;

	linCounterHandler_type::tableSP_type f(new linCounterHandler_type::tableSP_type::element_type());
	if (f->attach(new linCounterHandler_type(f)))
		return true;

	dst_.get<0>() = v;
	dst_.get<1>() = d;
	dst_.get<2>() = n;
	dst_.get<3>() = c;
	dst_.get<4>() = f;
	return false;
}

} // namespace VE
} // namespace Rmond

