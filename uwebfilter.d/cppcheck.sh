# http://cppcheck.net/

if ! command -v cppcheck 2>&1 >/dev/null; then
	echo cppcheck not found
	exit 1
fi


# don't use `-j`, it misses some cases
# `--addon=misra` takes a long time
# caching
#     mkdir -p /tmp/uwebfilterd-cppcheck-d
#     --cppcheck-build-dir=/tmp/uwebfilterd-cppcheck-d
cppcheck                                               \
    --force                                            \
    --enable=all                                       \
    --inline-suppr                                     \
    --suppress=missingIncludeSystem                    \
    --check-level=exhaustive                           \
    source/src                                         \
    2> /tmp/uwebfilterd-cppcheck


# known errors
# ------------
# anything related to uthash.h
#
# logger.c: The function 'logger_info' is never used. [unusedFunction]
# logger.c: The function 'logger_warning' is never used. [unusedFunction]
# logger.c: The function 'logger_critical' is never used. [unusedFunction]
#
# config.c: The function 'domain_free' is never used. [unusedFunction]
#
# domain_lookup.c: The function 'json_object_put1' is never used. [unusedFunction]
# domain_lookup.c: The function 'curl_json_buffer_cleanup' is never used. [unusedFunction]
