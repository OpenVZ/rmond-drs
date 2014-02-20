#include "ve.h"
#include "system.h"
#include "handler.h"
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_array.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash/hash.hpp>
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
		Perspective<Disk::TABLE> d(m_tuple, space_.get<1>());
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
		addQueryUsage(new Memory(m_tuple));
		addEventUsage(new Disk::Io(ve_, d));
		addQueryUsage(new Disk::Space(d));
		addQueryUsage(new Network::Traffic::Query(ve_, n));
		addEventUsage(new Network::Traffic::Event(ve_, n));
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

