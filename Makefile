MODULES = pre_prepare
DATA_built = pre_prepare.sql

PG_CONFIG = pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SRC      = .
BUILDDIR = /tmp/preprepare
ORIG     = preprepare.orig
DEBIAN   = debian
PACKAGE  = postgresql-8.[34]-preprepare
SOURCE   = preprepare

README.html: README
	asciidoc -a toc README

unsign-deb: prepare
	cd $(BUILDDIR)/$(SOURCE) && debuild -us -uc
	cp $(BUILDDIR)/$(PACKAGE)_* ..
	cp $(BUILDDIR)/$(SOURCE)_* ..

deb: prepare
	cd $(BUILDDIR)/$(SOURCE) && debuild
	cp $(BUILDDIR)/$(PACKAGE)_* ..
	cp $(BUILDDIR)/$(SOURCE)_* ..

prepare:
	-test -d $(BUILDDIR) && rm -rf $(BUILDDIR)
	mkdir -p $(BUILDDIR)/$(SOURCE)
	rsync -Ca --exclude $(DEBIAN) $(SRC)/* $(BUILDDIR)/$(SOURCE)
	rsync -Ca $(BUILDDIR)/$(SOURCE)/ $(BUILDDIR)/$(ORIG)/
	rsync -Ca $(DEBIAN) $(BUILDDIR)/$(SOURCE)


