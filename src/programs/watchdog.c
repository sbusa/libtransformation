#include <corenova/source-stub.h>

THIS = {
	.name = "Watchdog",
	.version = "1.0",
	.author = "Rick Jen <rickijen@gmail.com>",
	.description = "This program will set up the watchdog device.",
	.requires = LIST ("corenova.sys.watchdog",
					  "corenova.sys.signals",
					  "corenova.sys.getopts"),
	.options = {
		OPTION ("wdt_timeout", "20", "watchdog timeout in seconds."),
		OPTION ("wdt_sleep", "10", "seconds of sleep before kicking it again"),
		OPTION_NULL
	}
};

#include <corenova/sys/debug.h>
#include <corenova/sys/watchdog.h>
#include <corenova/sys/getopts.h>
#include <corenova/sys/signals.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h> 			/* for sleep */
#define DEF_SLEEP 10
static int fd = 0;

static void
_signalCloseWdt (void) {
	if (fd != -1) {
		/* Be careful.
		 * If the kernel was compiled with the "Disable watchdog shutdown on close"
		 * support, then the next write & close operations will not disable the
		 * watchdog.
		 */
		I (Watchdog)->disable(fd);
		DEBUGP (DDEBUG,"_signalCloseWdt", "wdt closed.");
	}

	exit(0);
}

int32_t main(int32_t argc, char **argv, char **envp)
{
	int currentTimeout=0;

	/* signal handler */
	I (Signal)->handler(SIGINT,  _signalCloseWdt);

	/* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
		char *ptr=NULL;
		int timeout = 0;
		int sleeptime = 0;

		ptr = I (Parameters)->getValue (params,"wdt_timeout");
		if (ptr) {
			timeout = atoi(ptr);
			ptr = NULL;
		}
		ptr = I (Parameters)->getValue (params,"wdt_sleep");
		if (ptr) {
			sleeptime = atoi(ptr);
			ptr = NULL;
		}

		/* enable the watchdog */
		fd = I (Watchdog)->enable ("/dev/watchdog",
				(timeout?timeout:WDT_DEFAULT_TIMEOUT));

		I (Watchdog)->get_timeout(fd, &currentTimeout);
		DEBUGP (DDEBUG,"main","currentTimeout=%d", currentTimeout);

		/* kicking the dog in a while loop */
		while(1)
		{
			I (Watchdog)->keepalive(fd);
			sleep(sleeptime?sleeptime:DEF_SLEEP);
		}

	} else {
		DEBUGP (DERR,"main","no parameters?");
	}

    I (Parameters)->destroy (&params);
	return 0;
}

