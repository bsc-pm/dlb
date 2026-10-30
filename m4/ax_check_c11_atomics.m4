
dnl AX_CHECK_C11_ATOMICS
dnl Sets ax_cv_c_stdatomic to "yes"/"no" and defines HAVE_C11_ATOMICS
dnl if <stdatomic.h> exists AND the compiler implements C11 atomics.

# AX_CHECK_C11_ATOMICS
# --------------------
AC_DEFUN([AX_CHECK_C11_ATOMICS],
[
    AC_CHECK_HEADERS([stdatomic.h], [], [], [])
    AC_CACHE_CHECK([for a working C11 <stdatomic.h>],
        [ax_cv_c_stdatomic],
        [AS_IF([test "x$ac_cv_header_stdatomic_h" != xyes], [
            ax_cv_c_stdatomic=no
        ],[
            AX_VAR_PUSHVALUE([CFLAGS], [-Wall -Werror])
            AC_COMPILE_IFELSE([
                AC_LANG_PROGRAM([[
                    #include <stdatomic.h>
                    #include <stdint.h>

                    #ifdef __STDC_NO_ATOMICS__
                    # error "compiler declares no atomics support"
                    #endif

                    _Atomic int           g_i = 0;
                    atomic_int_least64_t  g_i64;
                    atomic_uint_least64_t g_u64;
                ]], [[
                    // check implicit conversion
                    int base = g_i;
                    g_i = base + 1;

                    // check correct types
                    atomic_store_explicit(&g_i64, 2, memory_order_release);
                    int_least64_t expected = atomic_load_explicit(&g_i64, memory_order_acquire);
                    (void) atomic_compare_exchange_strong(&g_i64, &expected, 3);
                    (void) atomic_fetch_add(&g_u64, 1);
                ]])
            ], [
                ax_cv_c_stdatomic=yes
            ], [
                ax_cv_c_stdatomic=no
            ])
            AX_VAR_POPVALUE([CFLAGS])
        ])
    ])

    AS_IF([test "x$ax_cv_c_stdatomic" = xyes], [
        AC_DEFINE([HAVE_C11_ATOMICS], [1],
                  [Define to 1 if <stdatomic.h> provides full C11 atomics])
    ])
])
