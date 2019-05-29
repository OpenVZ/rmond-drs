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

#include "asn.h"
#include "value.h"
#include <boost/foreach.hpp>

extern oid snmptrap_oid[];
extern int snmptrap_oid_len;
extern oid sysuptime_oid[];
extern int sysuptime_oid_len;
extern oid agentaddr_oid[];
extern int agentaddr_oid_len;

namespace Rmond
{
namespace Value
{
///////////////////////////////////////////////////////////////////////////////
// struct Provider

Provider::~Provider()
{
}

netsnmp_variable_list* Provider::make() const
{
	return SNMP_MALLOC_TYPEDEF(netsnmp_variable_list);
}

///////////////////////////////////////////////////////////////////////////////
// struct Uptime

struct Uptime: Provider
{
	netsnmp_variable_list* make() const;
};

netsnmp_variable_list* Uptime::make() const
{
	netsnmp_variable_list* output = Provider::make();
	if (NULL == output)
		return NULL;

	Asn::Policy::Integer<ASN_TIMETICKS>::get(netsnmp_get_agent_uptime(), *output);
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct TrapAddress

struct TrapAddress: Provider
{
	netsnmp_variable_list* make() const;
};

netsnmp_variable_list* TrapAddress::make() const
{
	netsnmp_variable_list* output = Provider::make();
	if (NULL == output)
		return NULL;

	Asn::Policy::IP::get(ntohl(get_myaddr()), *output);
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Trap

netsnmp_pdu* Trap::pdu(netsnmp_variable_list* data_)
{
	if (NULL == data_)
		return NULL;

	netsnmp_pdu* u = snmp_pdu_create(SNMP_MSG_TRAP2);
	while (NULL != u)
	{
		netsnmp_variable_list* h = Named(sysuptime_oid, sysuptime_oid_len,
							new Uptime).make();
		if (NULL == h)
			break;

		u->variables = h;
		h->next_variable = Named(snmptrap_oid, snmptrap_oid_len,
						new Trap).make();
		h = h->next_variable;
		if (NULL == h)
			break;

		h->next_variable = Named(agentaddr_oid, snmptrap_oid_len,
						new TrapAddress).make();
		h = h->next_variable;
		if (NULL == h)
			break;

		h->next_variable = data_;
#if 0
		for (h = u->variables; h != NULL; h = h->next_variable)
		{
			std::ostringstream o;
			for (size_t i = 0; i < h->name_length; ++i)
			{
				o << "." << h->name[i];
			}
			snmp_log(LOG_ERR, LOG_PREFIX"varbind int the trap %s\n", o.str().c_str());
		}
		snmp_log(LOG_ERR, LOG_PREFIX"\n\n");
#endif // 0
		u->version = SNMP_VERSION_2c;
		return u;
	}
	snmp_free_pdu(u);
	snmp_free_varbind(data_);
	return NULL;
}

netsnmp_variable_list* Trap::make() const
{
	netsnmp_variable_list* output = Provider::make();
	if (NULL == output)
		return NULL;

	Oid_type n = Central::traps();
	n.push_back(0);
	n.push_back(51);
	Asn::Policy::ObjectId::get(n, *output);
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Trivial

Trivial::Trivial(netsnmp_variable_list* value_): m_value(value_)
{
}

Trivial::~Trivial()
{
	snmp_free_varbind(m_value);
}

netsnmp_variable_list* Trivial::make() const
{
	return snmp_clone_varbind(m_value);
}

///////////////////////////////////////////////////////////////////////////////
// struct Named

Named::Named(const name_type& name_, Provider* value_):
	m_name(name_), m_value(value_)
{
}

Named::Named(const name_type& name_, netsnmp_variable_list* value_):
	m_name(name_), m_value(new Trivial(value_))
{
}

Named::Named(const oid* name_, size_t length_, Provider* value_):
	m_name(name_, name_ + length_), m_value(value_)
{
}

netsnmp_variable_list* Named::make() const
{
	if (NULL == m_value.get())
		return NULL;

	netsnmp_variable_list* output = m_value->make();
	if (NULL != output && !m_name.empty())
		snmp_set_var_objid(output, &m_name[0], m_name.size());

	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct List

netsnmp_variable_list* List::make() const
{
	netsnmp_variable_list *output = NULL, *x = NULL;
	boost::ptr_list<Provider>::const_iterator p = this->begin(), e = this->end();
	for (; p != e && x == NULL; ++p)
	{
		output = x = p->make();
	}
	for (; p != e; ++p)
	{
		for (; x->next_variable != NULL; x = x->next_variable) {}
		x->next_variable = p->make();
	}
	return output;
}

///////////////////////////////////////////////////////////////////////////////
// struct Storage

Storage::~Storage()
{
}

namespace Composite
{
///////////////////////////////////////////////////////////////////////////////
// struct Base

Base::~Base()
{
}

} // namespace Composite
} // namespace Value
} // namespace Rmond

