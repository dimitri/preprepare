--- a/Makefile
+++ b/Makefile
@@ -1,6 +1,6 @@
 EXTENSION = pre_prepare
 MODULES = pre_prepare
-DATA = pre_prepare--0.4.sql pre_prepare--unpackaged--0.4.sql
+DATA = pre_prepare--unpackaged--0.4.sql
 DATA_built = pre_prepare--0.4.sql
 # use extenstion in 9.1+
 SETUPSQL = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0" && echo create_module || echo create_extension)
