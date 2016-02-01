ARCH=$(shell uname -i)
ifeq "${ARCH}" "x86_64"
LIBDIR ?= /usr/lib64
else
LIBDIR ?= /usr/lib
endif
DATADIR ?= /usr/share

OBJS=scheduler.lo value.lo asn.lo environment.lo ve.lo details.lo host.lo container.lo mib.lo sink.lo rmond.lo system.lo
TARGET=rmond.so

#CFLAGS=$(shell net-snmp-config --cflags) -fPIC -Wall -Werror
CFLAGS=-DNETSNMP_ENABLE_IPV6 -O0 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=0 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic -D_RPM_4_4_COMPAT -Ulinux -Dlinux=linux -I/usr/include/rpm -D_REENTRANT -D_GNU_SOURCE -fno-strict-aliasing -pipe -fstack-protector -I/usr/local/include -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I/usr/lib64/perl5/CORE -I. -I/usr/include -fPIC -Wall -Werror
#SWALIBS=-g -Wl,-Bdynamic -lpthread -lprl_sdk -Wl,-Bstatic -lboost_thread-mt -Wl,-Bdynamic
SWALIBS=-g -Wl,-Bdynamic -lpthread -lprl_sdk -Wl,-Bdynamic
CXXFLAGS+= $(CFLAGS) -Wno-ctor-dtor-privacy
BUILDLIBS=$(shell net-snmp-config --libs)
BUILDAGENTLIBS=$(shell net-snmp-config --agent-libs)

# shared library flags (assumes gcc)
LDFLAGS=-shared

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(LDFLAGS) $(OBJS) $(BUILDAGENTLIBS) $(SWALIBS)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(DATADIR)/snmp/mibs
	mkdir -p $(DESTDIR)$(LIBDIR)/rmond
	install -m 644 ../mibs/*.txt $(DESTDIR)$(DATADIR)/snmp/mibs/
	install -m 744 $(TARGET) $(DESTDIR)$(LIBDIR)/rmond/$(TARGET)

clean-depend:
	rm -f $(OBJS:.lo=.dep)

clean:
	rm -f $(OBJS) $(TARGET)

.SUFFIXES: .lo .dep

.c.dep:
	$(CC) -MT $(<:.c=.lo) -MM $(CFLAGS) $< > $@

.cpp.dep:
	$(CXX) -MT $(<:.cpp=.lo) -MM $(CXXFLAGS) $< > $@

-include $(OBJS:.lo=.dep)

.c.lo:
	$(CC) -c $(CFLAGS) $< -o $(<:.c=.lo)

.cpp.lo:
	$(CXX) -c $(CXXFLAGS) $< -o $(<:.cpp=.lo)

