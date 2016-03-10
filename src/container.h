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

#ifndef THREADSAFE_CONTAINER_H
#define THREADSAFE_CONTAINER_H

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <set>
#include <iterator>

namespace Rmond
{
namespace ThreadsafeContainer
{

///////////////////////////////////////////////////////////////////////////////
// struct Unit

struct Unit
{
	int insert(const void* data_);
	void* find(const void* key_);
	void* findNext(const void* key_);
	size_t size();
	int remove(const void* data_);
	void clear(netsnmp_container_obj_func* f_, void* context_);
	netsnmp_void_array* getSubset(void* data_);

private:
	typedef const void *value_type;
	struct Less
	{
		bool operator()(const value_type& rhs_, const value_type& lhs_) const
		{
			return (netsnmp_compare_netsnmp_index(rhs_, lhs_) < 0);
		}
	};
	typedef std::set<value_type, Less> data_type;
	typedef data_type::iterator iterator_type;
	typedef std::pair<iterator_type, iterator_type> range_type;

	data_type m_data;
	pthread_mutex_t m_lock;
};

void inject();

} // namespace ThreadsafeContainer
} // namespace Rmond

#endif // THREADSAFE_CONTAINER_H
