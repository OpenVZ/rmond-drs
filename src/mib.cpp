#include "host.h"
#include "sink.h"
#include <limits>
#include "system.h"
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace
{
std::map<uintptr_t, Rmond::ServerSP> g_active;
pthread_mutex_t g_big = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_ves = PTHREAD_MUTEX_INITIALIZER;

} // namespace

namespace Rmond
{
enum
{
	REAPER_TIMEOUT = 5,
	CONNECT_TIMEOUT = 30,
	COLLECT_TIMEOUT = 20
};

///////////////////////////////////////////////////////////////////////////////
// struct Server declaration

struct Server: boost::enable_shared_from_this<Server>, Details::Automat<Server, PRL_HANDLE>
{
	void pull(PRL_HANDLE event_);
	void erase(PRL_HANDLE event_);
	void state(PRL_HANDLE event_);
	void detach(PRL_HANDLE event_);
	void performance(PRL_HANDLE event_);

	bool attach(PRL_HANDLE host_);
	void  snapshot(const Value::Metrix_type& metrix_, boost::ptr_list<Value::Provider>& dst_) const;
	static ServerSP inject();

	typedef mpl::vector<
			Row<PET_DSP_EVT_DISP_CONNECTION_CLOSED, &Server::detach>,
			Row<PET_DSP_EVT_DISP_SHUTDOWN, &Server::detach>,
			Row<PET_DSP_EVT_VM_PERFSTATS, &Server::performance>,
			Row<PET_DSP_EVT_VM_DELETED, &Server::erase>,
			Row<PET_DSP_EVT_VM_UNREGISTERED, &Server::erase>,
			Row<PET_DSP_EVT_VM_ADDED, &Server::pull>,
			Row<PET_DSP_EVT_VM_STATE_CHANGED, &Server::state>,
			Row<PET_DSP_EVT_VM_CONFIG_CHANGED, &Server::pull>,
			Row<PET_DSP_EVT_VM_STARTED, &Server::pull>,
			Row<PET_DSP_EVT_VM_STOPPED, &Server::pull>,
			Row<PET_DSP_EVT_VM_TOOLS_STATE_CHANGED, &Server::pull>,
			Row<PET_DSP_EVT_VM_MIGRATE_FINISHED, &Server::pull>,
			Row<PET_DSP_EVT_VM_PAUSED, &Server::pull>,
			Row<PET_DSP_EVT_VM_SUSPENDED, &Server::pull>,
			Row<PET_DSP_EVT_VM_RESETED, &Server::pull>,
			Row<PET_DSP_EVT_VM_ABORTED, &Server::pull>,
			Row<PET_DSP_EVT_VM_MIGRATE_STARTED, &Server::pull>,
			Row<PET_DSP_EVT_VM_CONTINUED, &Server::pull>,
			Row<PET_DSP_EVT_VM_RESUMED, &Server::pull>
		>::type table_type;
private:
	typedef std::map<std::string, VE::UnitSP> veMap_type;

	Server(const Host::space_type& host_, const VE::space_type& ves_);

	static PRL_RESULT PRL_CALL handle(PRL_HANDLE , PRL_VOID_PTR );
	
	PRL_HANDLE m_psdk;
	std::pair<VE::space_type, veMap_type> m_ves;
	std::pair<Host::space_type, Host::UnitSP> m_host;
};

namespace Handler
{
///////////////////////////////////////////////////////////////////////////////
// struct Link

struct Link
{
	explicit Link(ServerSP server_): m_server(server_)
	{
	}

	void operator()() const;
	void reschedule() const;
private:
	ServerSP m_server;
};

void Link::reschedule() const
{
	Central::schedule(CONNECT_TIMEOUT, *this);
}

void Link::operator()() const
{
	PRL_RESULT r = PRL_ERR_FAILURE;
	PRL_HANDLE s = PRL_INVALID_HANDLE, j = PRL_INVALID_HANDLE;
	do
	{
		PRL_RESULT e = PrlSrv_Create(&s);
		if (PRL_FAILED(e))
			break;

		j = PrlSrv_LoginLocalEx(s, "", 0, PSL_HIGH_SECURITY, PACF_NON_INTERACTIVE_MODE);
		if (PRL_INVALID_HANDLE == j)
			break;

		e = PrlJob_Wait(j, Sdk::TIMEOUT);
		if (PRL_FAILED(e))
			break;

		PrlJob_GetRetCode(j, &r);
	} while(false);

	PrlHandle_Free(j);
	if (PRL_FAILED(r))
		PrlHandle_Free(s);
	else if (!m_server->attach(s))
		return;
	reschedule();
}

namespace Snatch
{
///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit
{
	typedef void (* impl_type)(boost::shared_ptr<Environment>);

	Unit(boost::shared_ptr<Environment> environment_, impl_type impl_):
		m_impl(impl_), m_environment(environment_)
	{
	}

	void operator()() const
	{
		boost::shared_ptr<Environment> e = m_environment.lock();
		if (NULL != e.get())
			m_impl(e);
	}
private:
	impl_type m_impl;
	boost::weak_ptr<Environment> m_environment;
};

void pullState(boost::shared_ptr<Environment> ve_)
{
	Lock g(g_ves);
	ve_->pullState();
}

void pullUsage(boost::shared_ptr<Environment> ve_)
{
	Lock g(g_ves);
	ve_->pullUsage();
	Central::schedule(COLLECT_TIMEOUT, Unit(ve_, &pullUsage));
}

} // namespace Snatch

///////////////////////////////////////////////////////////////////////////////
// struct Reaper

struct Reaper
{
	explicit Reaper(Sink::ReaperSP reaper_): m_reaper(reaper_)
	{
	}

	void operator()() const
	{
		m_reaper->do_();
		Central::schedule(REAPER_TIMEOUT, *this);
	}
private:
	Sink::ReaperSP m_reaper;
};

} // namespace Handler

///////////////////////////////////////////////////////////////////////////////
// struct Server definition

ServerSP Server::inject()
{
	VE::space_type v;
	Host::space_type h;
	if (Host::Unit::inject(h) || VE::Unit::inject(v))
		return ServerSP();

	return ServerSP(new Server(h, v));
}

Server::Server(const Host::space_type& host_, const VE::space_type& ves_):
	m_psdk(PRL_INVALID_HANDLE)
{
	m_ves.first = ves_;
	m_host.first = host_;
}

bool Server::attach(PRL_HANDLE host_)
{
	std::auto_ptr<Host::Unit> h(new Host::Unit(host_, m_host.first));
	std::list<VE::UnitSP> a;
	if (h->list(a, m_ves.first))
		return true;

	SchedulerSP s = Central::scheduler();
	if (NULL == s.get())
		return false;

	Lock g(g_big);
	if (NULL != m_host.second.get())
		return false;

	m_psdk = host_;
	m_host.second.reset(h.release());
	s->push(Handler::Snatch::Unit(m_host.second, &Handler::Snatch::pullState));
	s->push(Handler::Snatch::Unit(m_host.second, &Handler::Snatch::pullUsage));
	BOOST_FOREACH(const VE::UnitSP& x, a)
	{
		std::string u;
		if (x->uuid(u))
			continue;

		m_ves.second[u] = x;
		s->push(Handler::Snatch::Unit(x, &Handler::Snatch::pullState));
		s->push(Handler::Snatch::Unit(x, &Handler::Snatch::pullUsage));
	}
	m_host.second->ves(m_ves.second.size());
	g_active[(uintptr_t)this] = shared_from_this();
	PRL_RESULT e = PrlSrv_RegEventHandler(m_psdk, &Server::handle, this);
	(void)e;
	return false;
}

void Server::detach(PRL_HANDLE )
{
	Lock g(g_big);
	if (0 < g_active.erase((uintptr_t)this))
	{
		PRL_RESULT e = PrlSrv_UnregEventHandler(m_psdk, &Server::handle, this);
		(void)e;
	}
	m_host.second.reset();
	m_psdk = PRL_INVALID_HANDLE;
	m_ves.second.clear();
	g.leave();
	Handler::Link(shared_from_this()).reschedule();
}

void Server::pull(PRL_HANDLE event_)
{
	SchedulerSP s = Central::scheduler();
	std::string d = Sdk::getIssuerId(event_);
	Lock g(g_big);
	veMap_type::iterator p = m_ves.second.find(d);
	if (m_ves.second.end() != p)
	{
		VE::UnitSP u = p->second;
		if (NULL != s.get() && u.get() != NULL)
			s->push(Handler::Snatch::Unit(u, &Handler::Snatch::pullState));

		return;
	}
	Host::UnitSP h = m_host.second;
	g.leave();
	if (NULL == h.get())
		return;

	VE::UnitSP u = h->find(d, m_ves.first);
	if (NULL == u.get())
		return;

	u->pullState();
	g.enter();
	m_ves.second[d] = u;
	m_host.second->ves(m_ves.second.size());
	if (NULL != s.get())
		s->push(Handler::Snatch::Unit(u, &Handler::Snatch::pullUsage));
}

void Server::state(PRL_HANDLE event_)
{
	Lock g(g_big);
	veMap_type::iterator v = m_ves.second.find(Sdk::getIssuerId(event_));
	if (m_ves.second.end() != v)
		v->second->state(event_);
}

void Server::performance(PRL_HANDLE event_)
{
	PRL_EVENT_ISSUER_TYPE t;
	PRL_RESULT r = PrlEvent_GetIssuerType(event_, &t);
	if (PRL_FAILED(r))
		return;

	PRL_UINT32 n = 0;
	boost::shared_ptr<Environment> e;
	r = PrlEvent_GetParamsCount(event_, &n);
	Lock g(g_big);
	switch (t)
	{
	case PIE_DISPATCHER:
		e = m_host.second;
		break;
	case PIE_VIRTUAL_MACHINE:
	{
		veMap_type::iterator v = m_ves.second.find(Sdk::getIssuerId(event_));
		if (m_ves.second.end() != v)
		{
			e = v->second;
			break;
		}
	}
	default:
		return;
	}
	if (NULL == e.get())
		return;
	if (~0U == n)
		return e->refresh(event_);

	while(n-- > 0)
	{
		PRL_HANDLE p = PRL_INVALID_HANDLE;
		r = PrlEvent_GetParam(event_, n, &p);
		if (PRL_FAILED(r))
			continue;
		e->refresh(p);
		PrlHandle_Free(p);
	}
}

void Server::erase(PRL_HANDLE event_)
{
	Lock g(g_big);
	m_ves.second.erase(Sdk::getIssuerId(event_));
	m_host.second->ves(m_ves.second.size());
}

void Server::snapshot(const Value::Metrix_type& metrix_, boost::ptr_list<Value::Provider>& dst_) const
{
	Lock a(g_big);
	if (NULL == m_host.second.get())
		return;

	Lock b(g_ves);
	Value::Provider* h = m_host.second->snapshot(metrix_);
	if (NULL != h)
		dst_.push_back(h);

	BOOST_FOREACH(veMap_type::const_reference r, m_ves.second)
	{
		Value::Provider* u = r.second->snapshot(metrix_);
		if (NULL != u)
			dst_.push_back(u);
	}
}

PRL_RESULT PRL_CALL Server::handle(PRL_HANDLE event_, PRL_VOID_PTR user_)
{
	ServerSP s;
	PRL_EVENT_TYPE t;
	PRL_HANDLE_TYPE e;
	if (PRL_SUCCEEDED(PrlHandle_GetType(event_, &e)) && e == PHT_EVENT)
	{
		if (PRL_SUCCEEDED(PrlEvent_GetType(event_, &t)))
		{
			Lock g(g_big);
			s = g_active[(uintptr_t)user_];
		}
	}
	if (NULL != s.get())
		s->do_(t, event_);

	PrlHandle_Free(event_);
	return PRL_ERR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// struct Central

Scheduler::UnitSP Central::s_scheduler;

bool Central::init()
{
	PRL_RESULT e = PrlApi_Init(PARALLELS_API_VER);
	if (PRL_FAILED(e) && e != PRL_ERR_DOUBLE_INIT)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot init the PrlSDK: 0x%x\n", e);
		return true;
	}
	else
	{
		Lock g(g_big);
		do
		{
			if (NULL != s_scheduler.get())
			{
				snmp_log(LOG_ERR, LOG_PREFIX"the MIB has "
						"already been initialized\n");
				return false;
			}
			ServerSP s = Server::inject();
			if (NULL == s.get())
				break;

			Sink::ReaperSP x = Sink::Unit::inject(s);
			if (NULL == x.get())
				break;

			Scheduler::UnitSP y(new Scheduler::Unit);
			if (y->go() || y->push(Handler::Link(s)))
				break;

			y->push(Handler::Reaper(x));
			s_scheduler = y;
			return false;
		} while(false);
	}
	PrlApi_Deinit();
	return true;
}

void Central::fini()
{
	Lock g(g_big);
	Scheduler::UnitSP x = s_scheduler;
	if (NULL != x.get())
	{
		s_scheduler.reset();
		g_active.clear();
		g.leave();
		PrlApi_Deinit();
		x->stop();
	}
}

Oid_type Central::traps()
{
	static const Oid_type::value_type NAME[] = {SNMP_OID_ENTERPRISES, 26171, 2};
	return Oid_type(NAME, NAME + sizeof(NAME)/sizeof(NAME[0]));
}

Oid_type Central::product()
{
	static const Oid_type::value_type NAME[] = {SNMP_OID_ENTERPRISES, 26171, 1, 1};
	return Oid_type(NAME, NAME + sizeof(NAME)/sizeof(NAME[0]));
}

SchedulerSP Central::scheduler()
{
	Lock g(g_big);
	return s_scheduler;
}

bool Central::schedule(unsigned timeout_, Scheduler::Queue::job_type job_)
{
	SchedulerSP x = scheduler();
	return NULL == x.get() || x->push(timeout_, job_);
}

namespace Sink
{
///////////////////////////////////////////////////////////////////////////////
// struct Inform

void Inform::push(table_type::tupleSP_type target_) const
{
	ServerSP z = m_server.lock();
	if (NULL == z.get())
		return;

	Unit u(target_, m_metrix);
	if (u.bad())
		return;

	int n = u.limit(), i = 0;
	boost::ptr_list<Value::Provider> d;
	z->snapshot(u.metrix(), d);
	netsnmp_variable_list a = {}, *b = &a;
	BOOST_FOREACH(const Value::Provider& p, d)
	{
		netsnmp_variable_list* v = p.make();
		if (NULL == v)
			continue;
		b->next_variable = v;
		for (; b->next_variable != NULL; b = b->next_variable) {}
		if (++i < n)
			continue;

		u.push(a.next_variable);
		i = 0;
		b = &a;
		a.next_variable = NULL;
	}
	u.push(a.next_variable);
}

} // namespace Sink
} // namespace Rmond

