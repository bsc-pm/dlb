
SUBDIRS = gens

DISTCLEANFILES = tests.log tests.log.xml

EXTRA_DIST = test

check-local: $(top_srcdir)/scripts/bets
	$(top_srcdir)/scripts/bets $(BETS_OPTIONS) -o tests.log $(srcdir)/test

