#ifndef SINK_H
#define SINK_H

#include "value.h"
#include "handler.h"
#include <boost/tuple/tuple.hpp>

namespace Rmond
{
namespace Sink
{
enum TABLE
{
	HOST = 1,
	PORT,
	PERIOD,
	LIMIT,
	ACKS,
	ROW_STATUS,
	TICKET
};
} // namespace Sink

namespace Metrix
{
enum TABLE
{
	METRIC = 1,
	ROW_STATUS
};
} // namespace Metrix

///////////////////////////////////////////////////////////////////////////////
// struct Schema<Sink::TABLE>

template<>
struct Schema<Sink::TABLE>: mpl::vector<
			Declaration<Sink::TABLE, Sink::HOST, ASN_OCTET_STR>,
			Declaration<Sink::TABLE, Sink::PORT, ASN_INTEGER>,
			Declaration<Sink::TABLE, Sink::PERIOD, ASN_INTEGER>,
			Declaration<Sink::TABLE, Sink::LIMIT, ASN_INTEGER>,
			Declaration<Sink::TABLE, Sink::ACKS, ASN_INTEGER>,
			Declaration<Sink::TABLE, Sink::TICKET, ASN_OCTET_STR>,
			Declaration<Sink::TABLE, Sink::ROW_STATUS, ASN_INTEGER> >
{
	typedef mpl::vector<
			mpl::integral_c<Sink::TABLE, Sink::HOST>,
			mpl::integral_c<Sink::TABLE, Sink::PORT>
		> index_type;

	static const Sink::TABLE ROW_STATUS = Sink::ROW_STATUS;

	static Oid_type uuid();
	static const char* name();
	static netsnmp_handler_registration* handler(Netsnmp_Node_Handler* handler_, void* my_);
};

///////////////////////////////////////////////////////////////////////////////
// struct Schema<Metrix::TABLE>

template<>
struct Schema<Metrix::TABLE>: mpl::vector<
			Declaration<Metrix::TABLE, Metrix::METRIC, ASN_OBJECT_ID>,
			Declaration<Metrix::TABLE, Metrix::ROW_STATUS, ASN_INTEGER> >
{
	typedef mpl::vector<
			mpl::integral_c<Sink::TABLE, Sink::HOST>,
			mpl::integral_c<Sink::TABLE, Sink::PORT>,
			mpl::integral_c<Metrix::TABLE, Metrix::METRIC>
		> index_type;

	static const Metrix::TABLE ROW_STATUS = Metrix::ROW_STATUS;

	static Oid_type uuid();
	static const char* name();
	static netsnmp_handler_registration* handler(Netsnmp_Node_Handler* handler_, void* my_);
};

namespace Metrix
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
} // namespace Metrix

namespace Sink
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
typedef boost::weak_ptr<table_type::tuple_type> tupleWP_type;

///////////////////////////////////////////////////////////////////////////////
// struct Reaper

struct Reaper
{
	Reaper(tableSP_type table_, Metrix::tableSP_type metrix_):
		m_table(table_), m_metrix(metrix_)
	{
	}

	void do_();
	void track(table_type::tupleSP_type sink_);
private:
	typedef std::list<tupleWP_type> sinkList_type;

	boost::mutex m_lock;
	tableSP_type m_table;
	sinkList_type m_sinkList;
	Metrix::tableSP_type m_metrix;
};
typedef boost::shared_ptr<Reaper> ReaperSP;

///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit
{
	Unit(table_type::tupleSP_type tuple_, Metrix::tableWP_type metrix_):
		m_metrix(metrix_), m_tuple(tuple_)
	{
	}

	unsigned limit() const;
	netsnmp_session* session() const;
	Value::Metrix_type metrix() const;
	netsnmp_variable_list* ticket(netsnmp_variable_list* list_) const;

	static ReaperSP inject(ServerSP server_);
private:
	Metrix::tableWP_type m_metrix;
	table_type::tupleSP_type m_tuple;
};

///////////////////////////////////////////////////////////////////////////////
// struct Inform

struct Inform
{
	Inform(table_type::tupleSP_type sink_, Metrix::tableWP_type metrix_,
		ServerWP server_);

	void operator()() const;
private:
	void push(const Unit& ) const;

	ServerWP m_server;
	Metrix::tableWP_type m_metrix;
	tupleWP_type m_sink;
};

///////////////////////////////////////////////////////////////////////////////
// struct Actor

struct Actor
{
	Actor(Metrix::tableSP_type metrix_, ReaperSP reaper_, ServerSP server_);

	void commit(Table::Request::Unit<TABLE> event_);
	void reserve(Table::Request::Unit<TABLE> );
private:
	ServerWP m_server;
	Metrix::tableWP_type m_metrix;
	boost::weak_ptr<Reaper> m_reaper;
	
};
} // namespace Sink
} // namespace Rmond

#endif // SINK_H

