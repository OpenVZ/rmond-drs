#include "host.h"
#include "system.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>

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
		PRL_UINT32 x = 0;
		switch (t)
		{
		case PVT_VM:
			u = Sdk::getString(boost::bind(&PrlVmCfg_GetUuid, h_, _1, _2));
			break;
		case PVT_CT:
			r = PrlVmCfg_GetEnvId(h_, &x);
			if (PRL_SUCCEEDED(r))
			{
				std::ostringstream s;
				s << x;
				u = s.str();
			}
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

///////////////////////////////////////////////////////////////////////////////
// struct License

struct License: Value::Storage
{
	License(PRL_HANDLE host_, tupleSP_type tuple_):
		m_host(host_), m_tuple(tuple_)
	{
	}

	void refresh(PRL_HANDLE h_);
private:
	void ves(PRL_HANDLE event_);

	PRL_HANDLE m_host;
	tupleWP_type m_tuple;
};

void License::ves(PRL_HANDLE event_)
{
	PRL_UINT32 n = 0, v = 0;
	PrlEvent_GetParamsCount(event_, &n);
	while (n-- > 0)
	{
		PRL_HANDLE p = PRL_INVALID_HANDLE;
		PRL_RESULT r = PrlEvent_GetParam(event_, n, &p);
		if (PRL_FAILED(r))
			continue;

		std::string b = Sdk::getString(boost::bind(&PrlEvtPrm_GetName, p, _1, _2));
		if (0 == b.compare("vzlicense_vms_total"))
		{
			r = PrlEvtPrm_ToUint32(p, &v);
			PrlHandle_Free(p);
			break;
		}
		PrlHandle_Free(p);
	}
	tupleSP_type t = m_tuple.lock();
	if (NULL != t.get())
		t->put<LICENSE_VES>(v);
}

void License::refresh(PRL_HANDLE h_)
{
	PRL_STR s = NULL;
	PRL_RESULT r = PrlLic_ToString(h_, (PRL_VOID_PTR_PTR)&s);
	if (PRL_FAILED(r) || s == NULL)
		return;

	PRL_HANDLE v = PRL_INVALID_HANDLE;
	r = PrlSrv_CreateVm(m_host, &v);
	if (PRL_SUCCEEDED(r))
	{
		PRL_HANDLE e = PRL_INVALID_HANDLE;
		r = PrlVm_CreateAnswerEvent(v, &e, 0);
		if (PRL_SUCCEEDED(r = PrlVm_CreateAnswerEvent(v, &e, 0)) &&
			PRL_SUCCEEDED(r = PrlEvent_FromString(e, s)))
		{
			ves(e);
		}
		PrlHandle_Free(e);
		PrlHandle_Free(v);
	}
	PrlBuffer_Free(s);
}

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
	addUsage(new License(host_, m_data));
	// report
	addValue(new Value::Composite::Scalar<PROPERTY>(m_data));
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
	PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
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
		PRL_RESULT e = PrlJob_Wait(j, UINT_MAX);
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
}

} // namespace Host
} // namespace Rmond

