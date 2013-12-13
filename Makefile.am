SUBDIRS =   src scripts tests doc

ACLOCAL_AMFLAGS=-I m4

dist-hook:
	if [ -x "$(GIT)" ]; \
	then \
	    "$(GIT)" --git-dir=$(top_srcdir)/.git log --pretty=format:"[%h] %cd : %s (%an)" --date=short > $(distdir)/ChangeLog; \
	    git_run_version=`"$(GIT)" --git-dir=$(top_srcdir)/.git show --pretty=format:"%h %ci" HEAD | head -n 1`; \
	    git_run_branch=`"$(GIT)" --git-dir=$(top_srcdir)/.git branch | grep ^* | sed s/*\ //g`; \
	    git_version="git $${git_run_branch} $${git_run_version} developer version"; \
	    echo $${git_version} > ${distdir}/VERSION; \
	elif [ -e $(top_srcdir)/VERSION ]; \
	then \
	    cp $(top_srcdir)/VERSION $(distdir)/VERSION; \
	fi
