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

#include "environment.h"
#include <boost/foreach.hpp>

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Environment

Environment::Environment(PRL_HANDLE h_): m_h(h_)
{
}

Environment::~Environment()
{
	PrlHandle_Free(m_h);
}

void Environment::pullState()
{
	BOOST_FOREACH(valueList_type::reference r, m_stateList)
	{
		r.refresh(m_h);
	}
}

void Environment::refresh(PRL_HANDLE performance_)
{
	PRL_HANDLE_TYPE t;
	if (PRL_FAILED(PrlHandle_GetType(performance_, &t)))
		return;

	valueList_type* v = NULL;
	switch (t)
	{
	case PHT_EVENT:
	case PHT_EVENT_PARAMETER:
		v = &m_eventList;
		break;
	default:
		v = &m_queryList;
	}
	BOOST_FOREACH(valueList_type::reference r, *v)
	{
		r.refresh(performance_);
	}
}

Value::Provider* Environment::snapshot(const Value::Metrix_type& metrix_) const
{
	Value::List* output = new Value::List;
	providerList_type::const_iterator e = m_providerList.end();
	providerList_type::const_iterator p = m_providerList.begin();
	for (; p != e; ++p)
	{
		Value::Provider* y = p->snapshot(metrix_);
		if (NULL != y)
			output->push_back(y);
	}
	return output;
}

} // namespace Rmond

