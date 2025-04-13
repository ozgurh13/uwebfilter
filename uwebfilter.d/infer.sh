# https://fbinfer.com/

if ! command -v infer 2>&1 >/dev/null; then
	echo infer not found
	exit 1
fi


infer run                          \
    --reactive                     \
    --bufferoverrun                \
    --liveness                     \
    --loop-hoisting                \
    --parameter-not-null-checked   \
    --pulse                        \
    --racerd                       \
    --siof                         \
    --starvation                   \
    -o /tmp/uwebfilterd-infer      \
    -- make -C source all clean


# known errors (don't try to fix)
# ------------
# #1
# source/src/config.c:267: error: Integer Overflow L2
#   ([0, +oo] - 1):unsigned32.
#   265. 		}
#   266.
#   267. 		HASH_ADD_STR(config.domains_blacklist, domain, d);
#          ^
#   268. 	}
#   269. }
#
# #2
# source/src/config.c:295: error: Integer Overflow L2
#   ([0, +oo] - 1):unsigned32.
#   293. 		}
#   294.
#   295. 		HASH_ADD_STR(config.domains_whitelist, domain, d);
#          ^
#   296. 	}
#   297. }
#
# #3
# source/src/config.c:464: error: Integer Overflow L2
#   ([0, +oo] - 1):unsigned32 by call to `config_domain_lookup`.
#   462. config_is_domain_blacklisted(const char domain[DOMAIN_LENGTH])
#   463. {
#   464. 	return config_domain_lookup(config.domains_blacklist, domain);
#                ^
#   465. }
#   466.
#
# #4
# source/src/config.c:473: error: Integer Overflow L2
#   ([0, +oo] - 1):unsigned32 by call to `config_domain_lookup`.
#   471. config_is_domain_whitelisted(const char domain[DOMAIN_LENGTH])
#   472. {
#   473. 	return config_domain_lookup(config.domains_whitelist, domain);
#                ^
#   474. }
#
# #5
# source/src/domain_lookup.c:147: error: Buffer Overrun L2
#   Offset: [0, 131070] (⇐ [0, 65535] + [0, 65535]) Size: 65536.
#   145. {
#   146. 	for (size_t i = 0; i < DOMAIN_INFO_ARENA_SIZE; i++) {
#   147. 		domain_info_arena_ptr[i] = &_domain_info_arena[i];
#          ^
#   148. 	}
#   149. }
#
# #6
# source/src/domain_lookup.c:245: error: Integer Overflow L2
#   ([0, `domain_info_cache->cache->hh.tbl->buckets->count`] - 1):unsigned32 by call to `domain_info_cache_delete`.
#   243. 			if (d->expire < now || rand() < 0.2) {
#   244. 				logger_debug("[gc] %s", d->domain);
#   245. 				domain_info_cache_delete(domain_info_cache, d);
#            ^
#   246. 				domain_info_free(d);
#   247. 			}
#
# #7
# source/src/domain_lookup.c:245: error: Integer Overflow L2
#   ([0, `domain_info_cache->cache->hh.tbl->num_items`] - 1):unsigned32 by call to `domain_info_cache_delete`.
#   243. 			if (d->expire < now || rand() < 0.2) {
#   244. 				logger_debug("[gc] %s", d->domain);
#   245. 				domain_info_cache_delete(domain_info_cache, d);
#            ^
#   246. 				domain_info_free(d);
#   247. 			}
#
# #8
# source/src/domain_lookup.c:280: error: Integer Overflow L2
#   ([0, +oo] - 1):unsigned32 by call to `domain_info_cache_delete`.
#   278. 		}
#   279.
#   280. 		domain_info_cache_delete(&domain_info_cache, domain_info);
#          ^
#   281. 		domain_info_free(domain_info);
#   282. 		domain_info = NULL;
#
# #9
# source/src/tcpflow.c:50: error: Integer Overflow L2
#   ([0, +oo] - 1):unsigned32.
#   48. tcpflow_insert(tcpflow_t *tcpflow)
#   49. {
#   50. 	HASH_ADD(hh, tcpflow_cache.cache, key, tcpflow_key_len, tcpflow);
#        ^
#   51.
#   52. 	const time_t now = time(NULL);
