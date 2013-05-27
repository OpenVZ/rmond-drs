#include "sink.h"
#include "value.h"
#include <sstream>
#include <boost/foreach.hpp>

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Schema<Sink::TABLE>

Oid_type Schema<Sink::TABLE>::uuid()
{
	return Schema<void>::uuid(51);
}

const char* Schema<Sink::TABLE>::name()
{
	return TOKEN_PREFIX"sinx";
}

netsnmp_handler_registration* Schema<Sink::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<Sink::TABLE>(handler_, my_, HANDLER_CAN_RWRITE);
}

///////////////////////////////////////////////////////////////////////////////
// struct Schema<Metrix::TABLE>

Oid_type Schema<Metrix::TABLE>::uuid()
{
	return Schema<void>::uuid(52);
}

const char* Schema<Metrix::TABLE>::name()
{
	return TOKEN_PREFIX"metrix";
}

netsnmp_handler_registration* Schema<Metrix::TABLE>::handler(Netsnmp_Node_Handler* handler_, void* my_)
{
	return Schema<void>::table<Metrix::TABLE>(handler_, my_, HANDLER_CAN_RWRITE);
}

namespace Sink
{
///////////////////////////////////////////////////////////////////////////////
// struct Unit

Unit::Unit(table_type::tupleSP_type tuple_, Metrix::tableWP_type metrix_):
	m_session(NULL), m_metrix(metrix_), m_tuple(tuple_)
{
	if (NULL == m_tuple.get() || m_tuple->get<PORT>() == 0)
		return;

	netsnmp_session x = {};
	snmp_sess_init(&x);
	std::ostringstream y;
	y << "udp:" << m_tuple->get<HOST>() << ":" << m_tuple->get<PORT>();
	std::string z = y.str();
	x.version = SNMP_VERSION_2c;
	x.peername = &z[0];
	x.remote_port = m_tuple->get<PORT>();

	m_session = snmp_sess_open(&x);
}

Unit::~Unit()
{
	snmp_sess_close(m_session);
}

unsigned Unit::limit() const
{
	if (NULL == m_tuple.get())
		return 0;

	int x = m_tuple->get<LIMIT>();
	if (0 < x)
		return x;

	return (std::numeric_limits<int>::max)();
}

Value::Metrix_type Unit::metrix() const
{
	Metrix::tableSP_type m = m_metrix.lock();
	if (NULL == m.get() || m_tuple.get() == NULL)
		return Value::Metrix_type();

	Value::Metrix_type output;
	BOOST_FOREACH(Metrix::table_type::tupleSP_type y, m->range(m_tuple->key()))
	{
		output.insert(y->get<Metrix::METRIC>());
	}
	return output;
}

ReaperSP Unit::inject(ServerSP server_)
{
	typedef Table::Handler::Mutable<TABLE, Actor> handler_type;
	typedef Table::Handler::Mutable<Metrix::TABLE> metrixHandler_type;

	Metrix::tableSP_type m(new Metrix::table_type);
	if (m->attach(new metrixHandler_type(m)))
		return ReaperSP();

	tableSP_type s(new table_type);
	ReaperSP output(new Reaper(s, m));
	if (s->attach(new handler_type(Actor(m, output, server_), s)))
		output.reset();

	return output;
}

bool Unit::push(netsnmp_variable_list* list_) const
{
	if (NULL == m_session)
	{
		snmp_free_varbind(list_);
		return true;
	}
	netsnmp_variable_list* x = list_;
	if (NULL != m_tuple.get() && !m_tuple->get<TICKET>().empty())
	{
		netsnmp_variable_list* y = 
			Value::Cell::Unit<Sink::TABLE, Sink::TICKET>(m_tuple).make();
		if (NULL != y)
		{
			y->next_variable = x;
			x = y;
		}
	}
	netsnmp_pdu* u = Value::Trap::pdu(x);
	if (NULL == u)
		return true;
	if (0 != snmp_sess_async_send(m_session, u, NULL, NULL))
		return false;

	snmp_free_pdu(u);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// struct Inform

Inform::Inform(table_type::tupleSP_type sink_, Metrix::tableWP_type metrix_,
		ServerWP server_): m_server(server_), m_metrix(metrix_),
		m_sink(sink_)
{
}

void Inform::operator()() const
{
	table_type::tupleSP_type t = m_sink.lock();
	if (NULL == t.get())
		return;

	unsigned a = t->get<ACKS>();
	if (0 == a)
		return;

	push(t);
	t->put<ACKS>(a - 1);
	Central::schedule(t->get<Sink::PERIOD>(), *this);
}

///////////////////////////////////////////////////////////////////////////////
// struct Actor

Actor::Actor(Metrix::tableSP_type metrix_, ReaperSP reaper_, ServerSP server_):
	m_server(server_), m_metrix(metrix_), m_reaper(reaper_)
{
}

void Actor::commit(Table::Request::Unit<TABLE> event_)
{
	Sink::table_type::tupleSP_type i = event_.inserted();
	if (NULL != i.get())
	{
		ReaperSP r = m_reaper.lock();
		if (NULL != r.get())
			r->track(i);
		Central::schedule(i->get<Sink::PERIOD>(),
				Inform(i, m_metrix, m_server));
	}
	event_.commit();
}

void Actor::reserve(Table::Request::Unit<TABLE> )
{
}

///////////////////////////////////////////////////////////////////////////////
// struct Reaper

void Reaper::do_()
{
	if (m_sinkList.empty())
		return;

	sinkList_type z;
	boost::mutex::scoped_lock g(m_lock);
	BOOST_FOREACH(tupleWP_type x, m_sinkList)
	{
		table_type::tupleSP_type y = x.lock();
		if (NULL == y.get())
			continue;
		if (0 < y->get<ACKS>())
		{
			z.push_back(y);
			continue;
		}
		BOOST_FOREACH(Metrix::table_type::tupleSP_type m,
				m_metrix->range(y->key()))
		{
			m_metrix->erase(*m);
		}
		m_table->erase(*y);
	}
	m_sinkList.swap(z);
}

void Reaper::track(table_type::tupleSP_type sink_)
{
	if (NULL == sink_.get())
		return;

	boost::mutex::scoped_lock g(m_lock);
	m_sinkList.push_back(sink_);
}

} // namespace Sink
} // namespace Rmond

