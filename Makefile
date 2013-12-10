EXTENSION = pre_prepare
MODULES = pre_prepare
DATA = pre_prepare--0.4.sql pre_prepare--unpackaged--0.4.sql
DATA_built = pre_prepare--0.4.sql
# use extenstion in 9.1+
SETUPSQL = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0" && echo create_module || echo create_extension)
REGRESS = $(SETUPSQL) pre_prepare

PG_CONFIG = pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SRC      = .
TARGET   = ./build
BUILDDIR = /tmp/preprepare
ORIG     = preprepare.orig
DEBIAN   = debian
PACKAGE  = postgresql-?.?-preprepare
SOURCE   = preprepare

README.html: README.asciidoc
	asciidoc -a toc README.asciidoc

pre_prepare--0.4.sql: pre_prepare.sql
	cp $< $@

unsign-deb: prepare
	cd $(BUILDDIR)/$(SOURCE) && debuild -us -uc
	cp $(BUILDDIR)/$(PACKAGE)_* $(TARGET)
	cp $(BUILDDIR)/$(SOURCE)_*  $(TARGET)

deb: prepare
	cd $(BUILDDIR)/$(SOURCE) && debuild
	cp $(BUILDDIR)/$(PACKAGE)_* $(TARGET)
	cp $(BUILDDIR)/$(SOURCE)_*  $(TARGET)

prepare:
	-test -d $(BUILDDIR) && rm -rf $(BUILDDIR)
	mkdir -p $(BUILDDIR)/$(SOURCE)
	rsync -Ca --exclude $(DEBIAN) $(SRC)/* $(BUILDDIR)/$(SOURCE)
	rsync -Ca $(BUILDDIR)/$(SOURCE)/ $(BUILDDIR)/$(ORIG)/
	rsync -Ca $(DEBIAN) $(BUILDDIR)/$(SOURCE)


