#################################################################################
#  Copyright 2017 Barcelona Supercomputing Center                               #
#                                                                               #
#  This file is part of the DLB library.                                        #
#                                                                               #
#  DLB is free software: you can redistribute it and/or modify                  #
#  it under the terms of the GNU Lesser General Public License as published by  #
#  the Free Software Foundation, either version 3 of the License, or            #
#  (at your option) any later version.                                          #
#                                                                               #
#  DLB is distributed in the hope that it will be useful,                       #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of               #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
#  GNU Lesser General Public License for more details.                          #
#                                                                               #
#  You should have received a copy of the GNU Lesser General Public License     #
#  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 #
#################################################################################


check-local: $(top_srcdir)/scripts/bets clean-coverage-data
	$(top_srcdir)/scripts/bets $(BETS_OPTIONS) $(srcdir)/test/$(BETS_SUBDIR)

if ENABLE_COVERAGE
coverage-local: check-local
	$(mkdir_p) coverage
	gcovr -r $(abs_top_builddir) --html --html-details -o coverage/index.html

clean-coverage-data:
	find $(abs_top_builddir) -name '*.gcda' -delete

clean-local: clean-coverage-data
	find $(abs_top_builddir) -name '*.gcno' -delete
	rm -rf coverage/
else
coverage-local:
	@echo "coverage is not configured."
clean-coverage-data:
clean-local:
endif
