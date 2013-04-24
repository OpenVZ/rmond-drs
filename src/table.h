#ifndef TABLE_H
#define TABLE_H

#include <list>
#include "details.h"

namespace Rmond
{
namespace Table
{
namespace Tuple
{
///////////////////////////////////////////////////////////////////////////////
// struct Key

template<class T>
class Key: public Details::Set<T, typename Details::Key<T>::type> 
{
	struct Extract
	{
		Extract(const Key& index_, netsnmp_variable_list*& data_):
			m_index(&index_), m_data(&data_)
		{
			*m_data = NULL;
		}

		template<class U>
		void operator()(U )
		{

			netsnmp_variable_list* y = SNMP_MALLOC_TYPEDEF(netsnmp_variable_list);
			netsnmp_variable_list* x = *m_data;
			if (NULL == x)
				*m_data = y;
			else
			{
				for (; NULL != x->next_variable; x = x->next_variable)
				{}
				x->next_variable = y;
			}
			if (NULL != y)
				m_index->template get<typename U::value_type, U::value>(*y);
		}
	private:
		const Key* m_index;
		netsnmp_variable_list** m_data;
	};
public:
	void extract(netsnmp_index& dst_) const
	{
		memset(&dst_, 0, sizeof(netsnmp_index));
		netsnmp_variable_list* x = NULL;
		mpl::for_each<typename Details::Key<T>::seq_type>(Extract(*this, x));
		build_oid(&dst_.oids, &dst_.len, NULL, 0, x);
		snmp_free_varbind(x);		
	}
};

///////////////////////////////////////////////////////////////////////////////
// struct Data

template<class T>
struct Data: Details::Set<T, typename Details::Tuple<T>::type>
{
};

///////////////////////////////////////////////////////////////////////////////
// struct Access

template<class T, class S, class F>
class Access: public Details::Automat<Access<T, S, F>,
		typename boost::add_reference<typename mpl::apply1<F, Data<T> >::type>::type>
{
	typedef typename mpl::apply1<F, Data<T> >::type data_type; 
	typedef typename boost::add_reference<data_type>::type reference_type;
	typedef Details::Automat<Access<T, S, F>, reference_type> base_type;

	template<class U, U N>
	struct Fire
	{
		static void do_(reference_type event_, netsnmp_variable_list* value_, mpl::true_)
		{
			event_.template get<U, N>(*value_);
		}
		static void do_(reference_type event_, netsnmp_variable_list* value_, mpl::false_)
		{
			event_.template put<N>(*value_);
		}
	};
	template<class U>
	struct Each
	{
		typedef typename base_type::template Row<U::value,
						&Access::template process<typename U::value_type, U::value> > type;
	};
public:
	Access(netsnmp_variable_list& value_): m_result(false), m_value(&value_)
	{
	}

	template<class U, U N>
	void process(reference_type event_)
	{
		Fire<U, N>::do_(event_, m_value, typename boost::is_const<data_type>::type());
	}
	void unknown(int case_, reference_type event_)
	{
		m_result = true;
	}
	bool result() const
	{
		return m_result;
	}

	typedef typename mpl::transform<S, Each<mpl::_1> >::type table_type;
private:
	bool m_result;
	netsnmp_variable_list* m_value;
};

///////////////////////////////////////////////////////////////////////////////
// class Unit

template<class T>
class Unit: private netsnmp_index
{
	typedef typename Details::Mutable<T>::type mutable_type;
	typedef typename Details::Tuple<T>::schema_type schema_type;
	typedef Data<T> data_type;

	struct Assign
	{
		Assign(const Key<T>& index_, Unit& tuple_):
			m_data(&tuple_.m_data), m_index(&index_)
		{
		}
		template<class U>
		void operator()(U )
		{
			m_data->put<U::value>(m_index->get<U::value>());
		}
	private:
		data_type* m_data;
		const Key<T>* m_index;
	};
	template<class U>
	struct Filter
	{
		typedef typename boost::is_same<T, typename U::value_type>::type type;
	};
public:
	explicit Unit(const Key<T>& index_)
	{
		index_.extract(*this);
		typedef typename mpl::copy_if<typename Details::Key<T>::seq_type,
			mpl::quote1<Filter> >::type seq_type;
		mpl::for_each<seq_type>(Assign(index_, *this));
	}
	explicit Unit(netsnmp_table_request_info* request_)
	{
		if (NULL == request_)
		{
			len = 0;
			oids = NULL;
			return;
		}
		len = request_->index_oid_len;
		oids = snmp_duplicate_objid(request_->index_oid, len);
		netsnmp_variable_list* x = request_->indexes;
		mpl::for_each<typename Details::Key<T>::seq_type>(Details::Index::Patch<T>(x));
		for (; x != NULL; x = x->next_variable)
		{
			Access<T, typename Details::Names<T>::type,
					mpl::identity<mpl::_1> > u(*x);
			u.do_(static_cast<T>(x->index), m_data);
		}
	}
	~Unit()
	{
		free(oids);
	}

	template<T N>
	typename Details::Column<T, N>::type::value_type get() const
	{
		return m_data.get<N>();
	}
	bool get(int name_, netsnmp_variable_list& dst_) const
	{
		Access<T, typename Details::Names<T>::type, boost::add_const<mpl::_1> > u(dst_);
		u.do_(name_, m_data);
		return u.result();
	}

	template<T N>
	typename boost::enable_if<
			mpl::contains<mutable_type, typename Details::Column<T, N>::type::declaration_type>
		>::type put(const typename Details::Column<T, N>::type::value_type& value_)
	{
		m_data.template put<N>(value_);
	}

	bool put(int name_, netsnmp_variable_list value_)
	{
		Access<T, typename Details::Names<T>::type,
			mpl::identity<mpl::_1> > u(value_);
		u.do_(name_, m_data);
		return u.result();
	}

	const netsnmp_index& key() const
	{
		return *this;
	}
private:
	Unit(const Unit& );
	Unit& operator=(const Unit& );

	data_type m_data;
};

} // namespace Tuple

///////////////////////////////////////////////////////////////////////////////
// struct Unit

template<class T>
struct Unit
{
	typedef Tuple::Key<T> key_type;
	typedef Tuple::Unit<T> tuple_type;
	typedef boost::shared_ptr<tuple_type> tupleSP_type;
	typedef typename Details::Tuple<T>::schema_type schema_type;

	Unit();
	~Unit();

	template<class H>
	bool attach(H* handler_);
	tupleSP_type find(const key_type& key_) const;
	tupleSP_type find(const netsnmp_index& key_) const;
	tupleSP_type extract(netsnmp_request_info* request_) const;
	bool erase(const tuple_type& tuple_);
	bool insert(tupleSP_type tuple_);
	std::list<tupleSP_type> range(Oid_type key_) const;
	std::list<tupleSP_type> range(netsnmp_index key_) const;
private:
	typedef std::pair<netsnmp_index, tupleSP_type> row_type;

	template<class H>
	static int handle(netsnmp_mib_handler* , netsnmp_handler_registration* ,
		netsnmp_agent_request_info* , netsnmp_request_info* );

	netsnmp_container* m_storage;
	netsnmp_handler_registration* m_registration;
};

template<class T>
Unit<T>::Unit(): m_storage(NULL), m_registration(NULL)
{
	std::string n = std::string(TOKEN_PREFIX)
				.append(schema_type::name())
				.append(":").append("table_container");
	m_storage = netsnmp_container_find(n.c_str());
	if (NULL == m_storage)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"cannot find container %s\n", n.c_str());
	}
}

template<class T>
template<class H>
bool Unit<T>::attach(H* handler_)
{
	DEBUGMSGTL((TOKEN_PREFIX"init", "initializing table %s\n", schema_type::name()));
	if (NULL == m_storage || m_registration != NULL)
		return true;

	netsnmp_table_registration_info* t = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
	if (NULL == t)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"error allocating table registration\n");
		return true;
	}
	netsnmp_handler_registration* r = schema_type::handler(&Unit<T>::handle<H>, handler_);
	if (NULL == r)
	{
		snmp_log(LOG_ERR, LOG_PREFIX"error creating handler registration for %s\n", schema_type::name());
	}
	else
	{
		mpl::for_each<typename Details::Key<T>::seq_type>(Details::Index::Inject<T>(t));
		netsnmp_mib_handler* z = netsnmp_container_table_handler_get(t, m_storage,
						  TABLE_CONTAINER_KEY_NETSNMP_INDEX);
		if (NULL == z)
		{
			snmp_log(LOG_ERR, LOG_PREFIX"error allocating table registration for %s\n", schema_type::name());
		}
		else if (SNMPERR_SUCCESS != netsnmp_inject_handler(r, z))
		{
			netsnmp_handler_free(z);
			snmp_log(LOG_ERR, LOG_PREFIX"error injecting container_table handler for %s\n", schema_type::name());
		}
		else
		{
			z = NULL;
			if (SNMPERR_SUCCESS == netsnmp_register_table(r, t))
			{
				m_registration = r;
				DEBUGMSGTL((TOKEN_PREFIX"init", "table %s initialized successfully\n", schema_type::name()));
				return false;
			}
			snmp_log(LOG_ERR, LOG_PREFIX"error registering table handler for %s\n", schema_type::name());
		}
		netsnmp_handler_registration_free(r);
	}
	SNMP_FREE(t);
	return true;
}

template<class T>
Unit<T>::~Unit()
{
	DEBUGMSGTL((TOKEN_PREFIX"fini", "finalizing table %s\n", schema_type::name()));
	if (NULL != m_registration)
		netsnmp_unregister_handler(m_registration);

	if (NULL != m_storage)
	{
		CONTAINER_FREE(m_storage);
	}
}

template<class T>
typename Unit<T>::tupleSP_type Unit<T>::extract(netsnmp_request_info* request_) const
{
	row_type* r = (row_type* )netsnmp_container_table_extract_context(request_);
	return NULL == r ? tupleSP_type() : r->second;
}

template<class T>
typename Unit<T>::tupleSP_type Unit<T>::find(const netsnmp_index& key_) const
{
	void* r = CONTAINER_FIND(m_storage, &key_);
	if (NULL == r)
		return tupleSP_type();

	return ((row_type* )r)->second;
}

template<class T>
typename Unit<T>::tupleSP_type Unit<T>::find(const key_type& key_) const
{
	netsnmp_index k;
	key_.extract(k);
	tupleSP_type output = find(k);
	free(k.oids);
	return output;
}

template<class T>
bool Unit<T>::insert(tupleSP_type tuple_)
{
	row_type* r = new row_type(tuple_->key(), tuple_);
	int e = CONTAINER_INSERT(m_storage, r);
	if (0 != e)
	{
		delete r;
		return true;
	}
	return false;
}

template<class T>
bool Unit<T>::erase(const tuple_type& tuple_)
{
	netsnmp_index x = tuple_.key();
	row_type* r = (row_type* )CONTAINER_FIND(m_storage, &x);
	if (NULL == r)
		return true;
	
	CONTAINER_REMOVE(m_storage, &x);
	delete r;
	return false;
}

template<class T>
std::list<typename Unit<T>::tupleSP_type> Unit<T>::range(netsnmp_index key_) const
{
	netsnmp_void_array* a = CONTAINER_GET_SUBSET(m_storage, &key_);
	std::list<tupleSP_type> output;
	if (NULL != a)
	{
		for (size_t i = 0; i < a->size; ++i)
		{
			row_type* r = (row_type* )a->array[i];
			output.push_back(r->second);
		}
		free(a->array);
		free(a);
	}
	return output;
}

template<class T>
std::list<typename Unit<T>::tupleSP_type> Unit<T>::range(Oid_type key_) const
{
	netsnmp_index k = {};
	if (!key_.empty())
	{
		k.len = key_.size();
		k.oids = &key_[0];
	}
	return range(k);
}

template<class T>
template<class H>
int Unit<T>::handle(netsnmp_mib_handler* handler_, netsnmp_handler_registration* ,
	netsnmp_agent_request_info* info_, netsnmp_request_info* requests_)
{
	DEBUGMSGTL((TOKEN_PREFIX"handle", "Processing request (%d)\n", info_->mode));
	for (; NULL != requests_; requests_ = requests_->next)
	{
		if (0 != requests_->processed)
			continue;

		if (0 != requests_->status)
		{
			// already got an error.
			break;
		}
		H* h = (H*)handler_->myvoid;
		(*h)(info_->mode, requests_);
	}
	return SNMP_ERR_NOERROR;
}

} // namespace Table
} // namespace Rmond

#endif // TABLE_H

