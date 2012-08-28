#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "A database abstraction layer.",
	.implements  = LIST ("Database","DatabasePool","DatabaseTransaction","DatabaseTransactionList"),
	.requires    = LIST ("corenova.data.list", "corenova.data.string", "corenova.sys.loader")
};

#include <corenova/data/database.h>
#include <corenova/data/list.h>
#include <corenova/data/string.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

/***** Database Interface Implementation *****/

#include <corenova/sys/loader.h>

static database_t *
connectDatabase (db_parameters_t *params) {
	if (params && params->module) {
		module_t *dbModule = I (DynamicLoader)->load (params->module);
		if (dbModule) {
			if (I_ACCESS (dbModule, Database)) {
				/* make sure dbModule is not self! */
				if (dbModule != SELF) {
					database_t *db = I_ACCESS (dbModule, Database)->connect (params);
					if (db) {
						db->module = dbModule;
						return db;
					} else {
						DEBUGP (DERR,"connect","unable to connect to database.");
					}
				} else {
					DEBUGP (DERR,"connect","self referential module detected!");
				}
			} else {
				DEBUGP (DERR,"connect","'%s' does not implement Database interface!",params->module);
			}
			I (DynamicLoader)->unload (dbModule);
		} else {
			DEBUGP (DERR,"connect","unable to load '%s' database module!",params->module);
		}
	}
	return NULL;
}

static db_result_t *
executeQuery (database_t *db, const char *query) {
	if (db) {
		return I_ACCESS (db->module,Database)->execute (db,query);
	}
	return NULL;
}

static db_result_t *
prepare (database_t *db, const char *stmtName, const char *query, int nParams, const enum db_types *paramTypes) {

    if(db) {
        return I_ACCESS(db->module, Database)->prepare(db, stmtName, query, nParams, paramTypes);
    }
    
    return NULL;

}

static db_result_t *
executePrepared (database_t *db, const char *stmtName, int nParams, const char * const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) {

    if(db) {
        return I_ACCESS(db->module, Database)->executePrepared(db, stmtName, nParams, paramValues, paramLengths, paramFormats, resultFormat);    
    }
    
    return NULL;

}

static boolean_t
isPrepared (database_t *db, const char *stmtName) {
    if (db) {
        return I_ACCESS (db->module, Database)->isPrepared (db, stmtName);
    }
    return FALSE;
}

/*

    Execute given stored procedure with arguments.
    Is a DBMS-specific feature, so it has to be
    implemented by each relevant driver.


*/

static db_result_t *
executeProc (database_t *db, const char *procname,...) {
	if (db) {

	        va_list ap;
    		va_start (ap, procname);
		
		return I_ACCESS (db->module,Database)->executeProc (db,procname,ap);
	}
	return NULL;
}

/*

    Use of stored functions differs from stored procedures.
    Also PostgreSQL, for example, doesn't have procedures
    at all.

*/

static db_result_t *
executeFunc (database_t *db, const char *funcname,...) {
	if (db) {

	        va_list ap;
    		va_start (ap, funcname);

		return I_ACCESS (db->module,Database)->executeFunc (db,funcname,ap);
	}
	return NULL;
}

static db_result_t *
executeFunc2 (database_t *db, const char *funcname, char **argv, int32_t argc) {
	if (db) {
		return I_ACCESS (db->module,Database)->executeFunc2 (db,funcname,argv, argc);
	}
	return NULL;
}

static db_result_t *
executeFunc3 (database_t *db, const char *funcname, char **argv, char **type, int32_t argc) {
	if (db) {
		return I_ACCESS (db->module,Database)->executeFunc3 (db,funcname,argv, type, argc);
	}
	return NULL;
}

static db_row_t
getRow (database_t *db, db_result_t *result) {
	if (db) {
		return I_ACCESS (db->module,Database)->getRow (db,result);
	}
	return NULL;
}

static void
freeResult (database_t *db, db_result_t *result) {
	if (db)	 {
		I_ACCESS (db->module,Database)->freeResult (db,result);
	}
}

/*
  Works like printf, supports ALL format modifiers.

  Escapes strings to database supported escaped encoding!
  
  This basically wraps around vsprintf, except that the %s format strings
  are captured and handled separately!
*/
static char *
queryString (database_t *db, char *format, ...) {
	if (db) {
		va_list ap;
		va_start (ap,format);
		db->passThrough = TRUE;
		return I_ACCESS (db->module,Database)->queryString (db,format,ap);
	}
	return NULL;
}

static char *
timeString (database_t *db, struct timeval *tv) {
	if (db) {
		return I_ACCESS (db->module,Database)->timeString (db,tv);
	}
	return NULL;
}

static void
destroyDatabase (database_t **dbPtr) {
	if (dbPtr) {
		database_t *db = *dbPtr;
		if (db) {
			if (db->inPool) {
				db->poolAccessors--;
			} else {
				/* not in pool, but may have been simply detached */
				if (!db->poolAccessors ||
					--db->poolAccessors == 0) {
					module_t *module = db->module;
					I_ACCESS (db->module,Database)->destroy (dbPtr);
					I (DynamicLoader)->unload (module);
				}
			}
			*dbPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (Database) = {
	.connect         = connectDatabase,
	.execute         = executeQuery,
	.executeFunc     = executeFunc,
	.executeFunc2    = executeFunc2,
	.executeFunc3    = executeFunc3,
	.executeProc     = executeProc,
	.getRow          = getRow,
	.freeResult      = freeResult,
	.queryString     = queryString,
	.timeString      = timeString,
	.destroy         = destroyDatabase,
	.prepare         = prepare,
	.executePrepared = executePrepared,
    .isPrepared      = isPrepared
};

/***** DatabasePool Interface Implementation *****/

typedef struct {

	db_parameters_t key;
	database_t *database;
	
} database_pool_entry_t;

static void
_destroyDatabasePoolEntry (database_pool_entry_t **ePtr) {
	if (ePtr) {
		database_pool_entry_t *entry = *ePtr;
		if (entry) {
            if (entry->database) {
                entry->database->inPool = FALSE;
                /*
                  We should only destroy database when we KNOW that there are
                  nobody looking at the database any more.
                */
                if (!entry->database->poolAccessors)
                    I (Database)->destroy (&entry->database);
            }
			free (entry->key.module);
			free (entry->key.host);
			free (entry->key.user);
			free (entry->key.pass);
			free (entry->key.dbname);
			free (entry);
			*ePtr = NULL;
		}
	}
}

static database_pool_t *
newDatabasePool (void) {
	return I (List)->new ();
}

static database_t *
connectDatabasePool (database_pool_t *pool, db_parameters_t *params) {
	list_item_t *item = NULL;

	MUTEX_LOCK (pool->lock);
	if ((item = I (List)->first (pool))) {
		do {
			database_pool_entry_t *entry = (database_pool_entry_t *)item->data;
			if (entry) {
				if (entry->key.module || params->module) {
					if (!I (String)->equal (entry->key.module,params->module))
						continue;
				}
				if (entry->key.host || params->host) {
					if (!I (String)->equal (entry->key.host, params->host))
						continue;
				}
				if (entry->key.user || params->user) {
					if (!I (String)->equal (entry->key.user, params->user))
						continue;
				}
				if (entry->key.pass || params->pass) {
					if (!I (String)->equal (entry->key.pass, params->pass))
						continue;
				}
				if (entry->key.dbname || params->dbname) {
					if (!I (String)->equal (entry->key.dbname, params->dbname))
						continue;
				}
				if (entry->key.port != params->port)
					continue;
				/*
				  Found a match in the Pool storage!
				*/
				entry->database->poolAccessors++;
				MUTEX_UNLOCK (pool->lock);

				DEBUGP (DINFO,"connectDatabasePool","Reusing existing database connection from Pool for %s@%s to %s!",
						params->user,params->host,params->dbname);
				
				return entry->database;
			}
		} while ((item = I (List)->next (item)));
	}
	MUTEX_UNLOCK (pool->lock);
	
	database_pool_entry_t *entry = (database_pool_entry_t *)calloc (1,sizeof (database_pool_entry_t));
	if (entry) {
	
	
		database_t *db = I (Database)->connect (params);
		if (db) {
			DEBUGP (DDEBUG,"connectDatabasePool","Successful connection to database via Pool!");
			entry->database   = db;
			entry->key.module = I (String)->copy (params->module);
			entry->key.host   = I (String)->copy (params->host);
			entry->key.user   = I (String)->copy (params->user);
			entry->key.pass   = I (String)->copy (params->pass);
			entry->key.dbname = I (String)->copy (params->dbname);
			entry->key.port   = params->port;
			
			if (I (List)->insert (pool,I (ListItem)->new (entry))) {
				DEBUGP (DDEBUG,"connectDatabasePool","Added this database into Pool!");
				db->inPool = TRUE;
				db->poolAccessors = 1;
				return db;
			} else {
				DEBUGP (DERR,"connectDatbasePool","unable to add new database connection into pool!");
			}
		} else {
			DEBUGP (DERR,"connectDatabasePool","unable to connect to database via pool!");
		}
        _destroyDatabasePoolEntry (&entry);
	}
	return NULL;
}

static uint32_t
cleanDatabasePool (database_pool_t *pool) {
	uint32_t cleanCounter = 0;
	list_item_t *item = I (List)->first (pool);
	while (item) {
		list_item_t *next = I (List)->next (item);
		database_pool_entry_t *entry = (database_pool_entry_t *)item->data;
		if (entry && !entry->database->poolAccessors) {
			list_item_t *old = I (List)->remove (pool,item);
			_destroyDatabasePoolEntry (&entry);
			I (ListItem)->destroy (&old);
			cleanCounter++;
		}	
		item = next;
	}
	return cleanCounter;
}

static void
destroyDatabasePool (database_pool_t **poolPtr) {
	if (poolPtr) {
		database_pool_t *pool = *poolPtr;
		if (pool) {
			list_item_t *item = NULL;
			while ((item = I (List)->pop (pool))) {
				database_pool_entry_t *entry = (database_pool_entry_t *)item->data;
				_destroyDatabasePoolEntry (&entry);
				I (ListItem)->destroy (&item);
			}
//			free (pool);
//			*poolPtr = NULL;
			I (List)->destroy((list_t**)poolPtr);
		}
	}
}

IMPLEMENT_INTERFACE (DatabasePool) = {
	.new     = newDatabasePool,
	.connect = connectDatabasePool,
	.clean   = cleanDatabasePool,
	.destroy = destroyDatabasePool
};

/***** DatabaseTransaction Interface Implementation *****/

static db_transaction_t *
newTransaction (database_t *db, const char *query) {
	db_transaction_t *trans = (db_transaction_t *)calloc (1,sizeof (db_transaction_t));
	if (trans) {
		trans->database = db;
		trans->query    = I (String)->copy (query);
	}
	return trans;
}

static boolean_t
executeTransaction (db_transaction_t *transaction) {

	db_result_t *res = I (Database)->execute (transaction->database, transaction->query);
	if (res) {
		if (transaction->result)
			I (Database)->freeResult (transaction->database,transaction->result);
		transaction->result = res;
		return TRUE;
	}
	return FALSE;
}

static void
destroyTransaction (db_transaction_t **tPtr) {
	if (tPtr) {
		db_transaction_t *transaction = *tPtr;
		if (transaction) {
			free (transaction->query);
			I (Database)->freeResult (transaction->database,transaction->result);
			free (transaction);
			*tPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (DatabaseTransaction) = {
	.new     = newTransaction,
	.execute = executeTransaction,
	.destroy = destroyTransaction
};

/***** DatabaseTransactionList Interface Implementation *****/

static db_transactions_t *
newDatabaseTransactionList (void) {
	return I (List)->new ();
}

static boolean_t
addDatabaseTransaction (db_transactions_t *list, db_transaction_t *trans) {
	if (list && trans) {
		return I (List)->insert (list, I (ListItem)->new (trans))?TRUE:FALSE;
	}
	return FALSE;
}

static boolean_t
executeDatabaseTransactionList (db_transactions_t *list) {
	uint32_t numQueries = I (List)->count (list);
	uint32_t numSuccess = 0;
	DEBUGP (DINFO,"executeDatabaseTransactionList","executing %u transaction queries",numQueries);
	if (numQueries) {
		list_item_t *item = I (List)->first (list);
		while (item) {
			db_transaction_t *transaction = (db_transaction_t *)item->data;
			if (transaction) {
				if (I (DatabaseTransaction)->execute (transaction)) {
					numSuccess++;
				} else {
					break;
				}
			}
			item = I (List)->next (item);
		}
		DEBUGP (DINFO,"executeDatabaseTransactionList","(%u/%u) transactions executed!",numSuccess,numQueries);
	}
	return (numQueries == numSuccess)?TRUE:FALSE;
}

static void
destroyDatabaseTransactionList (db_transactions_t **lPtr) {
	if (lPtr) {
		db_transactions_t *list = *lPtr;
		if (list) {
			list_item_t *item = NULL;
			while ((item = I (List)->pop (list))) {
				db_transaction_t *transaction = (db_transaction_t *)item->data;
				I (DatabaseTransaction)->destroy (&transaction);
				I (ListItem)->destroy (&item);
			}
			free (list);
			*lPtr = NULL;
		}
	}
}

IMPLEMENT_INTERFACE (DatabaseTransactionList) = {
	.new     = newDatabaseTransactionList,
	.add     = addDatabaseTransaction,
	.execute = executeDatabaseTransactionList,
	.destroy = destroyDatabaseTransactionList
};
