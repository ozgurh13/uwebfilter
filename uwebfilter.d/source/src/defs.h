#ifndef __DEFS_H__
#define __DEFS_H__

/*
 * note: buffer sizes should have (+1) length
 */

#define   likely(x)                        __builtin_expect(!!(x), 1)
#define   unlikely(x)                      __builtin_expect(!!(x), 0)

#define   IPADDR_LENGTH                     16
#define   DOMAIN_LENGTH                    256
#define   PATH_LENGTH                     1024

#define   SERVERADDR_LENGTH                256
#define   USERNAME_LENGTH                   64
#define   PASSWORD_LENGTH                   64

#define   BLOCKTYPE_NONE                     0
#define   BLOCKTYPE_APPLICATION              1
#define   BLOCKTYPE_CATEGORY                 2
#define   BLOCKTYPE_USER_BLACKLIST           3
#define   BLOCKTYPE_USER_WHITELIST           4

#endif
