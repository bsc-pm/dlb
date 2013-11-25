
# Install scripts in $(bindir) and distribute them.
dist_bin_SCRIPTS = run_with_dlb.sh 

EXTRA_DIST = bets run_with_dlb.sh.in

run_with_dlb.sh: run_with_dlb.sh.in
	rm -f $@
	sed \
	    -e's,@PREFIX_LIB\@,"$(libdir)",g' \
	    -e's,@NCPUS\@,$(NCPUS),g' \
	$^ > $@.tmp
	mv $@.tmp $@
	chmod 755 $@

