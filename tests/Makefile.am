
SUBDIRS = gens

CLEANFILES = tests.log tests.log.xml

check-local: $(top_srcdir)/scripts/bets
	$(top_srcdir)/scripts/bets $(BETS_OPTIONS) -o tests.log $(srcdir)/test

