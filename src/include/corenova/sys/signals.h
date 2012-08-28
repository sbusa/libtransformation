#ifndef __signals_H__
#define __signals_H__

#include <corenova/interface.h>
#include <signal.h> 			/* SIGXXXX constants */

#if defined (solaris2)
# include <sys/signal.h>
#endif

extern int lastSignal;

typedef void (*signal_cb_func)(void);

/*
  put together the function interface available for export so that other
  modules can call them!
*/
DEFINE_INTERFACE (Signal)
{
	void (*handler) (int32_t signum, signal_cb_func func);
};

#endif
