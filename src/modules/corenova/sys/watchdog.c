#include <corenova/source-stub.h>

THIS = {
	.version = "2.0",
	.author  = "Rick Jen <rickijen@gmail.com>",
	.description = "This module handles watchdog operations.",
	.implements = LIST ("Watchdog")
};

#include <corenova/sys/watchdog.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <errno.h>
#include <unistd.h>

/*
 *  When the watchdog is opened for the very first time, the default timeout
 *  is 60 seconds. If the watchdog timeout had been set previously, the timeout
 *  value will remain the same as set previously unless it's reset with new
 *  timeout.
 *
 *  Once the watchdog is opened, its internal timer will start to tick down and
 *  trigger a system reset. If the device is not closed properly, timer will not
 *  stop.
 */
const char wdtDevice[] = "/dev/watchdog";

static int
wdtOpen (const char *wdtDev, int newTimeout) {
	int fd=0;

	/* use default wdt device if needed */
	if (!wdtDev)
		wdtDev = wdtDevice;

	/* open the watchdog device */
	if((fd=open(wdtDev, O_RDWR | O_SYNC)) == -1)
	{
		int errNo = errno;
		DEBUGP (DERR, "wdtOpen",
			"Open %s failed: %d. Ensure module watchdog is inserted "
			"and /dev/watchdog exists\nIf necessary, create "
			"with: mknod /dev/watchdog c 10 130\n", wdtDev, errNo);
		return -1;
	}

	/* set timeout */
	if (ioctl(fd, WDIOC_SETTIMEOUT, &newTimeout) == -1) {
		int errNo = errno;
		DEBUGP (DERR, "wdtOpen", "ioctl error: %d", errNo);
		return -1;
	}

	DEBUGP (DDEBUG,"wdtOpen", "new timeout: %d", newTimeout);
	return fd;
}

static int
wdtClose (int fd) {
	/* write one byte to watchdog */
	write(fd, "V", 1);

	/* close the watchdog device */
	if((fd=close(fd)) == -1)
	{
		int errNo = errno;
		DEBUGP (DERR, "wdtClose", "Close failed: %d.", errNo);
		return -1;
	}

	DEBUGP (DDEBUG,"wdtClose","watchdog disabled");
	return 0;
}

static int
wdtSetTimeOut (int fd, int newTimeout) {
	if (ioctl(fd, WDIOC_SETTIMEOUT, &newTimeout) == -1) {
		int errNo = errno;
		DEBUGP (DERR,"wdtSetTimeOut","ioctl error: %d", errNo);
		return -1;
	}

	DEBUGP (DDEBUG,"wdtSetTimeOut","new timeout: %d", newTimeout);
	return 0;
}

static int
wdtGetTimeOut (int fd, int *curTimeout) {
	if (ioctl(fd, WDIOC_GETTIMEOUT, curTimeout) == -1) {
		int errNo = errno;
		DEBUGP (DERR,"wdtGetTimeOut","ioctl error: %d", errNo);
		return -1;
	}

	DEBUGP (DDEBUG,"wdtGetTimeOut","current timeout: %d", *curTimeout);
	return 0;
}

/*
 * This function simply sends an IOCTL to the driver, which in turn ticks
 * the PC Watchdog card to reset its internal timer so it doesn't trigger
 * a computer reset.
 */
static int
wdtKeepAlive (int fd) {
	if (ioctl(fd, WDIOC_KEEPALIVE, NULL) == -1) {
		int errNo = errno;
		DEBUGP (DERR,"wdtKeepAlive","ioctl error: %d", errNo);
		return -1;
	}

	DEBUGP (DDEBUG,"wdtKeepAlive","kick the dog.");
	return 0;
}

IMPLEMENT_INTERFACE (Watchdog) = {
	.enable      = wdtOpen,
    .set_timeout = wdtSetTimeOut,
	.get_timeout = wdtGetTimeOut,
	.keepalive   = wdtKeepAlive,
	.disable     = wdtClose,
};
