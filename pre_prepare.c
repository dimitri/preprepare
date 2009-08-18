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
#include "access/xact.h"

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

#if PG_MAJOR_VERSION != 803 && PG_MAJOR_VERSION != 804 && PG_MAJOR_VERSION != 805
#error "Unsupported postgresql version"
#endif

PG_MODULE_MAGIC;


/*
 * In 8.3 it seems that snapmgr.h is unavailable for module code
 *
 * That means there's no support for at_init and local_preload_libraries in
 * this version.
 */
#if PG_MAJOR_VERSION > 803
#include "utils/snapmgr.h"
static bool pre_prepare_at_init   = false;
#endif

static char *pre_prepare_relation = NULL;

void _PG_init(void);
Datum prepare_all(PG_FUNCTION_ARGS);

/*
 * Check that pre_prepare.relation is setup to an existing table.
 *
 * The catalog inquiry could use some optimisation (index use prevented).
 */
static inline
bool check_pre_prepare_relation(const char *relation_name) {
  int err;

  char *stmpl  = "SELECT 1 FROM pg_class WHERE " \
    "(SELECT nspname from pg_namespace WHERE oid = relnamespace) " \
    "|| '.' || relname = '%s';";

  int len      = (strlen(stmpl) - 2) + strlen(relation_name) + 1;
  char *select = (char *)palloc(len * sizeof(char));

  snprintf(select, len, stmpl, relation_name);

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
int pre_prepare_all(const char *relation_name) {
  int err, nbrows = 0;
  char *stmpl  = "SELECT name, statement FROM %s";
  int len      = (strlen(stmpl) - 2) + strlen(relation_name) + 1;
  char *select = (char *)palloc(len);

  snprintf(select, len, stmpl, relation_name);
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
  PG_TRY();
  {
#if PG_MAJOR_VERSION == 804
    bool at_init = false;
    
    if( parse_bool(GetConfigOptionByName("prepare.at_init", NULL), &at_init) )
      pre_prepare_at_init = at_init;
#endif

    pre_prepare_relation = GetConfigOptionByName("prepare.relation", NULL);
  }
  PG_CATCH();
  {
    /*
     * From 8.4 the Custom variables take two new options, the default value
     * and a flags field
     */
#if PG_MAJOR_VERSION == 803
    DefineCustomStringVariable("preprepare.relation",
			       "Table name where to find statements to prepare",
			       "Can be schema qualified, must have columns "
			       "\"name\" and \"statement\"",
			       &pre_prepare_relation,
			       PGC_USERSET,
			       NULL, 
			       NULL);

    /*
     * It's missing a way to use PushActiveSnapshot/PopActiveSnapshot from
     * within a module in 8.3 for this to be useful.
     *
     DefineCustomBoolVariable("preprepare.at_init",
			      "Do we prepare the statements at backend start",
			      "You have to setup local_preload_libraries too",
			      &pre_prepare_at_init,
			      PGC_USERSET,
			      NULL,
			      NULL);
    */
    EmitWarningsOnPlaceholders("prepare.relation");

#else
    DefineCustomStringVariable("preprepare.relation",
			       "Table name where to find statements to prepare",
			       "Can be schema qualified, must have columns "
			       "\"name\" and \"statement\"",
			       &pre_prepare_relation,
			       "",
			       PGC_USERSET,
			       GUC_NOT_IN_SAMPLE,
			       NULL, 
			       NULL);

    DefineCustomBoolVariable("preprepare.at_init",
			     "Do we prepare the statements at backend start",
			     "You have to setup local_preload_libraries too",
			     &pre_prepare_at_init,
			     false,
			     PGC_USERSET,
			     GUC_NOT_IN_SAMPLE,
			     NULL,
			     NULL);

    EmitWarningsOnPlaceholders("prepare.relation");
    EmitWarningsOnPlaceholders("prepare.at_init");
#endif
  }
  PG_END_TRY();

#if PG_MAJOR_VERSION == 804
  if( pre_prepare_at_init ) {
    int err;

    /*
     * We want to use SPI, so we need to ensure there's a current started
     * transaction, then take a snapshot
    start_xact_command();
     */
    Snapshot snapshot;

    StartTransactionCommand();
    /* CommandCounterIncrement(); */
    snapshot = GetTransactionSnapshot();
    PushActiveSnapshot(snapshot);

    err = SPI_connect();
    if (err != SPI_OK_CONNECT)
      elog(ERROR, "SPI_connect: %s", SPI_result_code_string(err));
    
    if( ! check_pre_prepare_relation(pre_prepare_relation) ) {
      ereport(ERROR,
	      (errcode(ERRCODE_DATA_EXCEPTION),
	       errmsg("Can not find relation '%s'", pre_prepare_relation),
	       errhint("Set preprepare.relation to an existing table.")));
    }
    
    pre_prepare_all(pre_prepare_relation);
    
    err = SPI_finish();
    if (err != SPI_OK_FINISH)
      elog(ERROR, "SPI_finish: %s", SPI_result_code_string(err));

    PopActiveSnapshot();
    CommitTransactionCommand();
  }
#endif
}

/*
 * PostgreSQL interface, with the SPI_connect and SPI_finish calls.
 */
PG_FUNCTION_INFO_V1(prepare_all);
Datum
prepare_all(PG_FUNCTION_ARGS)
{
  char * relation = NULL;
  int err;

  /*
   * we support for the user to override the GUC by passing in the relation name
   * SELECT prepare_all('some_other_statements');
   */
  if( PG_NARGS() == 1 )
    relation = DatumGetCString(DirectFunctionCall1(textout, 
						   PointerGetDatum(PG_GETARG_TEXT_P(0))));
  else
    {
      relation = pre_prepare_relation;

      /*
       * The function is STRICT so we don't check this error case in the
       * previous branch
       */
      if( relation == NULL )
	ereport(ERROR,
		(errcode(ERRCODE_DATA_EXCEPTION),
		 errmsg("The custom variable preprepare.relation is not set."),
		 errhint("Set preprepare.relation to an existing table.")));
    }

  err = SPI_connect();
  if (err != SPI_OK_CONNECT)
    elog(ERROR, "SPI_connect: %s", SPI_result_code_string(err));

  if( ! check_pre_prepare_relation(relation) )
    {
      char *hint = "Set preprepare.relation to an existing table, schema qualified";
      if( PG_NARGS() == 1 )
	hint = "prepare_all requires you to schema qualify the relation name";
	
      ereport(ERROR,
	      (errcode(ERRCODE_DATA_EXCEPTION),
	       errmsg("Can not find relation '%s'", relation),
	       errhint(hint)));
    }

#ifdef DEBUG
  elog(NOTICE, "preprepare.relation is found, proceeding");
#endif

  pre_prepare_all(relation);

  /* done with SPI */
  err = SPI_finish();
  if (err != SPI_OK_FINISH)
    elog(ERROR, "SPI_finish: %s", SPI_result_code_string(err));

  PG_RETURN_VOID();
}
