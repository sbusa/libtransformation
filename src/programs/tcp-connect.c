#include <corenova/source-stub.h>

THIS = {
	.name = "Tcp connector",
	.version = "1.0",
	.author = "Peter K. Lee <saint@corenova.com>",
	.description = "This program lets you make tcp connections.",
	.requires = LIST ("corenova.data.inifile",
					  "corenova.net.tcp",
					  "corenova.sys.getopts"),
	.options = {
		OPTION ("config_file", NULL, "name of configuration ini file"),
		OPTION ("host", NULL, "hostname to connect to"),
		OPTION ("port", NULL, "port to connect to"),
		OPTION_NULL
	}
};

#include <corenova/data/inifile.h>
#include <corenova/net/tcp.h>
#include <corenova/sys/getopts.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h> 			/* for sleep */

int32_t main(int32_t argc, char **argv, char **envp)
{
	/* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
		char *host = I (Parameters)->getValue (params,"host");
		char *port = I (Parameters)->getValue (params,"port");

		if (host && port) {
			DEBUGP (DINFO,"main","making a connection to %s:%s",host,port);
			tcp_t *conn = I (TcpConnector)->connect (host,atoi (port));
            if (conn) {
                DEBUGP (DINFO,"main","woohoo, got conn!");
            }
		} else {
			DEBUGP (DERR,"main","must provide host and port to connect!");
			exit (1);
		}
	} else {
		DEBUGP (DERR,"main","no parameters?");
	}
    I (Parameters)->destroy (&params);
	return 0;
}

