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

#include "details.h"
#include "handler.h"

namespace Rmond
{
///////////////////////////////////////////////////////////////////////////////
// struct Schema<void>

Oid_type Schema<void>::uuid(Oid_type::value_type luid_)
{
	Oid_type output = Central::product();
	output.push_back(luid_);
	return output;
}

namespace Table
{
namespace Request
{
///////////////////////////////////////////////////////////////////////////////
// struct Details

Details::Details(request_type* request_): m_request(request_)
{
}

void Details::push(const char* name_, void* data_)
{
	netsnmp_request_add_list_data(m_request,
	      netsnmp_create_data_list(name_, data_, NULL));
}

void Details::erase(const char* name_)
{
	netsnmp_request_remove_list_data(m_request, name_);
}

Details::cell_type* Details::cell() const
{
	return netsnmp_extract_table_info(m_request);
}

void Details::cannot(int code_)
{
	netsnmp_request_set_error(m_request, code_);
}

} // namespace Request
} // namespace Table
} // namespace Rmond

