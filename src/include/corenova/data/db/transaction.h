#ifndef __transaction_H__
#define __transaction_H__

#include <corenova/interface.h>

#include <corenova/data/database.h>

/*
  A Databse Transaction Item comprises of the query, and the result for the
  query.
*/
typedef struct {

	char      *query;
	db_result *result;
	
} db_transaction_item_t;

typedef struct {

	I_TYPE (Database) *interface;
	
	
} db_transaction_t;

DEFINE_INTERFACE (DbTransaction) {
	db_transaction_t      *(*new)     (database_t *);
	boolean_t              (*add)     (db_transaction_t *, db_transaction_item *);
	db_transaction_item_t *(*get)     (db_transaction_t *);
	uint32_t           (*count)   (db_transaction_t *);
	boolean_t              (*execute) (db_transaction_t *);
	void                   (*reset)   (db_transaction_t *);
};

DEFINE_INTERFACE (DbTransactionItem) {
	db_transaction_item_t *(*new) (char *query);
};

#endif	/* __transaction_H__ */
