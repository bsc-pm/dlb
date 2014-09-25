# AX_FLAGS_SAVE
# -------------
AC_DEFUN([AX_FLAGS_SAVE],
[
   saved_LIBS="${LIBS}"
   saved_CC="${CC}"
   saved_CFLAGS="${CFLAGS}"
   saved_CXXFLAGS="${CXXFLAGS}"
   saved_CPPFLAGS="${CPPFLAGS}"
   saved_LDFLAGS="${LDFLAGS}"
])


# AX_FLAGS_RESTORE
# ----------------
AC_DEFUN([AX_FLAGS_RESTORE],
[
   LIBS="${saved_LIBS}"
   CC="${saved_CC}"
   CFLAGS="${saved_CFLAGS}"
   CXXFLAGS="${saved_CXXFLAGS}"
   CPPFLAGS="${saved_CPPFLAGS}"
   LDFLAGS="${saved_LDFLAGS}"
])


# AX_CHECK_POINTER_SIZE
# ---------------------
AC_DEFUN([AX_CHECK_POINTER_SIZE],
[
   AC_TRY_RUN(
      [
         int main()
         {
            return sizeof(void *)*8;
         }
      ],
      [ BITS="0" ],
      [ BITS="$?"]
   )
   AC_MSG_CHECKING([for binary type])
   AC_MSG_RESULT([$BITS bits])
])


# AX_FIND_INSTALLATION
# --------------------
AC_DEFUN([AX_FIND_INSTALLATION],
[
   AC_REQUIRE([AX_CHECK_POINTER_SIZE])

   # Search for home directory
   AC_MSG_CHECKING([for $1 installation])
   for home_dir in [$2 "not found"]; do
      if test -d "$home_dir/$BITS" ; then
         home_dir="$home_dir/$BITS"
         break
      elif test -d "$home_dir" ; then
         break
      fi
   done
   AC_MSG_RESULT([$home_dir])
   $1_HOME="$home_dir"
   if test "$$1_HOME" = "not found" ; then
      $1_HOME=""
   else

      # Did the user passed a headers directory to check first?
      AC_ARG_WITH([$3-headers],
                  AS_HELP_STRING(
                     [--with-$3-headers@<:@=ARG@:>@],
                     [Specify location of include files for package $3]
                  ),
                  [ForcedHeaders="$withval"],
                  [ForcedHeaders=""]
            )

      # Search for includes directory
      AC_MSG_CHECKING([for $1 includes directory])
      if test "${ForcedHeaders}" = "" ; then
         for incs_dir in [$$1_HOME/include$BITS $$1_HOME/include "not found"] ; do
            if test -d "$incs_dir" ; then
               break
            fi
         done
      else
         for incs_dir in [${ForcedHeaders} "not found"] ; do
            if test -d "$incs_dir" ; then
               break
            fi
         done
      fi
      AC_MSG_RESULT([$incs_dir])

      $1_INCLUDES="$incs_dir"
      if test "$$1_INCLUDES" = "not found" ; then
         AC_MSG_ERROR([Unable to find header directory for package $3. Check option --with-$3-headers.])
      else
         $1_CPPFLAGS="-I$$1_INCLUDES"
      fi

      # Did the user passed a library directory to check first?
      AC_ARG_WITH([$3-libs],
                  AS_HELP_STRING(
                     [--with-$3-libs@<:@=ARG@:>@],
                     [Specify location of library files for package $3]
                  ),
                  [ForcedLibs="$withval"],
                  [ForcedLibs=""]
      )

      # Search for libs directory
      AC_MSG_CHECKING([for $1 libraries directory])
      if test "${ForcedLibs}" = "" ; then
         for libs_dir in [$$1_HOME/lib$BITS $$1_HOME/lib "not found"] ; do
            if test -d "$libs_dir" ; then
               break
            fi
         done
      else
         for libs_dir in [${ForcedLibs} "not found"] ; do
            if test -d "$libs_dir" ; then
               break
            fi
         done
      fi
      AC_MSG_RESULT([$libs_dir])

      $1_LIBSDIR="$libs_dir"
      if test "$$1_LIBSDIR" = "not found" ; then
         AC_MSG_ERROR([Unable to find library directory for package $3. Check option --with-$3-libs.])
      else
         $1_LDFLAGS="-L$$1_LIBSDIR"
         if test -d "$$1_LIBSDIR/shared" ; then
            $1_SHAREDLIBSDIR="$$1_LIBSDIR/shared"
         else
            $1_SHAREDLIBSDIR=$$1_LIBSDIR
         fi
      fi
   fi

   # Everything went OK?
   if test "$$1_HOME" != "" -a "$$1_INCLUDES" != "" -a "$$1_LIBSDIR" != "" ; then
      $1_INSTALLED="yes"

      AC_SUBST($1_HOME)
      AC_SUBST($1_INCLUDES)

      AC_SUBST($1_CPPFLAGS)

      AC_SUBST($1_LDFLAGS)
      AC_SUBST($1_SHAREDLIBSDIR)
      AC_SUBST($1_LIBSDIR)

      # Update the default variables so the automatic checks will take into account the new directories
      CPPFLAGS="$CPPFLAGS $$1_CPPFLAGS"
      LDFLAGS="$LDFLAGS $$1_LDFLAGS"
   else
      $1_INSTALLED="no"
   fi
])

AC_DEFUN([AX_GCC_VERSION_CHECK], [

   AS_IF([test "x$GCC" = "xyes"],[
      AC_CACHE_CHECK([gcc version],[ax_cv_gcc_version],[
         ax_cv_gcc_version="`$CC -dumpversion`"
         ax_cv_gcc_version_major=$(echo $ax_cv_gcc_version | cut -d'.' -f1)
         ax_cv_gcc_version_minor=$(echo $ax_cv_gcc_version | cut -d'.' -f2)
         # GCC > 4.3
         AS_IF([test $ax_cv_gcc_version_major = 4 -a $ax_cv_gcc_version_minor -gt 3],[
            ax_cv_gcc_version_warnings="-Wno-unused-result "
         ])
         # GCC < 4.5
         AS_IF([test $ax_cv_gcc_version_major = 4 -a $ax_cv_gcc_version_minor -lt 5],[
            ax_cv_gcc_version_warnings="-Wno-uninitialized "
         ])
      ])
   ])
])
