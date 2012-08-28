#ifndef __database_H__
#define __database_H__

#include <corenova/interface.h>

#include <stdarg.h>				/* for va_list va_arg, etc. */

#define DB_QUERY_MAXLEN 8192

const char PrintfModifiers[] = {
	'#','0','-',' ','+','\'','I',
	'1','2','3','4','5','6','7','8','9','*','$',
	'.',
	'h','l','L','q','j','z','t',
	'\0'
};

const char PrintfConversion[] = {
	'd','i',
	'o','u','x','X',
	'e','E','f','F','g','G','a','A',
	'c','s','C','S',
	'p','n','%',
	'\0'
};

const enum db_types {

	TYPE_SMALLINT,
	TYPE_INTEGER,
	TYPE_BIGINT,
	TYPE_VARCHAR,
	TYPE_CHAR,
	TYPE_INET,
	TYPE_MACADDR,

} __db_types;

/*
  Generic database result holder
*/
typedef struct {

	void    *set;
	int32_t value;
	
} db_result_t;

typedef char ** db_row_t;

typedef struct {

	char *name;					/* just for reference */
	void *connection;			/* i.e. holds MYSQL, PGSQL, etc. * */
	char queryBuffer[DB_QUERY_MAXLEN];

	/* internal use only! */
	module_t *module;
	boolean_t passThrough;
	boolean_t inPool;
	uint32_t poolAccessors;

} database_t;

typedef struct {

	char *module;
	char *host;
	char *user;
	char *pass;
	char *dbname;
	int32_t port;
	
} db_parameters_t;

DEFINE_INTERFACE (Database) {
	database_t  *(*connect)           (db_parameters_t *);
	db_result_t *(*execute)           (database_t *, const char *query);
	db_result_t *(*executeProc)       (database_t *, const char *procname,...);
	db_result_t *(*executeFunc)       (database_t *, const char *funcname,...);
	db_result_t *(*executeFunc2)      (database_t *, const char *funcname,char **, int);	
	db_result_t *(*executeFunc3)      (database_t *, const char *funcname,char **, char **, int);	
	db_row_t     (*getRow)            (database_t *, db_result_t *);
	void         (*freeResult)        (database_t *, db_result_t *);
	char        *(*queryString)       (database_t *, char *format, ...) __attribute__ ((format (printf,2,3)));
	char        *(*timeString)        (database_t *, struct timeval *tv);
	void         (*destroy)           (database_t **);
	db_result_t *(*prepare)           (database_t *, const char *stmtName, const char *query, int nParams, const enum db_types *paramTypes);
	db_result_t *(*executePrepared)   (database_t *, const char *stmtName, int nParams, const char * const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat);
    boolean_t    (*isPrepared)        (database_t *, const char *stmtName);
};

#include <corenova/data/list.h>

typedef list_t database_pool_t;

DEFINE_INTERFACE (DatabasePool) {
	database_pool_t *(*new)     (void);
	database_t      *(*connect) (database_pool_t *, db_parameters_t *);
	uint32_t         (*clean)   (database_pool_t *);
	void             (*destroy) (database_pool_t **);
};

typedef struct {

	database_t  *database;
	char  *query;
	db_result_t *result;
	
} db_transaction_t;

DEFINE_INTERFACE (DatabaseTransaction) {
	db_transaction_t *(*new)      (database_t *, const char *query);
	boolean_t         (*execute)  (db_transaction_t *);
	void              (*destroy)  (db_transaction_t **);
};

typedef list_t db_transactions_t;

DEFINE_INTERFACE (DatabaseTransactionList) {
	db_transactions_t *(*new)     (void);
	boolean_t          (*add)     (db_transactions_t *, db_transaction_t *);
	boolean_t          (*execute) (db_transactions_t *);
	void               (*destroy) (db_transactions_t **);
};

#endif	/* __database_H__ */
