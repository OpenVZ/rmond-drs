#include "mib.h"
#include <signal.h>
#include <net-snmp/agent/agent_callbacks.h>

namespace
{
///////////////////////////////////////////////////////////////////////////////
// struct Callback

struct Callback
{
	static void eject()
	{
		snmp_unregister_callback(SNMP_CALLBACK_APPLICATION,
			SNMPD_CALLBACK_SEND_TRAP1, &do_, NULL, 0);
		snmp_unregister_callback(SNMP_CALLBACK_APPLICATION,
			SNMPD_CALLBACK_SEND_TRAP2, &do_, NULL, 0);
	}
	static int inject()
	{
		int e = netsnmp_register_callback(SNMP_CALLBACK_APPLICATION,
				SNMPD_CALLBACK_SEND_TRAP1, &do_, NULL,
				NETSNMP_CALLBACK_DEFAULT_PRIORITY);
		if (0 != e)
			return e;
		return netsnmp_register_callback(SNMP_CALLBACK_APPLICATION,
				SNMPD_CALLBACK_SEND_TRAP2, &do_, NULL,
				NETSNMP_CALLBACK_DEFAULT_PRIORITY);
	}
	static int do_(int major_, int minor_, void *server_, void *client_)
	{
		if (SNMP_CALLBACK_APPLICATION != major_)
			return 0;

		sigset_t a, o;
		switch (minor_)
		{
		case SNMPD_CALLBACK_SEND_TRAP1:
		case SNMPD_CALLBACK_SEND_TRAP2:
			snmp_log(LOG_WARNING, LOG_PREFIX"do real initialization "
						"of the "TOKEN" module\n");
			// NB. the main thread is the dedicated one for the
			// signal handling. all the rest should block all
			// the signals. this call is in context of the main
			// thread thus there is no race with signal handling
			// of the snmpd.
			sigfillset(&a);
			pthread_sigmask(SIG_BLOCK, &a, &o);
			Rmond::Central::init();
			pthread_sigmask(SIG_SETMASK, &o, NULL);
			// NB. callback is under the lock. need to move the
			// eject out of the lock.
			Rmond::Central::schedule(0, &eject);
			break;
		}
		return 0;
	}
};

} // namespace

extern "C"
{

void init_RmondMIB(void)
{
        snmp_log(LOG_WARNING, LOG_PREFIX"Initializing the "TOKEN" module\n");
	// NB. cannot do initialization before the fork in case of a daemon
	// snmpd. subscribe on the startup trap and do real init inside the
	// callback, then unsubscribe.
	int e = Callback::inject();
        snmp_log(LOG_WARNING, LOG_PREFIX"Done initalizing "TOKEN" module %d\n", e);
}

void deinit_RmondMIB(void)
{
        snmp_log(LOG_WARNING, LOG_PREFIX"Finalizing the "TOKEN" module\n");
	Rmond::Central::fini();
        snmp_log(LOG_WARNING, LOG_PREFIX"Done finalizing "TOKEN" module\n");
}

}

