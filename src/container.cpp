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

#include "system.h"
#include "container.h"
#include <algorithm>

namespace Rmond
{
namespace ThreadsafeContainer
{
namespace
{

Unit* getData(netsnmp_container *ct_)
{
	return static_cast<Unit *>(ct_->container_data);
}

template <typename R, R (Unit::* F)()>
R delegate(netsnmp_container *ct_)
{
	Unit *a = getData(ct_);
	return (a->*F)();
}

template <typename R, typename V, R (Unit::* F)(V)>
R delegate(netsnmp_container *ct_, V arg_)
{
	Unit *a = getData(ct_);
	return (a->*F)(arg_);
}

template <typename R, typename V1, typename V2, R (Unit::* F)(V1, V2)>
R delegate(netsnmp_container *ct_, V1 arg1_, V2 arg2_)
{
	Unit *a = getData(ct_);
	return (a->*F)(arg1_, arg2_);
}

int cfree(netsnmp_container* ct_)
{
	if (ct_ == NULL)
		return -1;

	delete getData(ct_);
	::free(ct_);
	return 0; 
}

netsnmp_container* make()
{
	netsnmp_container *c = SNMP_MALLOC_TYPEDEF(netsnmp_container);
	if (NULL == c) {
		snmp_log(LOG_ERR, "couldn't allocate memory\n");
		return NULL;
	}   
	
	c->container_data = new Unit();
	
	c->get_size = delegate<size_t, &Unit::size>;
	c->init = NULL;
	c->cfree = cfree;
	c->insert = delegate<int, const void*, &Unit::insert>;
	c->remove = delegate<int, const void*, &Unit::remove>;
	c->find = delegate<void*, const void*, &Unit::find>;
	c->find_next = delegate<void*, const void*, &Unit::findNext>;
	c->get_subset = delegate<netsnmp_void_array*, void*, &Unit::getSubset>;
	c->get_iterator = NULL;
	c->for_each = NULL;
	c->clear = delegate<void, netsnmp_container_obj_func*, void*, &Unit::clear>;
	
	return c;
}

netsnmp_factory* getFactory()
{
	static netsnmp_factory f = { "threadsafe_array",
								 (netsnmp_factory_produce_f*)
								 make };
	return &f;
}

} // anonymous namespace

int Unit::insert(const void* data_)
{
	Lock g(m_lock);
	m_data.insert(data_);
	return 0;
}

void* Unit::find(const void* key_)
{
	Lock g(m_lock);

	iterator_type i = m_data.find(key_);
	if (i == m_data.end())
		return NULL;
	return const_cast<void *>(*i);
}

void* Unit::findNext(const void* key_)
{
	Lock g(m_lock);

	if (key_ == NULL) {
		if (m_data.empty())
			return NULL;
		else
			return const_cast<void *>(*m_data.begin());
	}

	iterator_type i = m_data.upper_bound(key_);
	if (i == m_data.end())
		return NULL;

	return const_cast<void *>(*i);
}

size_t Unit::size()
{
	Lock g(m_lock);
	return m_data.size(); 
}

int Unit::remove(const void* data_)
{
	Lock g(m_lock);

	if (m_data.empty())
		return 0;

	iterator_type i(m_data.find(data_));
	if (i == m_data.end())
		return -1;

	m_data.erase(i);
	return 0;
}

void Unit::clear(netsnmp_container_obj_func* f_, void* context_)
{
	Lock g(m_lock);
	m_data.clear();
}

netsnmp_void_array* Unit::getSubset(void* data_)
{
	Lock g(m_lock);

	range_type r = std::equal_range(m_data.begin(), m_data.end(), data_, data_type::key_compare());
	if (r.first == r.second)
		return NULL;

	void **rtn = static_cast<void **>(::malloc(std::distance(r.first, r.second) * sizeof(void*)));
	if (rtn == NULL)
		return NULL;
	
	iterator_type i(r.first);
	size_t z = 0;
	for (; i != r.second; ++i)
		rtn[z++] = const_cast<void *>(*i);

	netsnmp_void_array* va = SNMP_MALLOC_TYPEDEF(netsnmp_void_array);
	if (va == NULL)
	{
		::free(rtn);
		return NULL;
	}

	va->size = z;
	va->array = rtn;
	
	return va;
}

void inject()
{
	int ret = 
	netsnmp_container_register_with_compare("threadsafe_array",
			getFactory(),
			netsnmp_compare_netsnmp_index);
	snmp_log(LOG_ERR, LOG_PREFIX"register with compare: %d\n", ret);
}

} // namespace ThreadsafeContainer
} // namespace Rmond

