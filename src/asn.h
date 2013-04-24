#ifndef ASN_H
#define ASN_H
#include "mib.h"
#include <string>

namespace Rmond
{
namespace Asn
{
namespace Policy
{
///////////////////////////////////////////////////////////////////////////////
// struct Integer

template<int T>
struct Integer
{
	static void get(int src_, netsnmp_variable_list& dst_)
	{
		snmp_set_var_typed_value(&dst_, T,
			(u_char* )&src_, sizeof(int));
	}
	static void put(const netsnmp_variable_list& src_, int& dst_)
	{
		dst_ = *src_.val.integer;
	}
};

///////////////////////////////////////////////////////////////////////////////
// struct IP

struct IP
{
	static void get(in_addr_t src_, netsnmp_variable_list& dst_);
	static void put(const netsnmp_variable_list& src_, in_addr_t& dst_);
};

///////////////////////////////////////////////////////////////////////////////
// struct Counter

struct Counter
{
	static void get(unsigned long long src_, netsnmp_variable_list& dst_);
	static void put(const netsnmp_variable_list& src_, unsigned long long& dst_);
};

///////////////////////////////////////////////////////////////////////////////
// struct String

struct String
{
	static void get(const std::string& src_, netsnmp_variable_list& dst_);
	static void put(const netsnmp_variable_list& src_, std::string& dst_);
};

///////////////////////////////////////////////////////////////////////////////
// struct ObjectId

struct ObjectId
{
	typedef Oid_type value_type;

	static void get(const value_type& src_, netsnmp_variable_list& dst_);
	static void put(const netsnmp_variable_list& src_, value_type& dst_);
};

} // namespace Policy

///////////////////////////////////////////////////////////////////////////////
// struct Bean

template<class T, class P>
struct Bean
{
	typedef T value_type;

	Bean(): m_value()
	{
	}

	const value_type& get() const
	{
		return m_value;
	}
	void put(const value_type& value_)
	{
		m_value = value_;
	}
	void get(netsnmp_variable_list& dst_) const
	{
		P::get(m_value, dst_);
	}
	void put(const netsnmp_variable_list& src_)
	{
		P::put(src_, m_value);
	}
private:
	value_type m_value;
};

///////////////////////////////////////////////////////////////////////////////
// struct ValueFactory

template<int T>
struct ValueFactory: Bean<int, Policy::Integer<T> >
{
};

template<>
struct ValueFactory<ASN_IPADDRESS>: Bean<in_addr_t, Policy::IP>
{
};

template<>
struct ValueFactory<ASN_COUNTER64>: Bean<unsigned long long, Policy::Counter>
{
};

template<>
struct ValueFactory<ASN_OCTET_STR>: Bean<std::string, Policy::String>
{
};

template<>
struct ValueFactory<ASN_OBJECT_ID>: Bean<Oid_type, Policy::ObjectId>
{
};
} // namespace Asn
} // namespace Rmond

#endif // ASN_H

