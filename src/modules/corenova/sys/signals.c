#include <corenova/source-stub.h>

THIS = {
    .version = "2.1",
    .author = "Peter K. Lee <saint@corenova.com>",
    .description = "This module simplifies signal handling",
    .implements = LIST("Signal"),
    .requires = LIST("corenova.data.list")
};

#include <corenova/sys/signals.h>
#include <corenova/data/list.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <time.h>      /* for strftime and localtime */
#include <sys/time.h>  /* for gettimeofday */
#include <strings.h>            /* for bzero */

#if defined (linux)
#define SIGNALS_ENTRIES_MAXNUM __SIGRTMAX /* EVIL TO USE */
#elif defined (freebsd6) || defined (freebsd7) || defined(freebsd8)
#define SIGNALS_ENTRIES_MAXNUM _SIG_MAXSIG
#elif defined(solaris2)
#define SIGNALS_ENTRIES_MAXNUM MAXSIG
#else
#error unable to determine the maximum number of signals available on platform
#define SIGNALS_ENTRIES_MAXNUM
#endif

int lastSignal = 0;
static struct sigaction saved_action_table[SIGNALS_ENTRIES_MAXNUM];
/* enable multiple callbacks to be registered per signal */
static list_t *signal_handler_table[SIGNALS_ENTRIES_MAXNUM];
//static signal_cb_func signal_handler_table[SIGNALS_ENTRIES_MAXNUM];

static void
signalHandler(int32_t signum) {
    static struct sigaction newAction;
    static struct sigaction oldAction;
    static struct timeval tv;
    static char strTime[40];

    bzero(&newAction, sizeof (newAction));
    bzero(&oldAction, sizeof (oldAction));
    newAction.sa_handler = &signalHandler;
    if (sigaction(signum, &newAction, &oldAction) == -1) {
        DEBUGP(DINFO, "signalHandler", "no sigaction for %d", signum);
        return;
    }

    if (newAction.sa_handler != oldAction.sa_handler) {
        memcpy(&saved_action_table[signum], &oldAction, sizeof (oldAction));
        return;
    }

    /*
     * This is where we actually execute the callback
     */

    /* remember last signal and record the time of signal event */
    lastSignal = signum;
    gettimeofday(&tv, NULL);
    strftime(strTime, sizeof (strTime), "%Y-%m-%d %H:%M:%S", localtime((time_t *) & tv.tv_sec));
    DEBUGP(DINFO, "signalHandler",
            "received signal number %d on %s.%03ld",
            signum, strTime, tv.tv_usec / 1000);

    /* execute the appropriate signal handler */
    if (signal_handler_table[signum]) {
        list_item_t *item = I(List)->first(signal_handler_table[signum]);
        DEBUGP(DINFO, "signalHandler", "calling custom routine(s) for signal:%d!", signum);
        while (item) {
            signal_cb_func funk = (signal_cb_func) item->data;
            (*funk) (); // YEAH!
            item = I(List)->next(item);
        }
    } else if (saved_action_table[signum].sa_handler) {
        DEBUGP(DINFO, "signalHandler", "calling previous routine for signal:%d!", signum);
        (*saved_action_table[signum].sa_handler)(signum);
    } else {
        DEBUGP(DERR, "signalHandler", "no handler routine for signal:%d! exiting...", signum);
        exit(0);
    }
}

/*//////// API ROUTINES //////////////////////////////////////////*/

static void CONSTRUCTOR register_all_signals(void) {
    bzero(&signal_handler_table, sizeof (signal_handler_table));
    /*
     * int32_t i;
     * DEBUGP(2,"construct","INFO: catching ALL %d signals...",SIGNALS_ENTRIES_MAXNUM);
     * for (i = 1; i < SIGNALS_ENTRIES_MAXNUM; i++) {
     * 	signalHandler(i,NULL,NULL);
     * 	signal_handler_table[i] = NULL;
     * }
     */
}

static inline void
add_signal_handler(int32_t signum, signal_cb_func func) {

    if (!signal_handler_table[signum]) {
        list_t *mylist = I(List)->new();
        if (!mylist) {
            DEBUGP(DERR, "add_signal_handler", "cannot allocate a new list for signal handling!");
            return;
        }
        signal_handler_table[signum] = mylist;

        signalHandler(signum);
    }
    // add to the handler table list for this signal!
    I(List)->insert(signal_handler_table[signum], I(ListItem)->new(func));

}

/*
  initialize the module's function interface with actual functions!
 */
IMPLEMENT_INTERFACE(Signal) = {
    .handler = add_signal_handler
};
