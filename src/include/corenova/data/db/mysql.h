#ifndef __mysql_H__
#define __mysql_H__

#include <corenova/interface.h>

#include <mysql/mysql.h>
#include <sys/time.h>
#include <stdarg.h>

extern my_bool my_init(void);

DEFINE_INTERFACE (MySQL) {
	MYSQL     *(*connect)      (char *host, char *user, char *pass, char *dbname,
								uint32_t port, char *socket, uint32_t flags);
	void       (*close)        (MYSQL *);
	int32_t   (*query)        (MYSQL *, const char *sql);
	int32_t   (*execute)      (MYSQL *, const char *sql, MYSQL_RES **);
	int32_t   (*executeFunc)  (MYSQL *, const char *funcname, MYSQL_RES **, va_list);
	int32_t   (*executeFunc2) (MYSQL *, const char *funcname, MYSQL_RES **, char **, int);
	int32_t   (*executeFunc3) (MYSQL *, const char *funcname, MYSQL_RES **, char **, char**, int);
	int32_t   (*executeProc)  (MYSQL *, const char *procname, MYSQL_RES **, va_list);
	MYSQL_RES *(*getResult)    (MYSQL *);
	void       (*freeResult)   (MYSQL_RES *);
	int32_t   (*countFields)  (MYSQL *);
	int32_t   (*numRows)      (MYSQL_RES *);
	MYSQL_ROW  (*getRow)       (MYSQL_RES *);
	char      *(*timestamp)    (struct timeval *);
	char      *(*escapeString) (MYSQL *, const char *string);
	int32_t        (*snprintf)     (MYSQL *, char *, size_t, const char *format,...);
	int32_t        (*vsnprintf)    (MYSQL *, char *, size_t, const char *format, va_list);
};

#endif	/* __mysql_H__ */
