#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Provides API to MySQL Database.",
	.implements  = LIST ("MySQL","Database")
};

#include <corenova/data/db/mysql.h>
#include <corenova/data/database.h>

/*//////// MODULE CODE //////////////////////////////////////////*/

void CONSTRUCTOR __mysql_init__ () {
	my_init ();					/* make it thread safe! */
}

static MYSQL *
_connect (char *host, char *user, char *pass, char *dbname,
		  uint32_t port, char *socket, uint32_t flags) {
	MYSQL *conn = mysql_init(NULL);
	if (!conn) return NULL;
	
	mysql_options(conn, MYSQL_OPT_RECONNECT, "\x01"); // automatically reconnect when connection is lost
	
#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 32200 // 3.22 and up
	if (mysql_real_connect(conn, host, user, pass, dbname,
						   port, socket, flags) == NULL){
		DEBUGP(DERR,"connect","MySQL Error(%i): %s",mysql_errno (conn), mysql_error (conn));
		return (NULL);
	}
#else // pre 3.22
	if (mysql_real_connect(conn, host, user, pass,
						   port, socket, flags) == NULL){
		DEBUGP(DERR,"connect","MySQL Error(%i): %s",mysql_errno (conn), mysql_error (conn));
		return (NULL);
	}
	if (dbname != NULL){ // simulate effect of db_name parameter
		if (mysql_select_db(conn, dbname) != 0){
			DEBUGP(DERR,"connect","MySQL Error(%i): %s",mysql_errno (conn), mysql_error (conn));
			mysql_close(conn);
			return (NULL);
		}
	}
#endif
	return (conn);
}

static void
_close (MYSQL *conn) {
	if (conn) mysql_close (conn);
}

static int32_t
_query (MYSQL *conn, const char *sql) {
	int32_t affected_rows;

//	DEBUGP(DMSG,"query","%s",sql);
	if (mysql_query(conn,sql) != 0){
		DEBUGP (DERR,"query","MySQL Error(%i): %s",mysql_errno (conn), mysql_error (conn));
        DEBUGP (DERR,"query","Attempted: %s",sql);
		return (-1);
	}

	if ((affected_rows = (long) mysql_affected_rows(conn))!=-1)
		return (affected_rows);

	return (0);
}

/*
  Thread-safe way of querying the MySQL database (available only if
  pthreads is available!)

  Either returns number of rows retrieved, or number of rows affected (if
  SELECT)
*/
static int32_t
_execute (MYSQL *conn, const char *query, MYSQL_RES **result) {
	MODULE_LOCK ();
	int32_t affectedRows = I (MySQL)->query (conn,query);
	MYSQL_RES *res = (affectedRows != -1)?I (MySQL)->getResult (conn):NULL;
	MODULE_UNLOCK ();
	
	if (res) {
		*result = res;
		return I (MySQL)->numRows (res);
	} else {
		if (I (MySQL)->countFields (conn) == 0)
			return affectedRows;
		else {
			DEBUGP (DERR,"execute","unable to return result set!");
			return -1;
		}
	}
}

static int32_t                                                                                                                                              
_executeFunc (MYSQL *conn, const char *funcname, MYSQL_RES **result, va_list ap) { 

        char *param = NULL;
                                                                                                                                                             
        char stmtBuffer[DB_QUERY_MAXLEN];
                                                                                                                                                             
        int32_t l;
                                                                                                                                                             
        l = sprintf(stmtBuffer, "select %s(", funcname);

        while((param = va_arg (ap, char *))) {

            if(param == NULL) {

		l+= sprintf(stmtBuffer+l, "NULL,");
                continue;
		
	    }

            if(l+(strlen(param)*2) > DB_QUERY_MAXLEN) {

		l+= sprintf(stmtBuffer+l, "NULL,");
                continue;
		
	    }

	    l+= sprintf(stmtBuffer+l, "'%s',", I (MySQL)->escapeString(conn, param));

	}

	stmtBuffer[l-1] = ')';
	stmtBuffer[l] = '\0';
						                                                                                                                                                                 

	return I (MySQL)->execute(conn, stmtBuffer, result);
									    

}

static int32_t
_executeFunc2 (MYSQL *conn, const char *funcname, MYSQL_RES **result, char **argv, int32_t argc) { 

        char stmtBuffer[DB_QUERY_MAXLEN];
                                                                                                                                                             
        int32_t l, i;
                                                                                                                                                             
        l = sprintf(stmtBuffer, "select %s(", funcname);

        for(i = 0; i < argc; i++) {

            if(argv[i] == NULL) {

		l+= sprintf(stmtBuffer+l, "NULL,");
                continue;
		
	    }

            if(l+(strlen(argv[i])*2) > DB_QUERY_MAXLEN) {

		l+= sprintf(stmtBuffer+l, "NULL,");
                continue;
		
	    }

	    l+= sprintf(stmtBuffer+l, "'%s',", I (MySQL)->escapeString(conn, argv[i]));

	}

	stmtBuffer[l-1] = ')';
	stmtBuffer[l] = '\0';
						                                                                                                                                                                 

	return I (MySQL)->execute(conn, stmtBuffer, result);                                                                                                
									    

}

static int32_t
_executeFunc3 (MYSQL *conn, const char *funcname, MYSQL_RES **result, char **argv, char **type, int32_t argc) { 

    return _executeFunc2(conn, funcname, result, argv, argc);

}

static int32_t                                                                                                                                              
_executeProc (MYSQL *conn, const char *procname, MYSQL_RES **result, va_list ap) { 

        char *param = NULL;
                                                                                                                                                             
        char stmtBuffer[DB_QUERY_MAXLEN];
                                                                                                                                                             
        int32_t l;
                                                                                                                                                             
        l = sprintf(stmtBuffer, "call %s(", procname);

        while((param = va_arg (ap, char *))) {

            if(l+(strlen(param)*2) > DB_QUERY_MAXLEN)
                break;

	    l+= sprintf(stmtBuffer+l, "'%s',", I (MySQL)->escapeString(conn, param));

	}

	stmtBuffer[l-1] = ')';
	stmtBuffer[l] = '\0';
						                                                                                                                                                                 

	return I (MySQL)->execute(conn, stmtBuffer, result);

}

static MYSQL_RES *
_getResult (MYSQL *conn) {
	return mysql_store_result (conn);
}

static void
_freeResult (MYSQL_RES *result) {
	if (result)
		mysql_free_result (result);
}

/*
  Can find out if a result should be expected.
*/
static int32_t
_countFields (MYSQL *conn) {
	return mysql_field_count (conn);
}

static int32_t
_numRows (MYSQL_RES *result) {
	return (long int) mysql_num_rows (result);
}

static MYSQL_ROW
_getRow (MYSQL_RES *result) {
	return mysql_fetch_row(result);
}

#define TIMESTAMP_BUFFER_LEN 32
static char TimestampBuffer[TIMESTAMP_BUFFER_LEN];

static char *
_timestamp (struct timeval *tv) {
	char *tstamp = NULL;
	MODULE_LOCK ();
	if (TIMESTAMP_BUFFER_LEN >
		strftime(TimestampBuffer,TIMESTAMP_BUFFER_LEN,"%Y-%m-%d %H:%M:%S", gmtime((time_t *)&tv->tv_sec))) {
		tstamp = strdup (TimestampBuffer);
	}
	MODULE_UNLOCK ();
	return tstamp;
}

static char *
_escapeString (MYSQL *conn, const char *string) {
	char *eString = NULL;
	if (conn) {
		if (string) {
			uint32_t len = strlen (string);
			eString = (char *)calloc ((len*2)+1,sizeof (char));
			if (eString) {
				mysql_real_escape_string(conn, eString, string, len);
			} else {
				DEBUGP (DERR,"escapeString","OOM (req %u bytes)",(len *2)+1);
			}
		} else {
			eString = strdup (""); /* sure, why not. */
		}
	}
	return eString;
}

static int32_t
_snprintf (MYSQL *conn, char *str, size_t size, const char *format,...) {
	va_list ap;
	va_start (ap,format);
	return I (MySQL)->vsnprintf (conn,str,size,format,ap);
}

#define FORMAT_STRING_MAXLEN 32

static int32_t
_vsnprintf (MYSQL *conn, char *str, size_t size, const char *format, va_list ap) {
	const char *origFormat = format;
	uint32_t idx = 0;
	char formatBuffer[FORMAT_STRING_MAXLEN];
	if (!conn||!str) return -1;
	
	while (*format && idx < size) {
		if (*format == '%') {
			uint32_t lookAhead = 1;
			
			while (strchr (PrintfModifiers,format[lookAhead])) { lookAhead++; }
			
			if (strchr (PrintfConversion,format[lookAhead])) {
				/* Located the conversion character */
				if (lookAhead + 2 < FORMAT_STRING_MAXLEN) {
					memset (formatBuffer,'\0',sizeof (formatBuffer));
					if (strncpy (formatBuffer,format,lookAhead+2)) {
						char *string  = NULL;
						char *eString = NULL;
						size_t writtenLen = 0;
						
						switch (format[lookAhead]) {
						case 's':
							string = va_arg (ap, char *);
							eString = I (MySQL)->escapeString (conn,string);
							if (eString) {
								writtenLen = snprintf (str+idx,(size-idx),formatBuffer,eString);
								if (writtenLen != -1) {
									free (eString);
								} else {
									DEBUGP (DERR,"vsnprintf","unable to handle escaped string (%s)",eString);
									free (eString);
									return -1;
								}
							} else {
								DEBUGP (DERR,"vsnprintf","cannot escape string (%s)",string);
								return -1;
							}
							break;

						case '%':
							if (lookAhead != 1) {
								DEBUGP (DERR,"vsnprintf","malformed format section (%s)",formatBuffer);
								return -1;
							}

							str[idx] = '%';
							str[idx+1] = '\0';
							writtenLen = 2;
							break;
							
						default: {
							/* let vsprintf handle it... */
					
							va_list ap2;
							va_copy(ap2, ap);
						
							writtenLen = vsnprintf (str+idx,(size-idx),formatBuffer,ap2);
							
							va_end(ap2);
							
							if (writtenLen != -1) {
								va_arg (ap,void *); /* skip the last format + arg */
							} else {
								DEBUGP (DERR,"vsnprintf","unable to handle (%s)",formatBuffer);
								return -1;
							}
							
							}
							
						}
						
						format += (lookAhead+1);
						idx += (writtenLen-1);	/* don't count the terminator */

						continue; /* back to beginning! */
					}
				}
			} else {
				DEBUGP (DERR,"vsnprintf","malformed format section (%s) found in: %s",format,origFormat);
				return -1;
			}
		}
		/* just copy verbatim */
		str[idx++] = *format++;
	}
	str[idx] = '\0';			/* terminate string */
	return idx;
}

IMPLEMENT_INTERFACE (MySQL) = {
	.connect       = _connect,
	.close         = _close,
	.query         = _query,
	.execute       = _execute,
	.executeProc   = _executeProc,
	.executeFunc   = _executeFunc,
	.executeFunc2  = _executeFunc2,
	.executeFunc3  = _executeFunc3,
	.getResult     = _getResult,
	.freeResult    = _freeResult,
	.countFields   = _countFields,
	.numRows       = _numRows,
	.getRow        = _getRow,
	.timestamp     = _timestamp,
	.escapeString  = _escapeString,
	.snprintf      = _snprintf,
	.vsnprintf     = _vsnprintf
};


/***** Database Interface Implementation *****/

static database_t *
databaseConnect (db_parameters_t *params) {
	database_t *db = (database_t *) calloc (1,sizeof (database_t));
	if (db) {
		db->connection = I (MySQL)->connect (params->host,params->user,params->pass,params->dbname,
											 params->port, NULL, 0);
		if (db->connection) {
			DEBUGP (DINFO,"connect","successful connection to %s@%s:%u!",
					params->user,params->host,params->port);
		} else {
			I (Database)->destroy (&db);
		}
	}
	return db;
}

static void
databaseDestroy (database_t **dbPtr) {
	if (dbPtr) {
		database_t *db = *dbPtr;
		if (db) {
			I (MySQL)->close (db->connection);
			free (db);
			*dbPtr = NULL;
		}
	}
}

static db_result_t *
databaseExecute (database_t *db, const char *query) {
	db_result_t *result = (db_result_t *)calloc (1,sizeof (db_result_t));
	if (result) {
		MYSQL *connection = (MYSQL *)db->connection;
		MYSQL_RES *myResult = NULL;
		result->value = I (MySQL)->execute (connection,query,&myResult);
		if (result->value != -1) {
			result->set = (void *)myResult;
            return result;
		} else {
			DEBUGP (DERR,"databaseExecute","unable to retrieve result!");
			free (result);
		}
	}
	return NULL;
}

static db_result_t *                                                                                                                                         
databaseExecuteFunc (database_t *db, const char *funcname, ...) {                                                                                            
                                                                                                                                                             
        va_list ap;                                                                                                                                          
        va_start (ap,funcname);                                                                                                                              
                                                                                                                                                             
        db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));                                                                               
        if(result) {                                                                                                                                         
            MYSQL *connection = (MYSQL *)db->connection;                                                                                                     
            MYSQL_RES *myResult = NULL;                                                                                                                      
            ap = va_arg (ap,va_list);                                                                                                                        
            result->value = I (MySQL)->executeFunc (connection, funcname, &myResult, ap);                                                                    
            if(result->value != -1) {                                                                                                                        
                    result->set = (void *)myResult;                                                                                                          
                    return result;                                                                                                                           
                                                                                                                                                             
            } else {                                                                                                                                         
                                                                                                                                                             
                DEBUGP (DERR, "databaseExecuteFunc", "unable to retrieve result!");                                                                          
                free(result);                                                                                                                                
                                                                                                                                                             
            }                                                                                                                                                
                                                                                                                                                             
        }                                                                                                                                                    
	                                                                                                                                                             
	return NULL;                                                                                                                                         
		                                                                                                                                                             
}

static db_result_t *                                                                                                                                         
databaseExecuteFunc2 (database_t *db, const char *funcname, char **argv, int32_t argc) {
                                                                                                                                                             
        db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));
        if(result) {                                                                                                                                         
            MYSQL *connection = (MYSQL *)db->connection;                                                                                                     
            MYSQL_RES *myResult = NULL;                                                                                                                      
            result->value = I (MySQL)->executeFunc2 (connection, funcname, &myResult, argv, argc);
            if(result->value != -1) {                                                                                                                        
                    result->set = (void *)myResult;                                                                                                          
                    return result;                                                                                                                           
                                                                                                                                                             
            } else {                                                                                                                                         
                                                                                                                                                             
                DEBUGP (DERR, "databaseExecuteFunc2", "unable to retrieve result!");                                                                          
                free(result);                                                                                                                                
                                                                                                                                                             
            }                                                                                                                                                
                                                                                                                                                             
        }                                                                                                                                                    
	                                                                                                                                                             
	return NULL;                                                                                                                                         
		                                                                                                                                                             
}

static db_result_t *                                                                                                                                         
databaseExecuteFunc3 (database_t *db, const char *funcname, char **argv, char **type, int32_t argc) {
                                                                                                                                                             
        db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));
        if(result) {                                                                                                                                         
            MYSQL *connection = (MYSQL *)db->connection;                                                                                                     
            MYSQL_RES *myResult = NULL;                                                                                                                      
            result->value = I (MySQL)->executeFunc3 (connection, funcname, &myResult, argv, type, argc);
            if(result->value != -1) {                                                                                                                        
                    result->set = (void *)myResult;                                                                                                          
                    return result;                                                                                                                           
                                                                                                                                                             
            } else {                                                                                                                                         
                                                                                                                                                             
                DEBUGP (DERR, "databaseExecuteFunc2", "unable to retrieve result!");                                                                          
                free(result);                                                                                                                                
                                                                                                                                                             
            }                                                                                                                                                
                                                                                                                                                             
        }                                                                                                                                                    
	                                                                                                                                                             
	return NULL;                                                                                                                                         
		                                                                                                                                                             
}


static db_result_t *                                                                                                                                         
databaseExecuteProc (database_t *db, const char *procname, ...) {                                                                                            
                                                                                                                                                             
        va_list ap;                                                                                                                                          
        va_start (ap,procname);                                                                                                                              
                                                                                                                                                             
        db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));                                                                               
        if(result) {                                                                                                                                         
            MYSQL *connection = (MYSQL *)db->connection;                                                                                                     
            MYSQL_RES *myResult = NULL;                                                                                                                      
            ap = va_arg (ap,va_list);                                                                                                                        
            result->value = I (MySQL)->executeProc (connection, procname, &myResult, ap);                                                                    
            if(result->value != -1) {                                                                                                                        
                    result->set = (void *)myResult;                                                                                                          
                    return result;                                                                                                                           
                                                                                                                                                             
            } else {                                                                                                                                         
                                                                                                                                                             
                DEBUGP (DERR, "databaseExecuteFunc", "unable to retrieve result!");                                                                          
                free(result);                                                                                                                                
                                                                                                                                                             
            }                                                                                                                                                
                                                                                                                                                             
        }                                                                                                                                                    
	                                                                                                                                                             
	return NULL;                                                                                                                                         
		                                                                                                                                                             
}

static db_row_t 
databaseGetRow (database_t *db, db_result_t *result) {
	if (db) {
		MYSQL_RES *myResult = (MYSQL_RES *)result->set;
		return (db_row_t) I (MySQL)->getRow (myResult);
	}
	return NULL;
}

static void
databaseFreeResult (database_t *db, db_result_t *result) {
	if (result) {
		MYSQL_RES *myResult = (MYSQL_RES *)result->set;
		I (MySQL)->freeResult (myResult);
		free (result);
	}
}

static char *
queryString (database_t *db, char *format, ...) {
	if (db) {
		MYSQL *conn = (MYSQL *)db->connection;
		if (conn) {
			char *query = db->queryBuffer;
			va_list ap;
			va_start (ap,format);
			if (db->passThrough) { /* called via abstraction layer */
				ap = va_arg (ap,va_list); /* I can't believe this works! */
				db->passThrough = FALSE;
			}
			if (I (MySQL)->vsnprintf (conn,query,DB_QUERY_MAXLEN,format,ap) != -1){
				return strdup (query);
			}
		}
	}
	return NULL;
}

static char *
timeString (database_t *db, struct timeval *tv) {
	return I (MySQL)->timestamp (tv);
}

IMPLEMENT_INTERFACE (Database) = {
	.connect      = databaseConnect,
	.execute      = databaseExecute,
	.executeProc  = databaseExecuteProc,
	.executeFunc  = databaseExecuteFunc,
	.executeFunc2 = databaseExecuteFunc2,
	.executeFunc3 = databaseExecuteFunc3,
	.getRow       = databaseGetRow,
	.freeResult   = databaseFreeResult,
	.queryString  = queryString,
	.timeString   = timeString,
	.destroy      = databaseDestroy
};
