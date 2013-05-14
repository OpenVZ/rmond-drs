#include "ve.h"
#include "system.h"
#include "handler.h"
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_array.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/iterator/iterator_facade.hpp>

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

namespace VE
{
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

	std::string n = Sdk::getString(boost::bind(&PrlVmCfg_GetName, h_, _1, _2));
	if (!n.empty())
		y->put<NAME>(n);
}

///////////////////////////////////////////////////////////////////////////////
// struct Type

struct Type: Value::Storage
{
	explicit Type(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
	static bool extract(PRL_HANDLE h_, PRL_VM_TYPE& dst_);
private:
	tupleWP_type m_data;
};

void Type::refresh(PRL_HANDLE h_)
{
	tupleSP_type y = m_data.lock();
	if (NULL != y.get())
	{
		PRL_VM_TYPE x = PVT_VM;
		extract(h_, x);
		y->put<TYPE>(x);
	}
}

bool Type::extract(PRL_HANDLE h_, PRL_VM_TYPE& dst_)
{
	PRL_VM_TYPE t = PVT_VM;
	PRL_RESULT e = PrlVmCfg_GetVmType(h_, &t);
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
	if (Type::extract(h_, t))
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
		snmp_log(LOG_ERR, LOG_PREFIX"schaman status %d(%d):\n%s\n",
				WEXITSTATUS(s), s, e.str().c_str());
	}
}

///////////////////////////////////////////////////////////////////////////////
// struct Memory

struct Memory: Value::Storage
{
	explicit Memory(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
};

void Memory::refresh(PRL_HANDLE h_)
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

///////////////////////////////////////////////////////////////////////////////
// struct Usage

struct Usage: Value::Storage
{
	explicit Usage(tupleSP_type data_): m_data(data_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleWP_type m_data;
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
	}
	PrlHandle_Free(h);
}

} // namespace CPU

///////////////////////////////////////////////////////////////////////////////
// struct Perspective

template<class T>
struct Perspective
{
	typedef Table::Unit<T> table_type;

	Perspective(tupleSP_type parent_, boost::weak_ptr<table_type> table_);

	boost::shared_ptr<table_type> table()
	{
		return m_table.lock();
	}
	const typename table_type::key_type& uuid() const
	{
		return m_uuid;
	}
	Value::Composite::Base* value() const
	{
		return new Value::Composite::Range<T>(m_parent, m_table);
	}
	std::list<typename table_type::tupleSP_type> all();
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
std::list<typename Perspective<T>::table_type::tupleSP_type> Perspective<T>::all()
{
	boost::shared_ptr<table_type> x = table();
	if (NULL == x.get())
		return std::list<typename table_type::tupleSP_type>();

	return x->range(m_parent);
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
	explicit Device(PRL_HANDLE h_): m_h(h_)
	{
	}

	std::string name() const;
	PRL_UINT32 getStackIndex() const;
	PRL_MASS_STORAGE_INTERFACE_TYPE getIfType() const;
private:
	PRL_HANDLE m_h;
};

std::string Device::name() const
{
	return Sdk::getString(boost::bind(&PrlVmDev_GetSysName, m_h, _1, _2));
}

PRL_UINT32 Device::getStackIndex() const
{
	PRL_UINT32 output = 0;
	PRL_RESULT e = PrlVmDev_GetStackIndex(m_h, &output);
	(void)e;
	return output;
}

PRL_MASS_STORAGE_INTERFACE_TYPE Device::getIfType() const
{
	PRL_MASS_STORAGE_INTERFACE_TYPE output;
	PRL_RESULT e = PrlVmDev_GetIfaceType(m_h, &output);
	(void)e;
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Policy

struct Policy
{
	typedef boost::shared_array<PRL_HANDLE> list_type;
	typedef std::pair<list_type, size_t> value_type;

	static value_type make(PRL_HANDLE h_);
	static PRL_UINT32 count(const value_type& value_)
	{
		return value_.second;
	}
	static Device* take(const value_type& value_, PRL_UINT32 n_)
	{
		if (value_.second <= n_)
			return NULL;

		PRL_DEVICE_TYPE t;
		PRL_RESULT r = PrlVmDev_GetType(value_.first[n_], &t);
		if (PRL_FAILED(r) || t != PDE_HARD_DISK)
			return NULL;

		return new Device(value_.first[n_]);
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

Policy::value_type Policy::make(PRL_HANDLE h_)
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
// struct System

struct System: Perspective<TABLE>
{
	System(PRL_HANDLE ve_, VE::tupleSP_type parent_, const VE::space_type& space_);

	std::string device(PRL_MASS_STORAGE_INTERFACE_TYPE type_, PRL_UINT32 index_) const;
private:
	PRL_HANDLE m_ve;
};

System::System(PRL_HANDLE ve_, VE::tupleSP_type parent_, const VE::space_type& space_):
	Perspective<TABLE>(parent_, space_.get<1>()), m_ve(ve_)
{
}

std::string System::device(PRL_MASS_STORAGE_INTERFACE_TYPE type_, PRL_UINT32 index_) const
{
	typedef Iterator<List::Policy, List::Policy::value_type, List::Device>
			iterator_type;
	iterator_type p(List::Policy::make(m_ve)), e;
	for (; p != e; ++p)
	{
		if (type_ == p->getIfType() && p->getStackIndex() == index_)
			return p->name();
	}
	return std::string();
}

///////////////////////////////////////////////////////////////////////////////
// struct Space

struct Space: Value::Storage
{
	explicit Space(const System& system_): m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	System m_system;
};

void Space::refresh(PRL_HANDLE h_)
{
	tableSP_type z = m_system.table();
	if (NULL == z.get())
		return;

	std::map<std::string, tupleSP_type> x;
	Iterator<Usage::Policy, PRL_HANDLE, Usage::Device> e, p(h_);
	for (; p != e; ++p)
	{
		std::string n = p->name();
		if (n.empty())
			continue;
		table_type::key_type k = m_system.uuid();
		k.put<NAME>(n);
		tupleSP_type t(new tupleSP_type::value_type(k));
		t->put<USAGE>(p->getUsedBytes());
		t->put<TOTAL>(p->getUsedBytes() + p->getFreeBytes());
		x[n] = t;
	}
	if (x.empty())
		return;

	BOOST_FOREACH(tupleSP_type d, m_system.all())
	{
		tupleSP_type y = x[d->get<NAME>()];
		if (NULL == y.get())
			z->erase(*d);
		else
		{
			d->put<TOTAL>(y->get<TOTAL>());
			d->put<USAGE>(y->get<USAGE>());
			x.erase(d->get<NAME>());
		}
	}
	typedef std::map<std::string, tupleSP_type>::const_reference
			reference_type;
	BOOST_FOREACH(reference_type r, x)
	{
		z->insert(r.second);
	}
}

///////////////////////////////////////////////////////////////////////////////
// struct Io

struct Io: Value::Storage
{
	explicit Io(const System& system_): m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	tupleSP_type take(const std::string& counter_);

	System m_system;
};

void Io::refresh(PRL_HANDLE h_)
{
	std::string n = Sdk::getString(boost::bind(&PrlEvtPrm_GetName, h_, _1, _2));
	if (n.empty())
		return;
	table_type::tupleSP_type t = take(n);
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

tupleSP_type Io::take(const std::string& counter_)
{
	tableSP_type z = m_system.table();
	if (NULL == z.get())
		return tupleSP_type();

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
		return tupleSP_type();

	std::string n = m_system.device(a, b);
	if (n.empty())
		return tupleSP_type();

	table_type::key_type k = m_system.uuid();
	k.put<NAME>(n);
	tupleSP_type output = z->find(k);
	if (NULL == output.get())
	{
		output.reset(new tupleSP_type::value_type(k));
		if (z->insert(output))
			output.reset();
	}
	return output;
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

///////////////////////////////////////////////////////////////////////////////
// struct Traffic

struct Traffic: Value::Storage
{
	explicit Traffic(const Perspective<TABLE>& system_): m_system(system_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	Perspective<TABLE> m_system;
};

void Traffic::refresh(PRL_HANDLE h_)
{
	tableSP_type z = m_system.table();
	if (NULL == z.get())
		return;

	std::map<std::string, tupleSP_type> x;
	Iterator<Usage::Policy, PRL_HANDLE, Usage::Device> e, p(h_);
	for (; p != e; ++p)
	{
		std::string n = p->name();
		if (n.empty())
			continue;
		table_type::key_type k = m_system.uuid();
		k.put<NAME>(n);
		tupleSP_type t(new tupleSP_type::value_type(k));
		t->put<IN_BYTES>(p->getInBytes());
		t->put<OUT_BYTES>(p->getOutBytes());
		t->put<IN_PACKETS>(p->getInPackets());
		t->put<OUT_PACKETS>(p->getOutPackets());
		x[n] = t;
	}
	if (x.empty())
		return;

	BOOST_FOREACH(tupleSP_type d, m_system.all())
	{
		tupleSP_type y = x[d->get<NAME>()];
		if (NULL == y.get())
			z->erase(*d);
		else
		{
			d->put<IN_BYTES>(y->get<IN_BYTES>());
			d->put<OUT_BYTES>(y->get<OUT_BYTES>());
			d->put<IN_PACKETS>(y->get<IN_PACKETS>());
			d->put<OUT_PACKETS>(y->get<OUT_PACKETS>());
			x.erase(d->get<NAME>());
		}
	}
	typedef std::map<std::string, tupleSP_type>::const_reference
			reference_type;
	BOOST_FOREACH(reference_type r, x)
	{
		z->insert(r.second);
	}
}

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
		Disk::System d(ve_, m_tuple, space_);
		Perspective<Network::TABLE> n(m_tuple, space_.get<2>());
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
		addUsage(new Memory(m_tuple));
		addUsage(new Disk::Io(d));
		addUsage(new Disk::Space(d));
		addUsage(new Network::Traffic(n));
		// report
		const netsnmp_index& k = m_tuple->key();
		addValue(new Value::Composite::Range<TABLE>(
				Oid_type(k.oids, k.oids + k.len), m_table));
		addValue(d.value());
		addValue(n.value());
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

	PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
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
	typedef Table::Handler::ReadOnly<Disk::TABLE> diskHandler_type;
	typedef Table::Handler::ReadOnly<Network::TABLE> networkHandler_type;

	tableSP_type v(new table_type);
	if (v->attach(new handler_type(v)))
		return true;

	diskHandler_type::tableSP_type d(new diskHandler_type::tableSP_type::value_type);
	if (d->attach(new diskHandler_type(d)))
		return true;

	networkHandler_type::tableSP_type n(new networkHandler_type::tableSP_type::value_type);
	if (n->attach(new networkHandler_type(n)))
		return true;

	dst_.get<0>() = v;
	dst_.get<1>() = d;
	dst_.get<2>() = n;
	return false;
}

} // namespace VE
} // namespace Rmond

