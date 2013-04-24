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
	BOOST_FOREACH(valueList_type::reference r, m_usageList)
	{
		r.refresh(performance_);
	}
}

Value::Provider* Environment::snapshot(const Value::Metrix_type& metrix_) const
{
	Value::List* output = new Value::List;
	BOOST_FOREACH(providerList_type::const_reference r, m_providerList)
	{
		Value::Provider* y = r.snapshot(metrix_);
		if (NULL != y)
			output->push_back(y);
	}
	return output;
}

} // namespace Rmond

