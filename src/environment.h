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
	void addUsage(Value::Storage* value_)
	{
		m_usageList.push_back(value_);
	}
private:
	typedef boost::ptr_list<Value::Storage> valueList_type;
	typedef boost::ptr_list<Value::Composite::Base> providerList_type;

	PRL_HANDLE m_h;
	valueList_type m_stateList;
	valueList_type m_usageList;
	providerList_type m_providerList;
};

} // namespace Rmond

#endif // ENVIRONMENT_H

