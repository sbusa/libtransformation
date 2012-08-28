#ifndef __corenova_sys_watchdog_H__
#define __corenova_sys_watchdog_H__

#include <corenova/interface.h>

#define WDT_DEFAULT_TIMEOUT 60

DEFINE_INTERFACE (Watchdog) {
	int		(*enable)		(const char *, int);
	int		(*disable)		(int);
	int		(*set_timeout)	(int, int);
	int		(*get_timeout)	(int, int *);
	int		(*keepalive)	(int);
};

#endif
