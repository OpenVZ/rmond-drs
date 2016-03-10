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

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include "value.h"

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Environment

struct Environment: Value::Composite::Base, Value::Storage
{
	explicit Environment(PRL_HANDLE h_);
	virtual ~Environment();

	void refresh(PRL_HANDLE performance_);
	Value::Provider* snapshot(const Value::Metrix_type& metrix_) const;

	virtual void pullState() = 0;
	virtual void pullUsage() = 0;
protected:
	PRL_HANDLE h() const
	{
		return m_h;
	}
	void addState(Value::Storage* value_)
	{
		m_stateList.push_back(value_);
	}
	void addValue(Value::Composite::Base* value_)
	{
		m_providerList.push_back(value_);
	}
	void addEventUsage(Value::Storage* value_)
	{
		m_eventList.push_back(value_);
	}
	void addQueryUsage(Value::Storage* value_)
	{
		m_queryList.push_back(value_);
	}
private:
	typedef boost::ptr_list<Value::Storage> valueList_type;
	typedef boost::ptr_list<Value::Composite::Base> providerList_type;

	PRL_HANDLE m_h;
	valueList_type m_eventList;
	valueList_type m_queryList;
	valueList_type m_stateList;
	providerList_type m_providerList;
};

} // namespace Rmond

#endif // ENVIRONMENT_H

