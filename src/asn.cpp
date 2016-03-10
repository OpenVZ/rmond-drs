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

#include "asn.h"

namespace Rmond
{
namespace Asn
{
namespace Policy
{
///////////////////////////////////////////////////////////////////////////////
// struct IP

void IP::get(in_addr_t src_, netsnmp_variable_list& dst_)
{
	u_long n = htonl(src_);
	snmp_set_var_typed_value(&dst_, ASN_IPADDRESS, (u_char* )&n, sizeof(in_addr_t));
}

void IP::put(const netsnmp_variable_list& src_, in_addr_t& dst_)
{
	dst_ = ntohl(*src_.val.integer);
}

///////////////////////////////////////////////////////////////////////////////
// struct Counter

void Counter::get(unsigned long long src_, netsnmp_variable_list& dst_)
{
	counter64 x;
	x.low = src_ & 0xffffffff;
	x.high = src_ >> 32;
	snmp_set_var_typed_value(&dst_, ASN_COUNTER64, (u_char* )&x, sizeof(counter64));
}

void Counter::put(const netsnmp_variable_list& src_, unsigned long long& dst_)
{
	dst_ = src_.val.counter64->high;
	dst_ =(dst_ << 32) + src_.val.counter64->low;
}

///////////////////////////////////////////////////////////////////////////////
// struct String

void String::get(const std::string& src_, netsnmp_variable_list& dst_)
{
	snmp_set_var_typed_value(&dst_, ASN_OCTET_STR,
		(u_char* )src_.data(), src_.size());
}

void String::put(const netsnmp_variable_list& src_, std::string& dst_)
{
	dst_.assign((const char* )src_.val.string, src_.val_len);
}

///////////////////////////////////////////////////////////////////////////////
// struct ObjectId

void ObjectId::get(const value_type& src_, netsnmp_variable_list& dst_)
{
	if (src_.empty())
		snmp_set_var_typed_value(&dst_, ASN_OBJECT_ID, NULL, 0);
	else
	{
		snmp_set_var_typed_value(&dst_, ASN_OBJECT_ID,
				(u_char* )&src_[0], src_.size()*sizeof(src_[0]));
	}
}
void ObjectId::put(const netsnmp_variable_list& src_, value_type& dst_)
{
	dst_.assign(src_.val.objid, src_.val.objid + src_.val_len / sizeof(oid));
}

} // namespace Policy
} // namespace Asn
} // namespace Rmond

