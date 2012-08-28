#include <corenova/source-stub.h>

THIS = {
	.version     = "2.0",
	.author      = "Peter K. Lee <saint@corenova.com>",
	.description = "Provides API to PostgreSQL Database.",
	.implements  = LIST ("PgSQL","Database"),
	.requires    = LIST ("corenova.data.string", "corenova.data.array")
};

#include <corenova/data/db/pgsql.h>
#include <corenova/data/string.h>
#include <corenova/data/database.h>

#define SMALLINTOID	21
#define INTEGEROID	23
#define BIGINTOID	20
#define VARCHAROID	25
#define CHAROID		25
#define INETOID		869
#define MACADDROID	829
#define TEXTOID		25

static Oid PG_TYPES[] = {

    SMALLINTOID,
    INTEGEROID,
    BIGINTOID,
    VARCHAROID,
    CHAROID,
    INETOID,
    MACADDROID,
    TEXTOID

};

/*//////// MODULE CODE //////////////////////////////////////////*/

void CONSTRUCTOR __pgsql_init__ () {}

static PGSQL *
_connect (char *host, char *user, char *pass, char *dbname, uint32_t port, char *socket, uint32_t flags) {
		  
	PGSQL *conn = calloc(1, sizeof(PGSQL));
    if (conn) {
        conn->conn = PQsetdbLogin (host, NULL, NULL, NULL, dbname, user, pass);
        if(conn->conn) {
            if(PQstatus(conn->conn) != CONNECTION_OK) {
                DEBUGP (DERR, "_connect", "Connection failed : %s", PQerrorMessage(conn->conn));
		
                I (PgSQL)->close(conn);
                conn = NULL;
            } else {
                MUTEX_SETUP (conn->lock);
                conn->prepared = I (Array)->new ();
            }
        }
    }
	return (conn);
}

static void
_close (PGSQL *conn) {
	if (conn && conn->conn) {
	    PQfinish (conn->conn);
        MUTEX_LOCK (conn->lock); /* DB specific lock */
        I (Array)->destroy (&conn->prepared,NULL);
        MUTEX_UNLOCK (conn->lock);
        MUTEX_CLEANUP (conn->lock);
	    free(conn);
	}
}

static int32_t
_numRows (PGSQL_RES *result) {

	if (result && result->res) {
	
	    switch(PQresultStatus(result->res)) {
	    
          case PGRES_COMMAND_OK:  result->numRows = (strlen(PQcmdTuples ((result->res))) == 0) ? 0 : 1;
              break;
          case PGRES_TUPLES_OK : 	result->numRows = PQntuples (result->res);
              break;
          default  :  return -1;
	    
	    }
		
	    return result->numRows;
	    
	}
	
    return -1;

}

static int32_t
_query (PGSQL *conn, const char *sql) {

    if(conn && sql) {
	    PGSQL_RES *result = calloc(1, sizeof(PGSQL_RES));
        if (result) {
            uint32_t retryCount = 0;
            
            MUTEX_LOCK (conn->lock); /* DB specific lock */
        
            for(retryCount = 0; retryCount < MAX_RETRIES; retryCount++) {
                
                if ((result->res = PQexec(conn->conn, sql))) {
                    conn->res = result;
                    break;
                }

                DEBUGP (DERR,"query","PostgreSQL query %s failed : %s",sql, PQerrorMessage(conn->conn));
                DEBUGP (DWARN,"query", "resetting connection...");
                PQreset(conn->conn);

                /* INVALIDATE all previous prepared statements! */
                I (Array)->destroy (&conn->prepared,NULL);
                conn->prepared = I (Array)->new ();
            }

            if (result->res) {
                
                if (PQresultStatus(result->res) == PGRES_TUPLES_OK ||
                    PQresultStatus(result->res) == PGRES_COMMAND_OK) {
                    int32_t numRows = 0;
                    
                    // initialize column count
                    result->numFields = PQnfields(result->res);
	    
                    // initialize row count
                    I (PgSQL)->numRows(result);
                    result->row = calloc(result->numFields,sizeof(char*)); // will store column data here
                    numRows = I (PgSQL)->numRows (result);

                    MUTEX_UNLOCK (conn->lock);
                    return numRows;
	       
                } else {
                    DEBUGP (DERR, "query","PostgreSQL query failed: %s", PQerrorMessage(conn->conn));
                }
		
                PQclear(result->res);
                conn->res = NULL;
            } else {
                DEBUGP (DERR,"query","PostgreSQL query %s failed after %d attempts : %s",sql, retryCount, PQerrorMessage(conn->conn));
            }
            MUTEX_UNLOCK (conn->lock);
            free(result);
        }
	}
    return -1;
}

/*
  Either returns number of rows retrieved, or number of rows affected (if
  SELECT)
*/
static int32_t
_execute (PGSQL *conn, const char *query, PGSQL_RES **result) {
    int32_t retVal = 0;

    if (conn && query) {
        MODULE_LOCK (); /* ENTIRE MODULE lock since it chains multiple connection-specific calls */
        int32_t affectedRows = I (PgSQL)->query (conn,query);
        PGSQL_RES *res = (affectedRows != -1)?I (PgSQL)->getResult (conn):NULL;
	
        if (res) {
            *result = res;
            retVal = I (PgSQL)->numRows (res);
        } else {
            if (I (PgSQL)->countFields (conn) == 0)
                retVal = affectedRows;
            else {
                DEBUGP (DERR,"execute","unable to return result set!");
                retVal = -1;
            }
        }
        MODULE_UNLOCK ();
    }
    return retVal;
}

static int32_t
_executeFunc (PGSQL *conn, const char *funcname, PGSQL_RES **result, va_list ap) {

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
	    
	    	l+= sprintf(stmtBuffer+l ,"NULL,");
            DEBUGP(DINFO, "_executeFunc", "function parameter is too long");

            continue;
		
	    }
		
	    char *eString = I (PgSQL)->escapeString(conn, param);
	
	    l+= sprintf(stmtBuffer+l, "'%s',", eString);
	    
	    free(eString);
	
	}
	
	stmtBuffer[l-1] = ')';
	stmtBuffer[l] = '\0';
	
	// PostgreSQL doesn't have any specific routines for dealing with
	// stored procedures, just do a query.
	
	return I (PgSQL)->execute(conn, stmtBuffer, result);


}

static int32_t
_executeFunc2 (PGSQL *conn, const char *funcname, PGSQL_RES **result, char **argv, int32_t argc) {

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
            DEBUGP(DINFO, "_executeFunc2", "function parameter is too long");

            continue;
		
	    }
		
	    char *eString = I (PgSQL)->escapeString(conn, argv[i]);
	
	    l+= sprintf(stmtBuffer+l, "'%s',", eString);
	    
	    free(eString);
	
	}
	
	stmtBuffer[l-1] = ')';
	stmtBuffer[l] = '\0';
	
	// PostgreSQL doesn't have any specific routines for dealing with
	// stored procedures, just do a query.
	
	return I (PgSQL)->execute(conn, stmtBuffer, result);


}

static int32_t
_executeFunc3 (PGSQL *conn, const char *funcname, PGSQL_RES **result, char **argv, char **type, int32_t argc) {

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
            DEBUGP(DINFO, "_executeFunc3", "function parameter is too long");

            continue;
		
	    }
	    
	    char *eString = I (PgSQL)->escapeString(conn, argv[i]);
	    
	    l+= sprintf(stmtBuffer+l, "'%s'::%s,", eString, type[i]);
	    
	    free(eString);
	
	}
	
	stmtBuffer[l-1] = ')';
	stmtBuffer[l] = '\0';
	
	// PostgreSQL doesn't have any specific routines for dealing with
	// stored procedures, just do a query.
	
	return I (PgSQL)->execute(conn, stmtBuffer, result);


}

static int __prepared_stmt_cmp (void *first, void *second) {
    if (first && second) {
        return I (String)->equal ((char *)first, (char *)second);
    }
    return 0;
}

static int32_t
_prepare(PGSQL *conn, const char *stmtName, const char *query, int nParams, const enum db_types *paramTypes) {

    if (conn && conn->conn && stmtName && query) {
        
        PGSQL_RES *result = calloc(1, sizeof(PGSQL_RES));
    
        if (result) {
            int retryCount, i;
            Oid *types = NULL;
            
            if(nParams)
                types = malloc(nParams*sizeof(Oid));
    
            // convert corenova db types to PG types
            for(i = 0; i < nParams; i++) {
                types[i] = PG_TYPES[paramTypes[i]];
            }

            MUTEX_LOCK (conn->lock); /* DB specific lock */
	
            for(retryCount = 0; retryCount < MAX_RETRIES; retryCount++) {
	    
                result->res = PQprepare(conn->conn, stmtName, query, nParams, paramTypes);
                conn->res = result;
		
                // call failed due to broken connection
		
                if(!result->res && PQstatus(conn->conn) != CONNECTION_OK) {
                
            	    DEBUGP (DERR,"prepare","PostgreSQL failed to prepare %s as %s : %s", stmtName, query, PQerrorMessage(conn->conn));
            	    DEBUGP (DWARN,"prepare", "resetting connection...");
            	    PQreset(conn->conn);
		    
            	    /* INVALIDATE all previous prepared statements! */
            	    I (Array)->destroy (&conn->prepared,NULL);
            	    conn->prepared = I (Array)->new ();
		    
                    // this case is not fatal, retry prepare
                    continue;
		    
                } else 
		
                    if(result->res && PQresultStatus(result->res) != PGRES_COMMAND_OK) {
		
                        if(strcmp(PQresultErrorField(result->res, PG_DIAG_SQLSTATE), "42P05") == 0) {
		    
                            // shouldn't happen with client-side statement tracking, but just in case
		    
                            DEBUGP(DWARN, "prepare", "statement '%s' is already prepared", stmtName);
                            break;
			
                        }
		    
                        DEBUGP(DDEBUG, "_prepare", "sqlstate: %s", PQresultErrorField(result->res, PG_DIAG_SQLSTATE));
                        DEBUGP(DDEBUG, "_prepare", "result status: %s", PQresStatus(PQresultStatus(result->res)));
                        DEBUGP(DDEBUG, "_prepare", "error message: %s", PQresultErrorMessage(result->res));
                        break;
								    
                    } else {
		
                        break;
		
                    }

            }
	    
            if (result->res) {
                
                if (PQresultStatus(result->res) == PGRES_COMMAND_OK) {
            
                    if (!I (Array)->match (conn->prepared,(void *)stmtName,__prepared_stmt_cmp)) {
                        I (Array)->add (conn->prepared, (void *)stmtName);
                    }

                    MUTEX_UNLOCK (conn->lock);
                    return 1;
                } else {
                    DEBUGP (DERR, "prepare","PostgreSQL prepare failed: %s", PQerrorMessage(conn->conn));
                }

                PQclear(result->res);
                conn->res = NULL;
            } else {
                DEBUGP (DERR,"prepare","PostgreSQL prepare %s failed after %d attempts : %s", stmtName, retryCount, PQerrorMessage(conn->conn));
            }
            
            MUTEX_UNLOCK (conn->lock);
            free (result);
        }
    }
    return -1;
}

static int32_t
_executePrepared(PGSQL *conn, const char *stmtName, int nParams, const char * const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) {
    if(conn && conn->conn && stmtName) {

        MUTEX_LOCK (conn->lock);
        if (!I (Array)->match (conn->prepared,(void *)stmtName,__prepared_stmt_cmp)) {
            DEBUGP (DERR,"databaseExecutePrepared","requested statment '%s' has not been prepared yet!",stmtName);
            MUTEX_UNLOCK (conn->lock);
            return -2;
        }
        MUTEX_UNLOCK (conn->lock);

        PGSQL_RES *result = calloc(1, sizeof(PGSQL_RES));
        if (result) {
            int retryCount;
            
            MUTEX_LOCK (conn->lock);
            for(retryCount = 0; retryCount < MAX_RETRIES; retryCount++) {
                
                result->res = PQexecPrepared(conn->conn, stmtName, nParams, paramValues, paramLengths, paramFormats, resultFormat);
                conn->res = result;
		
                // call failed because connection is broken
            
                if(!result->res && PQstatus(conn->conn) != CONNECTION_OK) {
	    
            	    DEBUGP (DERR,"executePrepared","PostgreSQL failed to execute %s : %s", stmtName, PQerrorMessage(conn->conn));
            	    DEBUGP (DWARN,"executePrepared", "resetting connection...");
            	    PQreset(conn->conn);

            	    /* INVALIDATE all previous prepared statements! */
            	    I (Array)->destroy (&conn->prepared,NULL);
            	    conn->prepared = I (Array)->new ();
		    
                    // this case is fatal because statement text is not available in this function
                    break;
		    
                } else
		
                    // call succeeded, but query failed
		
                    if(result->res && PQresultStatus(result->res) != PGRES_COMMAND_OK && PQresultStatus(result->res) != PGRES_TUPLES_OK) {
                        char *errmsg = PQresultErrorField(result->res, PG_DIAG_SQLSTATE);
                        if(errmsg && strncmp(errmsg, "26000", 5) == 0) {
		    
                            // shouldn't happen with client-side statement tracking
		    
                            DEBUGP(DWARN, "executePrepared", "statement '%s' does not exist", stmtName);
                            break;
			
                        }

                        if (errmsg) {
                            DEBUGP(DDEBUG, "_executePrepared", "sqlstate: %s", errmsg);
                            DEBUGP(DDEBUG, "_executePrepared", "result status: %s", PQresStatus(PQresultStatus(result->res)));
                            DEBUGP(DDEBUG, "_executePrepared", "error message: %s", PQresultErrorMessage(result->res));
                        } else {
                            DEBUGP (DWARN,"_executePrepared","unable to extract message from result set!");
                        }
                        break;
		    
                    } else {
		
                        break;
		
                    }
                
            }

            if (result->res) {

                if (PQresultStatus(result->res) == PGRES_TUPLES_OK ||
                    PQresultStatus(result->res) == PGRES_COMMAND_OK) {
                    int32_t numRows = 0;

                    // initialize column count
                    result->numFields = PQnfields(result->res);
	
                    // initialize row count
                    I (PgSQL)->numRows(result);
                    result->row = calloc(result->numFields,sizeof(char*)); // will store column data here
                    numRows = I (PgSQL)->numRows (result);

                    MUTEX_UNLOCK (conn->lock);
                    return numRows;
                    
                } else {
                    DEBUGP (DERR, "executePrepared","PostgreSQL failed: %s", PQerrorMessage(conn->conn));
                }

                PQclear(result->res);
                conn->res = NULL;
            } else {
                DEBUGP (DERR,"executePrepared","PostgreSQL %s failed after %d attempts : %s", stmtName, retryCount, PQerrorMessage(conn->conn));
            }
            MUTEX_UNLOCK (conn->lock);
            free(result);
        }
    }
    return -1;
}

static boolean_t
_isPrepared (PGSQL *conn, const char *stmtName) {
    if (conn && stmtName) {

        if (I (Array)->match (conn->prepared, (void *)stmtName, __prepared_stmt_cmp)) {
            return TRUE;
        }
        
    }
    return FALSE;
}

static int32_t
_executeProc (PGSQL *conn, const char *procname, PGSQL_RES **result, va_list ap) {

    return -1;

}

static PGSQL_RES *
_getResult (PGSQL *conn) {

	if(conn)
	    return conn->res;
	    
	return NULL;
	    
}

static void
_freeResult (PGSQL_RES *result) {

	if (result && result->res) {
	
	    PQclear ((PGresult *)result->res);
	    
	    if(result->row)
            free(result->row);
		
	    free(result);

	}

}

static int32_t
_countFields (PGSQL *conn) {
    
	if (conn && conn->res && conn->res->res) {
	
	    if(!conn->res->numFields)
            conn->res->numFields = PQnfields (conn->res->res);
		
	    return conn->res->numFields;
	    
	}
	
    return -1;

}


static PGSQL_ROW
_getRow (PGSQL_RES *result) {

	if(result && result->res && I (PgSQL)->numRows(result) > result->currRow-1) {
	
	    int32_t c;
	    
	    for(c = 0; c < result->numFields; c++)
            result->row[c] = PQgetvalue(result->res, result->currRow, c);
	
	    result->currRow++;
	    
	    return result->row;
	
	}
	
    return NULL;
	
}

#define TIMESTAMP_BUFFER_LEN 32
static char TimestampBuffer[TIMESTAMP_BUFFER_LEN];

static char *
_timestamp (struct timeval *tv) {
	char *tstamp = NULL;
	MODULE_LOCK ();
	if (TIMESTAMP_BUFFER_LEN >
		strftime(TimestampBuffer,TIMESTAMP_BUFFER_LEN,"%Y-%m-%d %H:%M:%S", gmtime((time_t*)&tv->tv_sec))) {
		tstamp = strdup (TimestampBuffer);
	}
	MODULE_UNLOCK ();
	return tstamp;
}

static char *
_escapeString (PGSQL *conn, const char *string) {
	char *eString = NULL;
	if (conn) {
		if (string) {
			uint32_t len = strlen (string);
			eString = (char *)calloc ((len*2)+1,sizeof (char));
			if (eString) {
				PQescapeStringConn (conn->conn, eString, string, len, NULL);
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
_snprintf (PGSQL *conn, char *str, size_t size, const char *format,...) {
	va_list ap;
	va_start (ap,format);
	return I (PgSQL)->vsnprintf (conn,str,size,format,ap);
}

#define FORMAT_STRING_MAXLEN 32

static int32_t
_vsnprintf (PGSQL *conn, char *str, size_t size, const char *format, va_list ap) {
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
                              eString = I (PgSQL)->escapeString (conn,string);
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

IMPLEMENT_INTERFACE (PgSQL) = {
	.connect      = _connect,
	.close        = _close,
	.query        = _query,
	.execute      = _execute,
	.executeFunc  = _executeFunc,
	.executeFunc2 = _executeFunc2,
	.executeFunc3 = _executeFunc3,
	.executeProc  = _executeProc,
	.getResult    = _getResult,
	.freeResult   = _freeResult,
	.countFields  = _countFields,
	.numRows      = _numRows,
	.getRow       = _getRow,
	.timestamp    = _timestamp,
	.escapeString = _escapeString,
	.snprintf     = _snprintf,
	.vsnprintf    = _vsnprintf,
	.prepare      = _prepare,
	.executePrepared = _executePrepared,
    .isPrepared   = _isPrepared
};


/***** Database Interface Implementation *****/

static database_t *
databaseConnect (db_parameters_t *params) {
	database_t *db = (database_t *) calloc (1,sizeof (database_t));
	if (db) {
		db->connection = I (PgSQL)->connect (params->host,params->user,params->pass,params->dbname,
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
			I (PgSQL)->close (db->connection);
			free (db);
			*dbPtr = NULL;
		}
	}
}

static db_result_t *
databaseExecute (database_t *db, const char *query) {
	db_result_t *result = (db_result_t *)calloc (1,sizeof (db_result_t));
	if (result) {
		PGSQL *connection = (PGSQL *)db->connection;
		PGSQL_RES *myResult = NULL;
		result->value = I (PgSQL)->execute (connection,query,&myResult);
		if (result->value != -1) {
			result->set = (void *)myResult;
            return result;
		} else {
			DEBUGP (DERR,"databaseExecute","unable to retrieve result!");
			I (PgSQL)->freeResult(myResult);
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
	    PGSQL *connection = (PGSQL *)db->connection;
	    PGSQL_RES *myResult = NULL;
	    ap = va_arg (ap,va_list);
	    result->value = I (PgSQL)->executeFunc (connection, funcname, &myResult, ap);
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
	    PGSQL *connection = (PGSQL *)db->connection;
	    PGSQL_RES *myResult = NULL;
	    result->value = I (PgSQL)->executeFunc2 (connection, funcname, &myResult, argv, argc);
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
databaseExecuteFunc3 (database_t *db, const char *funcname, char **argv, char **type, int32_t argc) {

	db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));
	if(result) {
	    PGSQL *connection = (PGSQL *)db->connection;
	    PGSQL_RES *myResult = NULL;
	    result->value = I (PgSQL)->executeFunc3 (connection, funcname, &myResult, argv, type, argc);
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
databasePrepare (database_t *db, const char *stmtName, const char *query, int nParams, const enum db_types *paramTypes) {

    if (db && stmtName && query) {
        db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));

        if(result) {

            PGSQL *connection = (PGSQL *)db->connection;

            PGSQL_RES *myResult = NULL;

            result->value = I (PgSQL)->prepare (connection, stmtName, query, nParams, (Oid *)paramTypes);
	
            if(result->value != -1) {
                myResult = I (PgSQL)->getResult(connection);
                result->set = (void *)myResult;
                return result;
		    
            } else {
	    
                DEBUGP (DERR, "databasePrepare", "unable to retrieve result!");
                free(result);
		
            }
	    
        }
    }
    return NULL;

}

static db_result_t *
databaseExecutePrepared (database_t *db, const char *stmtName, int nParams, const char * const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) {

    if (db && stmtName) {
        db_result_t *result = (db_result_t *)calloc (1, sizeof (db_result_t));
	
        if(result) {
	
            PGSQL *connection = (PGSQL *)db->connection;
            PGSQL_RES *myResult = NULL;
	    
            result->value = I (PgSQL)->executePrepared(connection, stmtName, nParams, paramValues, paramLengths, paramFormats, resultFormat);
	    
            if(result->value >= 0) {
	    
                myResult = I (PgSQL)->getResult(connection);
                result->set = (void *)myResult;
                return result;
		    
            } else {
	    
                DEBUGP (DERR, "databaseExecutePrepared", "unable to retrieve result!");
                free(result);
		
            }
        }
    }
	return NULL;
}

static boolean_t
databaseIsPrepared (database_t *db, const char *stmtName) {
    if (db && stmtName) {
        if (db->connection) {
            return I (PgSQL)->isPrepared ((PGSQL *) db->connection, stmtName);
        }
    }
    
    return FALSE;
    
}

/*
  PostgreSQL doesn't have stored procedures, so we should just
  return NULL in this one. They should check database capabilities
  before doing those calls anyway.
    
*/

static db_result_t *
databaseExecuteProc (database_t *db, const char *funcname, ...) {

    return NULL;

}

static db_row_t 
databaseGetRow (database_t *db, db_result_t *result) {
	if (db) {
		PGSQL_RES *myResult = (PGSQL_RES *)result->set;
		return (db_row_t) I (PgSQL)->getRow (myResult);
	}
	return NULL;
}

static void
databaseFreeResult (database_t *db, db_result_t *result) {
	if (result) {
		PGSQL_RES *myResult = (PGSQL_RES *)result->set;
		I (PgSQL)->freeResult (myResult);
		free (result);
	}
}

static char *
queryString (database_t *db, char *format, ...) {
	if (db) {
		PGSQL *conn = (PGSQL *)db->connection;
		if (conn) {
			char *query = db->queryBuffer;
			va_list ap;
			va_start (ap,format);
			if (db->passThrough) { /* called via abstraction layer */
				ap = va_arg (ap,va_list); /* I can't believe this works! */
				db->passThrough = FALSE;
			}
			if (I (PgSQL)->vsnprintf (conn,query,DB_QUERY_MAXLEN,format,ap) != -1){
				return strdup (query);
			}
		}
	}
	return NULL;
}

static char *
timeString (database_t *db, struct timeval *tv) {
	return I (PgSQL)->timestamp (tv);
}

IMPLEMENT_INTERFACE (Database) = {
	.connect      = databaseConnect,
	.execute      = databaseExecute,
	.executeFunc  = databaseExecuteFunc,
	.executeFunc2 = databaseExecuteFunc2,
	.executeFunc3 = databaseExecuteFunc3,
	.executeProc  = databaseExecuteProc,
	.getRow       = databaseGetRow,
	.freeResult   = databaseFreeResult,
	.queryString  = queryString,
	.timeString   = timeString,
	.destroy      = databaseDestroy,
	.prepare      = databasePrepare,
	.executePrepared = databaseExecutePrepared,
    .isPrepared   = databaseIsPrepared
};
