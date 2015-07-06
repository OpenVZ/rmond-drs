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

