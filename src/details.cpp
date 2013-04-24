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

