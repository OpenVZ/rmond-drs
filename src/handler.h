#ifndef HANDLER_H
#define HANDLER_H

#include "table.h"
#include <boost/type_traits.hpp>
#include "net-snmp/library/snmp-tc.h"

namespace Rmond
{
namespace Table
{
namespace Request
{
///////////////////////////////////////////////////////////////////////////////
// struct Details

struct Details
{
	typedef netsnmp_request_info request_type;
	typedef netsnmp_table_request_info cell_type;

	explicit Details(request_type* request_);

        template<typename T>
        T* pop(const char* name_)
        {
                T* output = find<T>(name_);
                if (NULL != output)
                        erase(name_);

                return output;
        }
        template<typename T>
        T* find(const char* name_) const
        {
                return (T* )netsnmp_request_get_list_data(m_request, name_);
        }
	void cannot(int code_);
	cell_type* cell() const;
	void erase(const char* name_);
	void push(const char* name_, void* data_);
private:
	request_type* m_request;
};

///////////////////////////////////////////////////////////////////////////////
// struct Unit

template<class T>
struct Unit: private Details
{
	typedef Table::Unit<T> table_type;
	typedef typename table_type::tupleSP_type row_type;
	typedef typename table_type::schema_type schema_type;

	Unit(request_type* request_, table_type& table_):
		Details(request_), m_request(request_), m_table(&table_)
	{
	}

	void get();
	void put();
	void commit();
	void reserve();
	void rollback();
	row_type inserted() const;
private:
	row_type row() const;
	void cannotInsert();
	void cannotExtract();
	netsnmp_variable_list* getBackup()
	{
		return pop<netsnmp_variable_list>(TOKEN_PREFIX"backup");
	}

	request_type* m_request;
	table_type* m_table;
};

template<class T>
typename Unit<T>::row_type Unit<T>::row() const
{
	row_type r = m_table->extract(m_request);
	if (NULL != r.get())
		return r;

	cell_type* c = cell();
	if (NULL == c)
		return row_type();

	netsnmp_index p;
	p.len = c->index_oid_len;
	p.oids = c->index_oid;
	return m_table->find(p);
}

template<class T>
typename Unit<T>::row_type Unit<T>::inserted() const
{
	row_type r = row();
	if (NULL == r.get())
		return row_type();

	if (RS_NOTINSERVICE == r->template get<schema_type::ROW_STATUS>())
		return r;

	return row_type();
}

template<class T>
void Unit<T>::get()
{
	row_type r = row();
	cell_type* c = cell();
	if (NULL == r.get() || c == NULL)
		return cannotExtract();

	if (r->get(c->colnum, *m_request->requestvb))
		cannot(SNMP_NOSUCHOBJECT);
}

template<class T>
void Unit<T>::put()
{
	row_type r = row();
	cell_type* c = cell();
	if (NULL == r.get() || c == NULL)
		return cannotExtract();

	if (schema_type::ROW_STATUS == c->colnum)
		return;

	if (r->put(c->colnum, *m_request->requestvb))
		cannot(SNMP_NOSUCHOBJECT);
}

template<class T>
void Unit<T>::reserve()
{
	cell_type* c = cell();
	if (NULL == c)
		return cannotExtract();

	row_type r = row();
	if (schema_type::ROW_STATUS == c->colnum)
	{
		int v = 0;
		Asn::Policy::Integer<ASN_INTEGER>::put(*m_request->requestvb, v);
		switch (v)
		{
		case RS_CREATEANDGO:
		case RS_CREATEANDWAIT:
			if (NULL != r.get())
				return cannot(SNMP_ERR_INCONSISTENTVALUE);

			r.reset(new typename table_type::tuple_type(c));
			if (NULL == r.get())
				return cannotInsert();

			r->template put<schema_type::ROW_STATUS>(RS_NOTINSERVICE);
			if (m_table->insert(r))
				return cannotInsert();
		case RS_DESTROY:
			return;
		default:
			return cannot(SNMP_ERR_WRONGVALUE);
		}
	}
	else if (NULL != r.get())
	{
		netsnmp_variable_list* v = SNMP_MALLOC_TYPEDEF(netsnmp_variable_list);
		if (NULL == v)
			return;

		if (r->get(c->colnum, *v))
		{
			::free(v);
			return cannot(SNMP_NOSUCHOBJECT);
		}
		push(TOKEN_PREFIX"backup", v);
	}
}

template<class T>
void Unit<T>::commit()
{
	cell_type* c = cell();
	if (NULL == c || c->colnum != schema_type::ROW_STATUS)
		return;

	int v = 0;
	row_type r = row();
	Asn::Policy::Integer<ASN_INTEGER>::put(*m_request->requestvb, v);
	switch (v)
	{
	case RS_CREATEANDGO:
		r->template put<schema_type::ROW_STATUS>(RS_ACTIVE);
		break;
	case RS_CREATEANDWAIT:
		r->template put<schema_type::ROW_STATUS>(RS_NOTREADY);
		break;
	case RS_DESTROY:
		if (NULL != r.get())
			m_table->erase(*r);
	}
}

template<class T>
void Unit<T>::rollback()
{
	row_type r = row();
	if (NULL != r.get())
	{
		netsnmp_variable_list* b = getBackup();
		if (NULL != b)
		{
			r->put(cell()->colnum, *b);
			::free(b);
		}
	}
	row_type i = inserted();
	if (NULL != i.get())
		m_table->erase(*i);
}

template<class T>
void Unit<T>::cannotInsert()
{
	snmp_log(LOG_ERR, LOG_PREFIX"could not insert an entry to the %s\n",
		schema_type::name());
	snmp_set_var_typed_value(m_request->requestvb, SNMP_ERR_GENERR,
				 NULL, 0);
}

template<class T>
void Unit<T>::cannotExtract()
{
	snmp_log(LOG_ERR, LOG_PREFIX"could not extract a table entry or info for %s\n",
		schema_type::name());
	snmp_set_var_typed_value(m_request->requestvb, SNMP_ERR_GENERR,
				 NULL, 0);
}

} // namespace Request

namespace Handler
{
///////////////////////////////////////////////////////////////////////////////
// struct Base

template<class T, class P>
struct Base
{
	typedef boost::shared_ptr<Table::Unit<T> > tableSP_type;

	explicit Base(tableSP_type table_): m_table(table_)
	{
	}

	void operator()(int mode_, netsnmp_request_info* request_)
	{
		tableSP_type x = m_table.lock();
		if (NULL == x.get())
			return;

		static_cast<P* >(this)->do_(mode_, Request::Unit<T>(request_, *x));
	}
private:
	boost::weak_ptr<Table::Unit<T> > m_table;
};

///////////////////////////////////////////////////////////////////////////////
// struct Trivial

template<class T>
struct Trivial
{
	void commit(Request::Unit<T> event_)
	{
		event_.commit();
	}
	void reserve(Request::Unit<T> )
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
// struct ReadOnly

template<class T>
struct ReadOnly: Details::Automat<ReadOnly<T>, Request::Unit<T> >, Base<T, ReadOnly<T> >
{
	typedef Base<T, ReadOnly<T> > base_type;
	typedef typename base_type::tableSP_type tableSP_type;
	typedef Details::Automat<ReadOnly<T>, Request::Unit<T> > automat_type;

	explicit ReadOnly(tableSP_type table_): base_type(table_)
	{
	}

	void get(Request::Unit<T> event_)
	{
		event_.get();
	}
	typedef typename mpl::vector<
			typename automat_type::template Row<MODE_GET, &ReadOnly::get>
		>::type table_type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Mutable

template<class T, class I = Trivial<T> >
struct Mutable: Details::Automat<Mutable<T, I>, Request::Unit<T> >, Base<T, Mutable<T, I> >
{
	typedef Base<T, Mutable<T, I> > base_type;
	typedef typename base_type::tableSP_type tableSP_type;
	typedef Details::Automat<Mutable<T, I>, Request::Unit<T> > automat_type;

	template<class U>
	explicit Mutable(U table_,
			typename boost::enable_if_c<boost::has_trivial_constructor<I>::value &&
						boost::is_same<tableSP_type, U>::value>::type* = 0):
		base_type(table_)
	{
	}

	Mutable(I impl_, tableSP_type table_): base_type(table_), m_impl(impl_)
	{
	}

	void get(Request::Unit<T> event_)
	{
		event_.get();
	}
	void setFree(Request::Unit<T> event_)
	{
		event_.rollback();
	}
	void setUndo(Request::Unit<T> event_)
	{
		event_.rollback();
	}
	void setAction(Request::Unit<T> event_)
	{
		event_.put();
	}
	void setReserve1(Request::Unit<T> event_)
	{
		event_.reserve();
	}
	void setReserve2(Request::Unit<T> event_)
	{
		m_impl.reserve(event_);
	}
	void setCommit(Request::Unit<T> event_)
	{
		m_impl.commit(event_);
	}
	typedef typename mpl::vector<
			typename automat_type::template Row<MODE_GET, &Mutable::get>,
			typename automat_type::template Row<MODE_SET_ACTION, &Mutable::setAction>,
			typename automat_type::template Row<MODE_SET_RESERVE1, &Mutable::setReserve1>,
			typename automat_type::template Row<MODE_SET_RESERVE2, &Mutable::setReserve2>,
			typename automat_type::template Row<MODE_SET_FREE, &Mutable::setFree>,
			typename automat_type::template Row<MODE_SET_UNDO, &Mutable::setUndo>,
			typename automat_type::template Row<MODE_SET_COMMIT, &Mutable::setCommit>
		>::type table_type;
private:
	I m_impl;
};

} // namespace Handler
} // namespace Table
} // namespace Rmond

#endif // HANDLER_H

