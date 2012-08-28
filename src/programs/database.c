#include <corenova/source-stub.h>

THIS = {
	.name = "Database Utility",
	.version = "1.0",
	.author = "Peter K. Lee <saint@corenova.com>",
	.description = "This program lets you work with a database.",
	.requires = LIST ("corenova.data.inifile",
					  "corenova.data.database",
					  "corenova.sys.getopts"),
	.options = {
		OPTION ("config_file", NULL, "name of configuration ini file"),
		OPTION ("module",   NULL, "database module to use"),
		OPTION ("host",     NULL, "hostname to connect to"),
		OPTION ("port",     NULL, "port to connect to"),
		OPTION ("user",     NULL, "user name of database"),
		OPTION ("password", NULL, "password for user"),
		OPTION ("dbname",   NULL, "name of database"),
		OPTION_NULL
	}
};

#include <corenova/data/inifile.h>
#include <corenova/data/database.h>
#include <corenova/sys/getopts.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

#include <unistd.h> 			/* for sleep */

int main(int argc, char **argv, char **envp)
{
	/* process command line arguments */
	parameters_t *params = I (OptionParser)->parse(&this,argc,argv);
	if (params && params->count) {
		char *port = I (Parameters)->getValue (params,"port");
		db_parameters_t dbParams = {
			.module = I (Parameters)->getValue (params,"module"),
			.host   = I (Parameters)->getValue (params,"host"),
			.user   = I (Parameters)->getValue (params,"user"),
			.pass   = I (Parameters)->getValue (params,"password"),
			.dbname = I (Parameters)->getValue (params,"dbname"),
			.port   = port?atoi (port):0
		};

		database_t *db = I (Database)->connect (&dbParams);
		if (db) {
			DEBUGP (DINFO,"main","congratulations! you are now connected!");
			char *query =
				I (Database)->queryString (db, "SELECT %s","NOW()");
			db_result_t *res = I (Database)->execute (db, query);
			if (res) {
				DEBUGP (DINFO,"main","got result: %li",(long int)res->value);
			} else {
				DEBUGP (DERR,"main","can't get result!!!");
			}
			free (query);
		} else {
			DEBUGP (DERR,"main","unable to connect to database!");
			exit (1);
		}
        I (Database)->destroy (&db);
	} else {
		DEBUGP (DERR,"main","no parameters?");
	}
    I (Parameters)->destroy (&params);
	return 0;
}

