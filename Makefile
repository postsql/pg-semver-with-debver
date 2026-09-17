MAIN_EXTENSION = $(shell grep -m 1 '"name":' META.json | \
                  sed -e 's/[[:space:]]*"name":[[:space:]]*"\([^"]*\)",/\1/')
EXTENSION    = $(MAIN_EXTENSION) debversion
EXTVERSION   = $(shell grep -m 1 '[[:space:]]\{8\}"version":' META.json | \
               sed -e 's/[[:space:]]*"version":[[:space:]]*"\([^"]*\)",\{0,1\}/\1/')
DISTVERSION  = $(shell grep -m 1 '[[:space:]]\{3\}"version":' META.json | \
               sed -e 's/[[:space:]]*"version":[[:space:]]*"\([^"]*\)",\{0,1\}/\1/')

DATA_built   = sql/$(MAIN_EXTENSION)--$(EXTVERSION).sql sql/debversion--$(EXTVERSION).sql
DATA         = $(filter-out $(DATA_built) sql/$(MAIN_EXTENSION).sql, $(wildcard sql/*.sql))
DOCS         = $(wildcard doc/*.md)
TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test
MODULE_big   = $(MAIN_EXTENSION)
OBJS         = src/semver.o src/debversion.o src/debver_evr.o
PG_CONFIG   ?= pg_config
EXTRA_CLEAN  = $(DATA_built) src/$(MAIN_EXTENSION).c
PG92         = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0| 9\.1" && echo no || echo yes)

ifeq ($(PG92),no)
$(error $(MAIN_EXTENSION) requires PostgreSQL 9.2 or higher)
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: $(DATA_built)

sql/$(MAIN_EXTENSION)--$(EXTVERSION).sql: sql/$(MAIN_EXTENSION).sql
	cp $< $@

sql/debversion--$(EXTVERSION).sql: sql/$(MAIN_EXTENSION).sql
	cp $< $@

src/$(MAIN_EXTENSION).c: src/$(MAIN_EXTENSION).c.in
	sed -e 's,__VERSION__,$(EXTVERSION),g' $< > $@

.PHONY: results
results:
	rsync -avP --delete results/ test/expected

dist:
	git archive --format zip --prefix=$(MAIN_EXTENSION)-$(DISTVERSION)/ -o $(MAIN_EXTENSION)-$(DISTVERSION).zip HEAD

latest-changes.md: Changes
	perl -e 'while (<>) {last if /^(v?\Q${DISTVERSION}\E)/; } print "Changes for v${DISTVERSION}:\n"; while (<>) { last if /^\s*$$/; s/^\s+//; print }' Changes > $@

# Temporary fix for PostgreSQL compilation chain / llvm bug, see
# https://github.com/rdkit/rdkit/issues/2192
COMPILE.cxx.bc = $(CLANG) -xc++ -Wno-ignored-attributes $(BITCODE_CPPFLAGS) $(CPPFLAGS) -emit-llvm -c
%.bc : %.cpp
	$(COMPILE.cxx.bc) -o $@ $<
	$(LLVM_BINPATH)/opt -module-summary -f $@ -o $@
