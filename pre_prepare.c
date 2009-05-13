/*
 * pre_prepare allows to store statements in a table and will prepare them
 * all when the prepare_all() function is being called.
 *
 * Unfortunately it's not possible to SPI_connect() when LOAD is done via
 * local_preload_libraries, so the function call can't be made transparent
 * to the client connection.
 */
#include <stdio.h>
#include "postgres.h"

#include "executor/spi.h"
#include "utils/guc.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "utils/builtins.h"
#include "libpq/pqformat.h"

/*
#define  DEBUG
 */

/*
 * This code has only been tested with PostgreSQL 8.3.
 */
#ifdef PG_VERSION_NUM
#define PG_MAJOR_VERSION (PG_VERSION_NUM / 100)
#else
#error "Unknown postgresql version"
#endif

#if PG_MAJOR_VERSION != 803 && PG_MAJOR_VERSION != 804
#error "Unsupported postgresql version"
#endif

PG_MODULE_MAGIC;

static char *pre_prepare_relation = NULL;

void _PG_init(void);
Datum prepare_all(PG_FUNCTION_ARGS);

/*
 * Check that pre_prepare.relation is setup to an existing table.
 *
 * The catalog inquiry could use some optimisation (index use prevented).
 */
static inline
bool check_pre_prepare_relation() {
  int err;

  char *stmpl  = "SELECT 1 FROM pg_class WHERE " \
    "(SELECT nspname from pg_namespace WHERE oid = relnamespace) " \
    "|| '.' || relname = '%s';";

  int len      = (strlen(stmpl) - 2) + strlen(pre_prepare_relation) + 1;
  char *select = (char *)palloc(len * sizeof(char));

  snprintf(select, len, stmpl, pre_prepare_relation);

#ifdef DEBUG
  elog(NOTICE, select);
#endif

  err = SPI_execute(select, true, 1);

  if( err != SPI_OK_SELECT )
    elog(ERROR, "SPI_execute: %s", SPI_result_code_string(err));

  return 1 == SPI_processed;
}

/*
 * Prepare all the statements in pre_prepare.relation
 */
static inline
int pre_prepare_all() {
  int err, nbrows = 0;
  char *stmpl  = "SELECT name, statement FROM %s";
  int len      = (strlen(stmpl) - 2) + strlen(pre_prepare_relation) + 1;
  char *select = (char *)palloc(len);

  snprintf(select, len, stmpl, pre_prepare_relation);
  err = SPI_execute(select, true, 0);

  if( err != SPI_OK_SELECT ) {
    elog(ERROR, "SPI_execute: %s", SPI_result_code_string(err));
    return -1;
  }

  nbrows = SPI_processed;

  if( nbrows > 0 && SPI_tuptable != NULL ) {
    TupleDesc tupdesc = SPI_tuptable->tupdesc;
    SPITupleTable *tuptable = SPI_tuptable;
    int row;
        
    for (row = 0; row < nbrows; row++) {
      HeapTuple tuple = tuptable->vals[row];
      char *name = SPI_getvalue(tuple, tupdesc, 1);
      char *stmt = SPI_getvalue(tuple, tupdesc, 2);

      elog(NOTICE, "Preparing statement name: %s", name);
      
      err = SPI_execute(stmt, false, 0);

      if( err != SPI_OK_UTILITY ) {
	elog(ERROR, "SPI_execute: %s", SPI_result_code_string(err));
	return -1;
      }
    }
  }
  else
    elog(NOTICE, "No statement to prepare found in '%s'", pre_prepare_relation);

  return nbrows;
}

/*
 * _PG_init()			- library load-time initialization
 *
 * DO NOT make this static nor change its name!
 *
 * Init the module, all we have to do here is getting our GUC
 */
void
_PG_init(void) {
  DefineCustomStringVariable("preprepare.relation",
			     "Table name where to find statements to prepare",
			     "Can be schema qualified, must have columns \"name\" and \"statement\"",
			     &pre_prepare_relation,
			     PGC_USERSET,
			     NULL, 
			     NULL);

  EmitWarningsOnPlaceholders("prepare.relation");
}

/*
 * PostgreSQL interface, with the SPI_connect and SPI_finish calls.
 */
PG_FUNCTION_INFO_V1(prepare_all);
Datum
prepare_all(PG_FUNCTION_ARGS)
{
  int err;

  if( pre_prepare_relation == NULL )
    ereport(ERROR,
	    (errcode(ERRCODE_DATA_EXCEPTION),
	     errmsg("The custom variable preprepare.relation is not set."),
	     errhint("Set preprepare.relation to an existing table.")));

  err = SPI_connect();
  if (err != SPI_OK_CONNECT)
    elog(ERROR, "SPI_connect: %s", SPI_result_code_string(err));

  if( ! check_pre_prepare_relation() ) {
    ereport(ERROR,
	    (errcode(ERRCODE_DATA_EXCEPTION),
	     errmsg("Can not find relation '%s'", pre_prepare_relation),
	     errhint("Set preprepare.relation to an existing table.")));
  }

#ifdef DEBUG
  elog(NOTICE, "preprepare.relation is found, proceeding");
#endif

  pre_prepare_all(false);

  /* done with SPI */
  err = SPI_finish();
  if (err != SPI_OK_FINISH)
    elog(ERROR, "SPI_finish: %s", SPI_result_code_string(err));

  PG_RETURN_VOID();
}
