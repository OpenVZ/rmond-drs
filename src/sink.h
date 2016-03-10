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
	Unit(table_type::tupleSP_type tuple_, Metrix::tableWP_type metrix_);
	~Unit();

	bool bad() const
	{
		return NULL == m_session;
	}
	unsigned limit() const;
	Value::Metrix_type metrix() const;
	bool push(netsnmp_variable_list* list_) const;

	static ReaperSP inject(ServerSP server_);
private:
	void* m_session;
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
	void push(table_type::tupleSP_type target_) const;

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

