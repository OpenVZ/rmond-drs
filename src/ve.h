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

#ifndef VE_H
#define VE_H

#include "environment.h"
#include <boost/tuple/tuple.hpp>

namespace Rmond
{
namespace VE
{
enum TABLE
{
	VEID = 1,
	NAME,
	STATE,
	PERFECT_NODE,
	MEMORY_TOTAL,
	MEMORY_USAGE,
	SWAP_TOTAL,
	SWAP_USAGE,
	CPU_NUMBER,
	CPU_LIMIT,
	CPU_UNITS,
	CPU_SYSTEM,
	CPU_USER,
	TYPE,
	OS_TYPE,
	UUID
};

namespace Disk
{
enum TABLE
{
	NAME = 1,
	TOTAL,
	USAGE,
	READ_REQUESTS,
	WRITE_REQUESTS,
	READ_BYTES,
	WRITE_BYTES,
	HASH1,
	HASH2
};

} // namespace Disk

namespace Network
{
enum TABLE
{
	NAME = 1,
	IN_BYTES,
	OUT_BYTES,
	IN_PACKETS,
	OUT_PACKETS,
	MAC
};

} // namespace Network

namespace CPU
{
enum TABLE
{
	ORDINAL = 1,
	TIME
};

} // namespace CPU
} // namespace VE

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::TABLE>

template<>
struct Schema<VE::TABLE>: mpl::vector<
			Declaration<VE::TABLE, VE::VEID, ASN_OCTET_STR>,
			Declaration<VE::TABLE, VE::TYPE, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::OS_TYPE, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::NAME, ASN_OCTET_STR>,
			Declaration<VE::TABLE, VE::UUID, ASN_OCTET_STR>,
			Declaration<VE::TABLE, VE::STATE, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::PERFECT_NODE, ASN_OCTET_STR>,
			Declaration<VE::TABLE, VE::MEMORY_TOTAL, ASN_COUNTER64>,
			Declaration<VE::TABLE, VE::MEMORY_USAGE, ASN_COUNTER64>,
			Declaration<VE::TABLE, VE::SWAP_TOTAL, ASN_COUNTER64>,
			Declaration<VE::TABLE, VE::SWAP_USAGE, ASN_COUNTER64>,
			Declaration<VE::TABLE, VE::CPU_NUMBER, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::CPU_LIMIT, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::CPU_UNITS, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::CPU_SYSTEM, ASN_INTEGER>,
			Declaration<VE::TABLE, VE::CPU_USER, ASN_INTEGER> >

{
	typedef mpl::vector<
			mpl::integral_c<VE::TABLE, VE::VEID>
		> index_type;

	static Oid_type uuid();
	static const char* name();
	static netsnmp_handler_registration* handler(Netsnmp_Node_Handler* handler_, void* my_);
};

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::Disk::TABLE>

template<>
struct Schema<VE::Disk::TABLE>: mpl::vector<
			Declaration<VE::Disk::TABLE, VE::Disk::HASH1, ASN_COUNTER>,
			Declaration<VE::Disk::TABLE, VE::Disk::HASH2, ASN_COUNTER>,
			Declaration<VE::Disk::TABLE, VE::Disk::NAME, ASN_OCTET_STR>,
			Declaration<VE::Disk::TABLE, VE::Disk::TOTAL, ASN_COUNTER64>,
			Declaration<VE::Disk::TABLE, VE::Disk::USAGE, ASN_COUNTER64>,
			Declaration<VE::Disk::TABLE, VE::Disk::READ_REQUESTS, ASN_COUNTER64>,
			Declaration<VE::Disk::TABLE, VE::Disk::WRITE_REQUESTS, ASN_COUNTER64>,
			Declaration<VE::Disk::TABLE, VE::Disk::READ_BYTES, ASN_COUNTER64>,
			Declaration<VE::Disk::TABLE, VE::Disk::WRITE_BYTES, ASN_COUNTER64> >

{
	typedef mpl::vector<
			mpl::integral_c<VE::TABLE, VE::VEID>,
			mpl::integral_c<VE::Disk::TABLE, VE::Disk::HASH1>,
			mpl::integral_c<VE::Disk::TABLE, VE::Disk::HASH2>
		> index_type;

	static Oid_type uuid();
	static const char* name();
	static netsnmp_handler_registration* handler(Netsnmp_Node_Handler* handler_, void* my_);
};

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::Network::TABLE>

template<>
struct Schema<VE::Network::TABLE>: mpl::vector<
			Declaration<VE::Network::TABLE, VE::Network::NAME, ASN_OCTET_STR>,
			Declaration<VE::Network::TABLE, VE::Network::IN_BYTES, ASN_COUNTER64>,
			Declaration<VE::Network::TABLE, VE::Network::OUT_BYTES, ASN_COUNTER64>,
			Declaration<VE::Network::TABLE, VE::Network::IN_PACKETS, ASN_COUNTER64>,
			Declaration<VE::Network::TABLE, VE::Network::OUT_PACKETS, ASN_COUNTER64>,
			Declaration<VE::Network::TABLE, VE::Network::MAC, ASN_OCTET_STR> >

{
	typedef mpl::vector<
			mpl::integral_c<VE::TABLE, VE::VEID>,
			mpl::integral_c<VE::Network::TABLE, VE::Network::NAME>
		> index_type;

	static Oid_type uuid();
	static const char* name();
	static netsnmp_handler_registration* handler(Netsnmp_Node_Handler* handler_, void* my_);
};

///////////////////////////////////////////////////////////////////////////////
// struct Schema<VE::CPU::TABLE>

template<>
struct Schema<VE::CPU::TABLE>: mpl::vector<
			Declaration<VE::CPU::TABLE, VE::CPU::ORDINAL, ASN_INTEGER>,
			Declaration<VE::CPU::TABLE, VE::CPU::TIME, ASN_COUNTER64> >

{
	typedef mpl::vector<
			mpl::integral_c<VE::TABLE, VE::VEID>,
			mpl::integral_c<VE::CPU::TABLE, VE::CPU::ORDINAL>
		> index_type;

	static Oid_type uuid();
	static const char* name();
	static netsnmp_handler_registration* handler(Netsnmp_Node_Handler* handler_, void* my_);
};

namespace VE
{
typedef Table::Unit<TABLE> table_type;
typedef boost::weak_ptr<table_type> tableWP_type;
typedef boost::shared_ptr<table_type> tableSP_type;
typedef table_type::tupleSP_type tupleSP_type;
typedef boost::weak_ptr<table_type::tuple_type> tupleWP_type;
typedef boost::tuple<tableSP_type,
		boost::shared_ptr<Table::Unit<Disk::TABLE> >,
		boost::shared_ptr<Table::Unit<Network::TABLE> >,
		boost::shared_ptr<Table::Unit<CPU::TABLE> > > space_type;

struct State;
///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit: Environment
{
	Unit(PRL_HANDLE ve_, const table_type::key_type& key_, const space_type& space_);
	~Unit();

	void pullState();
	void pullUsage();
	void state(PRL_HANDLE event_);
	bool uuid(std::string& dst_) const;

	static bool inject(space_type& dst_);
private:
	State* m_state;
	tupleSP_type m_tuple;
	tableWP_type m_table;
};
typedef boost::shared_ptr<Unit> UnitSP;

} // namespace VE
} // namespace Rmond

#endif // VE_H

