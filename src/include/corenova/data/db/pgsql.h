#ifndef __pgsql_H__
#define __pgsql_H__


#include <corenova/interface.h>
#include <corenova/data/database.h>

#include <libpq-fe.h>
#include <sys/time.h>
#include <stdarg.h>

#define MAX_RETRIES 3

typedef char** PGSQL_ROW;
typedef struct _PGSQL_RES {

    PGresult *res;
    PGSQL_ROW row;
    int32_t currRow;
    int32_t numFields;
    int32_t numRows;

} PGSQL_RES;

#include <corenova/data/array.h>

typedef struct _PGSQL {

    PGconn *conn;
    PGSQL_RES *res;
    array_t *prepared;

    MUTEX_TYPE lock;
    
} PGSQL;


DEFINE_INTERFACE (PgSQL) {
	PGSQL            *(*connect)         (char *host, char *user, char *pass, char *dbname, uint32_t port, char *socket, uint32_t flags);
	void              (*close)           (PGSQL *);
	int32_t           (*query)           (PGSQL *, const char *sql);
	int32_t           (*execute)         (PGSQL *, const char *sql, PGSQL_RES **);
	int32_t           (*executeFunc)     (PGSQL *, const char *funcname, PGSQL_RES **, va_list);
	int32_t           (*executeFunc2)    (PGSQL *, const char *funcname, PGSQL_RES **, char **, int);
	int32_t           (*executeFunc3)    (PGSQL *, const char *funcname, PGSQL_RES **, char **, char **, int);
	int32_t           (*executeProc)     (PGSQL *, const char *procname, PGSQL_RES **, va_list);	
	PGSQL_RES        *(*getResult)       (PGSQL *);
	void              (*freeResult)      (PGSQL_RES *);
	int32_t           (*countFields)     (PGSQL *);
	int32_t           (*numRows)         (PGSQL_RES *);
	PGSQL_ROW         (*getRow)          (PGSQL_RES *);
	char             *(*timestamp)       (struct timeval *);
	char             *(*escapeString)    (PGSQL *, const char *string);
	int32_t           (*snprintf)        (PGSQL *, char *, size_t, const char *format,...);
	int32_t           (*vsnprintf)       (PGSQL *, char *, size_t, const char *format, va_list);
	int32_t           (*prepare)         (PGSQL *conn, const char *stmtName, const char *query, int nParams, const enum db_types *paramTypes);
	int32_t           (*executePrepared) (PGSQL *conn, const char *stmtName, int nParams, const char * const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat);
    boolean_t         (*isPrepared)      (PGSQL *conn, const char *stmtName);
};

#endif	/* __pgsql_H__ */
