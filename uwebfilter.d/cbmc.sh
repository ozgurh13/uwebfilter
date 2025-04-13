# https://www.cprover.org/cbmc/

if ! command -v cbmc 2>&1 >/dev/null; then
	echo cbmc not found
	exit 1
fi

# apt update && apt install -y libnetfilter-queue-dev libmnl-dev libnfnetlink-dev libcurl4-openssl-dev libjson-c-dev libsqlite3-dev

cbmc                               \
    --bounds-check                 \
    --pointer-check                \
    --memory-leak-check            \
    --memory-cleanup-check         \
    --conversion-check             \
    --undefined-shift-check        \
    --enum-range-check             \
    --pointer-primitive-check      \
    --malloc-may-fail              \
    --unwind 64                    \
    --unwinding-assertions         \
    --z3                           \
    source/src/*.c                 \
      1> /tmp/uwebfilterd.out      \
      2> /tmp/uwebfilterd.err


# known errors
# -----------
# -- library code
# <builtin-library-vfprintf> function vfprintf
#
# -- can't unwind loop all the way
# source/src/domain_lookup.c function domain_info_arena_init
# [domain_info_arena_init.unwind.0] line 146 unwinding assertion loop 0: ERROR
#
# -- can't find the body of these functions
# source/src/uwebfilterlog_db.c function db_init
# [db_init.no-body.sqlite3_open] line 46 no body for callee sqlite3_open: ERROR
# [db_init.no-body.sqlite3_errmsg] line 47 no body for callee sqlite3_errmsg: ERROR
# [db_init.no-body.sqlite3_exec] line 50 no body for callee sqlite3_exec: ERROR
# [db_init.no-body.sqlite3_prepare_v2] line 54 no body for callee sqlite3_prepare_v2: ERROR
