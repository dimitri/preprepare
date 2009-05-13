MODULES = pre_prepare
DATA_built = pre_prepare.sql

PGXS = $(shell pg_config --pgxs)
include $(PGXS)

README.html: README
	asciidoc -a toc README
