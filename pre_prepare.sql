---
--- pre_prepare exports only a prepare_all() function.
---
CREATE OR REPLACE FUNCTION prepare_all()
RETURNS void
AS '$libdir/pre_prepare', 'prepare_all'
LANGUAGE 'C' STRICT VOLATILE;

CREATE OR REPLACE FUNCTION prepare_all(text)
RETURNS void
AS '$libdir/pre_prepare', 'prepare_all'
LANGUAGE 'C' STRICT VOLATILE;

CREATE OR REPLACE FUNCTION discard()
RETURNS void
LANGUAGE SQL
AS $$
  SET SESSION AUTHORIZATION DEFAULT;
  RESET ALL;
  CLOSE ALL;
  UNLISTEN *;
  SELECT pg_advisory_unlock_all();
  DISCARD PLANS;
  DISCARD TEMP;
$$;
