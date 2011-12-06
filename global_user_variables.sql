DROP FUNCTION IF EXISTS global_set;
CREATE FUNCTION global_set RETURNS INTEGER
  SONAME 'udf_global_user_variables.so';

DROP FUNCTION IF EXISTS global_store;
CREATE FUNCTION global_store RETURNS INTEGER
  SONAME 'udf_global_user_variables.so';

DROP FUNCTION IF EXISTS global_get;
CREATE FUNCTION global_get RETURNS STRING
  SONAME 'udf_global_user_variables.so';

DROP FUNCTION IF EXISTS global_add;
CREATE FUNCTION global_add RETURNS INTEGER
  SONAME 'udf_global_user_variables.so';

DROP FUNCTION IF EXISTS global_addp;
CREATE FUNCTION global_addp RETURNS INTEGER
  SONAME 'udf_global_user_variables.so';
