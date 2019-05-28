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

#ifndef DETAILS_H
#define DETAILS_H
#include "asn.h"
#include <boost/mpl/find.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/inherit.hpp>
#include <boost/mpl/find_if.hpp>
#include <boost/type_traits.hpp>
#include <boost/mpl/copy_if.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/max_element.hpp>
#include <boost/mpl/transform_view.hpp>
#include <boost/mpl/inherit_linearly.hpp>

namespace Rmond
{
namespace mpl = boost::mpl;

///////////////////////////////////////////////////////////////////////////////
// struct Declaration

template<class T, T N, int D>
struct Declaration
{
	typedef mpl::int_<D> asn_type;
	typedef mpl::integral_c<T, N> name_type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Schema

template<class T> struct Schema;

///////////////////////////////////////////////////////////////////////////////
// struct Schema<void>

template<>
struct Schema<void>
{
	template<class T>
	static netsnmp_handler_registration* table(Netsnmp_Node_Handler* handler_,
			void* my_, int mode_)
	{
		netsnmp_mib_handler* h = netsnmp_create_handler(Schema<T>::name(), handler_);
		if (NULL == h)
			return NULL;

		h->myvoid = my_;
		Oid_type u = Schema<T>::uuid();
		netsnmp_handler_registration* r = 
				netsnmp_handler_registration_create(Schema<T>::name(), h,
					&u[0], u.size(), mode_);
		if (NULL != r)
			return r;

		netsnmp_handler_free(h);
		return NULL;
	}
	static Oid_type uuid(Oid_type::value_type luid_);
};

namespace Details
{
///////////////////////////////////////////////////////////////////////////////
// struct Name

struct Name
{
	template<class U>
	struct apply
	{
		typedef typename U::name_type type;
	};
};

///////////////////////////////////////////////////////////////////////////////
// struct Producer

template<class T>
struct Producer
{
	template<class N>
	struct Slub: Asn::ValueFactory<T::asn_type::value>
	{
		typedef T declaration_type;
		enum
		{
			ASN_TYPE = T::asn_type::value
		};
	};
	typedef Slub<typename T::name_type> type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Tuple

template<class T>
struct Tuple
{
	typedef Schema<T> schema_type;
	typedef typename mpl::inherit_linearly<schema_type,
				mpl::inherit<mpl::_1, Producer<mpl::_2> > >::type type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Names

template<class T>
struct Names
{
	typedef typename Tuple<T>::schema_type seq_type;
	typedef typename mpl::transform<seq_type,
			mpl::apply1<Name, mpl::_1> >::type type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Select

struct Select
{
	template<class T>
	struct apply
	{
		typedef typename Tuple<typename T::value_type>::schema_type
				seq_type;
		typedef typename mpl::find_if<seq_type,
					boost::is_same<T, mpl::apply1<Name, mpl::_> >
						>::type iterator_type;
		BOOST_MPL_ASSERT((mpl::not_<boost::is_same<iterator_type,
				typename mpl::end<seq_type>::type> >));
		typedef typename mpl::deref<iterator_type>::type type;
	};
};

///////////////////////////////////////////////////////////////////////////////
// struct Key

template<class T>
struct Key
{
	typedef typename Tuple<T>::schema_type::index_type seq_type;
	typedef typename mpl::inherit_linearly<seq_type,
				mpl::inherit<mpl::_1, Producer<mpl::apply1<Select, mpl::_2> > > >::type type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Column

template<class T, T N>
struct Column
{
	typedef typename mpl::integral_c<T, N> needle_type;
	typedef typename Producer<typename mpl::apply1<Select, needle_type>::type>::type type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Mutable

template<class T>
struct Mutable
{
	typedef typename Tuple<T>::schema_type seq_type;
	typedef typename seq_type::index_type filter_type;

	typedef typename mpl::copy_if<seq_type,
			mpl::not_<mpl::contains<filter_type, mpl::apply1<Name, mpl::_1> > > >::type type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Limits

template<class T>
struct Limits
{
	typedef typename mpl::max_element<typename Tuple<T>::schema_type,
			mpl::less<mpl::apply1<Name, mpl::_1>, mpl::apply1<Name, mpl::_2> >
		>::type max_p;
	typedef typename mpl::deref<max_p>::type max_type;
	typedef typename mpl::max_element<typename Tuple<T>::schema_type,
			mpl::less<mpl::apply1<Name, mpl::_2>, mpl::apply1<Name, mpl::_1> >
		>::type min_p;
	typedef typename mpl::deref<min_p>::type min_type;
	enum
	{
		MAX = max_type::name_type::value,
		MIN = min_type::name_type::value
	};
};

///////////////////////////////////////////////////////////////////////////////
// struct Set

template<class T, class B>
struct Set: private B
{
	template<T N>
	typename Column<T, N>::type::value_type get() const
	{
		return static_cast<const typename Column<T, N>::type* >(this)->get();
	}
	template<class U, U N>
	void get(netsnmp_variable_list& dst_) const
	{
		static_cast<const typename Column<U, N>::type* >(this)->get(dst_);
	}
	template<class U, U N>
	typename Column<U, N>::type::value_type get() const
	{
		return static_cast<const typename Column<U, N>::type* >(this)->get();
	}
	template<T N>
	void put(const typename Column<T, N>::type::value_type& value_)
	{
		return static_cast<typename Column<T, N>::type* >(this)->put(value_);
	}
	template<T N>
	void put(const netsnmp_variable_list& value_)
	{
		static_cast<typename Column<T, N>::type* >(this)->put(value_);
	}
	template<class U, U N>
	void put(const typename Column<U, N>::type::value_type& value_)
	{
		return static_cast<typename Column<U, N>::type* >(this)->put(value_);
	}
};

namespace Index
{
///////////////////////////////////////////////////////////////////////////////
// struct Name

template<class T, class U>
struct Name
{
	typedef typename Key<T>::seq_type seq_type;
	typedef typename mpl::find<seq_type, U>::type pos_type;
	BOOST_MPL_ASSERT((mpl::not_<boost::is_same<pos_type,
			typename mpl::end<seq_type>::type> >));
	typedef typename mpl::distance<typename mpl::begin<seq_type>::type, pos_type>::type off_type;
	typedef typename mpl::if_<boost::is_same<T, typename U::value_type>, U,
				mpl::int_<Limits<T>::MAX + 1 + off_type::value> >::type type;
};

///////////////////////////////////////////////////////////////////////////////
// struct Inject

template<class T>
struct Inject
{
	explicit Inject(netsnmp_table_registration_info* table_): m_table(table_)
	{
		table_->min_column = Limits<T>::MIN;
		table_->max_column = Limits<T>::MAX;
	}
	template<class U>
	void operator()(U )
	{
		int t = Details::Column<typename U::value_type,
					U::value>::type::ASN_TYPE;
		netsnmp_variable_list* x = m_table->indexes;
		netsnmp_table_helper_add_index(m_table, t);
		x = NULL == x ? m_table->indexes : x->next_variable;
		if (NULL != x)
			x->index = Details::Index::Name<T, U>::type::value;
	}
private:
	netsnmp_table_registration_info* m_table;
};

///////////////////////////////////////////////////////////////////////////////
// struct Patch

template<class T>
struct Patch
{
	explicit Patch(netsnmp_variable_list* list_): m_list(list_)
	{
	}
	template<class U>
	void operator()(U )
	{
		if (NULL == m_list)
			return;
		m_list->index = Details::Index::Name<T, U>::type::value;
		m_list = m_list->next_variable;
	}
private:
	netsnmp_variable_list* m_list;
};

} // namespace Index

namespace Dispatcher
{
///////////////////////////////////////////////////////////////////////////////
// struct Default

struct Default
{
	template<class A, class E>
	static void do_(A& automat_, int case_, E event_)
	{
		automat_.unknown(case_, event_);
	}
};

///////////////////////////////////////////////////////////////////////////////
// struct Unit

template<class T, class N>
struct Unit
{
	typedef typename T::event_type event_type;
	typedef typename T::automat_type automat_type;

	static void do_(automat_type& automat_, int case_, event_type event_)
	{
		if (T::this_case == case_)
			return (void)T::execute(automat_, event_);

		return (void)N::do_(automat_, case_, event_);
	}
};

///////////////////////////////////////////////////////////////////////////////
// struct Generate

template<class T>
struct Generate
{
	typedef typename mpl::fold<
			T, Default, Unit<mpl::_2, mpl::_1>
		>::type type;
};

} // namespace Dispatcher

///////////////////////////////////////////////////////////////////////////////
// struct Automat

template<class P, class E>
struct Automat
{
	void do_(int case_, E event_)
	{
		typedef typename Dispatcher::Generate<
				typename P::table_type>::type
					dispatcher_type;
		P& p = *static_cast<P* >(this);
		dispatcher_type::do_(p, case_, event_);
	}
	void unknown(int case_, E event_)
	{
	}
protected:
	template<int N, void (P::* F)(E )>
	struct Row
	{
		typedef E event_type;
		typedef P automat_type;
		static const int this_case = N;

		static void execute(automat_type& automat_, event_type event_)
		{
			(automat_.*F)(event_);
		}
	};
};

} // namespace Details
} // namespace Rmond

#endif // DETAILS_H

