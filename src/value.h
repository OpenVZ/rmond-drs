#ifndef VALUE_H
#define VALUE_H
#include <map>
#include <set>
#include <list>
#include "table.h"
#include <memory>
#include <string>
#include <boost/ptr_container/ptr_list.hpp>

namespace Rmond
{
namespace Value
{
typedef std::set<Oid_type> Metrix_type;

///////////////////////////////////////////////////////////////////////////////
// struct Provider

struct Provider
{
	virtual ~Provider();
	virtual netsnmp_variable_list* make() const = 0;
};

///////////////////////////////////////////////////////////////////////////////
// struct Trap

struct Trap: Provider
{
	netsnmp_variable_list* make() const;
	static netsnmp_pdu* pdu(netsnmp_variable_list* );
};

///////////////////////////////////////////////////////////////////////////////
// struct Trivial

struct Trivial: Provider
{
	explicit Trivial(netsnmp_variable_list* value_);
	~Trivial();

	netsnmp_variable_list* make() const;
private:
	netsnmp_variable_list* m_value;
};

///////////////////////////////////////////////////////////////////////////////
// struct Named

struct Named: Provider
{
	typedef Oid_type name_type;

	Named(const name_type& name_, Provider* value_);
	Named(const name_type& name_, netsnmp_variable_list* value_);
	Named(const oid* name_, size_t length_, Provider* value_);

	netsnmp_variable_list* make() const;
private:
	name_type m_name;
	std::auto_ptr<Provider> m_value;
};

namespace Cell
{
///////////////////////////////////////////////////////////////////////////////
// struct Make

template<class T, class D>
struct Make;

template<class T>
struct Make<T, typename Table::Unit<typename T::value_type>::tuple_type>
{
	typedef typename Table::Unit<typename T::value_type>::tuple_type data_type;

	static void do_(const data_type& src_, netsnmp_variable_list& dst_)
	{
		src_.get(T::value, dst_);
	}
};

template<class T>
struct Make<T, Table::Tuple::Data<typename T::value_type> >
{
	typedef Table::Tuple::Data<typename T::value_type> data_type;

	static void do_(const data_type& src_, netsnmp_variable_list& dst_)
	{
		src_.template get<typename T::value_type, T::value>(dst_);
	}
};

///////////////////////////////////////////////////////////////////////////////
// struct Value

template<class T, class D>
struct Value: Provider
{
	typedef boost::weak_ptr<D> dataWP_type;
	typedef boost::shared_ptr<D> dataSP_type;

	explicit Value(dataSP_type data_): m_data(data_)
	{
	}

	netsnmp_variable_list* make() const
	{
		dataSP_type d = m_data.lock();
		if (NULL == d.get())
			return NULL;

		netsnmp_variable_list* output = Provider::make();
		if (NULL != output)
			Make<T, D>::do_(*d, *output);

		return output;
	}
private:
	dataWP_type m_data;
};

///////////////////////////////////////////////////////////////////////////////
// struct Unit

template<class T, T N>
struct Unit: Provider
{
	typedef Value<mpl::integral_c<T, N>, typename Table::Unit<T>::tuple_type>
			value_type;

	explicit Unit(typename value_type::dataSP_type data_): m_data(data_)
	{
	}

	Oid_type name() const
	{
		// NB. format - table.1.column.index
		Oid_type output = prefix();
		typename value_type::dataSP_type d = m_data.lock();
		if (NULL != d.get())
		{
			const netsnmp_index& k = d->key();
			output.insert(output.end(), k.oids, k.oids + k.len);
		}
		return output;
	}
	netsnmp_variable_list* make() const
	{
		typename value_type::dataSP_type x = m_data.lock();
		if (NULL == x.get())
			return NULL;

		return Named(name(), new value_type(x)).make();
	}
	static Oid_type prefix()
	{
		Oid_type output = Schema<T>::uuid();
		output.push_back(1);
		output.push_back(N);
		return output;
	}
private:
	typename value_type::dataWP_type m_data;
};

} // namespace Cell

///////////////////////////////////////////////////////////////////////////////
// struct List

struct List: Provider, private boost::ptr_list<Provider>
{
	netsnmp_variable_list* make() const;
	using boost::ptr_list<Provider>::push_back;
};

namespace Details
{
///////////////////////////////////////////////////////////////////////////////
// struct Tuple

template<class T>
struct Tuple
{
	typedef typename Table::Unit<T>::tupleSP_type tupleSP_type;
	typedef std::list<tupleSP_type> data_type;
	
	template<class U>
	struct Policy
	{
		typedef Cell::Unit<typename U::value_type, U::value> cell_type;

		static Oid_type uuid()
		{
			return cell_type::prefix();
		}
		static void copy(const data_type& data_, List& dst_)
		{
			typename data_type::const_iterator e = data_.end();
			typename data_type::const_iterator p = data_.begin();
			for (; p != e; ++p)
			{
				dst_.push_back(new cell_type(*p));
			}
		}
	};
};

///////////////////////////////////////////////////////////////////////////////
// struct Scalar

template<class T>
struct Scalar
{
	typedef Table::Tuple::Data<T> tuple_type;
	typedef boost::shared_ptr<tuple_type> data_type;
	
	template<class U>
	struct Policy
	{
		static Oid_type uuid()
		{
			return Schema<void>::uuid(U::value);
		}
		static void copy(const data_type& data_, List& dst_)
		{
			Oid_type u = uuid();
			u.push_back(0);
			dst_.push_back(new Value::Named(u,
				new Value::Cell::Value<U, tuple_type>(data_)));
		}
	};
};

///////////////////////////////////////////////////////////////////////////////
// struct Visitor

template<class T>
struct Visitor
{
	Visitor(typename T::data_type data_, const Metrix_type& metrix_, List& result_):
		m_result(&result_), m_metrix(&metrix_), m_data(data_)
	{
	}

	template<class U>
	void operator()(U )
	{
		typedef typename T::template Policy<U> policy_type;
		if (m_metrix->empty() || m_metrix->count(policy_type::uuid()) > 0)
			policy_type::copy(m_data, *m_result);
	}
private:
	List* m_result;
	const Metrix_type* m_metrix;
	typename T::data_type m_data;
};

} // namespace Details

///////////////////////////////////////////////////////////////////////////////
// struct Storage

struct Storage
{
	virtual ~Storage();
	virtual void refresh(PRL_HANDLE h_) = 0;
};

namespace Composite
{
///////////////////////////////////////////////////////////////////////////////
// struct Base

struct Base
{
	virtual ~Base();
	virtual Provider* snapshot(const Metrix_type& metrix_) const = 0;
};

///////////////////////////////////////////////////////////////////////////////
// class Tuple

template<class T, class U, class V>
struct Tuple: Base
{
	Provider* snapshot(const Metrix_type& metrix_) const
	{
		List* output = new List;
		const U& u = static_cast<const U& >(*this);
		mpl::for_each<typename Rmond::Details::Names<T>::type>(
			Details::Visitor<V>(u.data(), metrix_, *output));
		return output;
	}
};

///////////////////////////////////////////////////////////////////////////////
// class Range

template<class T>
struct Range: Tuple<T, Range<T>, Details::Tuple<T> >
{
	typedef Table::Unit<T> table_type;
	typedef boost::weak_ptr<table_type> tableWP_type;
	typedef typename Details::Tuple<T>::data_type data_type;

	Range(const Oid_type& key_, tableWP_type table_):
		m_key(key_), m_table(table_)
	{
	}

	data_type data() const
	{
		boost::shared_ptr<table_type> t = m_table.lock();
		if (NULL == t.get())
			return data_type();

		return t->range(m_key);
	}
private:
	Oid_type m_key;
	tableWP_type m_table;
};

///////////////////////////////////////////////////////////////////////////////
// class Scalar

template<class T>
struct Scalar: Tuple<T, Scalar<T>, Details::Scalar<T> >
{
	typedef typename Details::Scalar<T>::data_type data_type;

	explicit Scalar(data_type data_): m_data(data_)
	{
	}

	data_type data() const
	{
		return m_data;
	}
private:
	data_type m_data;
};

} // namespace Composite
} // namespace Value
} // namespace Rmond

#endif // VALUE_H

