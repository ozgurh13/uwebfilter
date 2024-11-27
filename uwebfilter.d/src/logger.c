#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sqlite3.h>

#include "color.h"
#include "logger.h"

#define  DBPATH   "/var/log/uwebfilter.db"

static pthread_mutex_t sqlite_mutex = PTHREAD_MUTEX_INITIALIZER;
static sqlite3 *db;

static const char* sql_table_create =
	"CREATE TABLE IF NOT EXISTS logs (    "
	"    srcip                   TEXT,    "
	"    srcport                 INTEGER, "
	"    dstip                   TEXT,    "
	"    dstport                 INTEGER, "
	"    proto                   TEXT,    "
	"    domain                  TEXT,    "
	"    path                    TEXT,    "
	"    action                  TEXT,    "
	"    category                INTEGER, "
	"    logtime                 INTEGER  "
	");                                   ";

static sqlite3_stmt *stmt_insert;
static const char* sql_insert =
	"INSERT INTO logs VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

static void __attribute__((constructor))
db_init()
{
	if (sqlite3_open(DBPATH, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open database: %s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	if (sqlite3_exec(db, sql_table_create, 0, 0, NULL) != SQLITE_OK) {
		fprintf(stderr, "cannot create table: %s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt_insert, NULL) != SQLITE_OK) {
		fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}


static void
webfilterlog_insert(webfilterlog_t *webfilterlog)
{
	sqlite3_bind_text(stmt_insert,  1, webfilterlog->srcip,    -1, NULL);
	sqlite3_bind_int (stmt_insert,  2, webfilterlog->srcport);
	sqlite3_bind_text(stmt_insert,  3, webfilterlog->dstip,    -1, NULL);
	sqlite3_bind_int (stmt_insert,  4, webfilterlog->dstport);
	sqlite3_bind_text(stmt_insert,  5, webfilterlog->proto,    -1, NULL);
	sqlite3_bind_text(stmt_insert,  6, webfilterlog->domain,   -1, NULL);
	sqlite3_bind_text(stmt_insert,  7, webfilterlog->path,     -1, NULL);
	sqlite3_bind_text(stmt_insert,  8, webfilterlog->action,   -1, NULL);
	sqlite3_bind_int (stmt_insert,  9, webfilterlog->category);
	sqlite3_bind_int (stmt_insert, 10, webfilterlog->logtime);

	sqlite3_step (stmt_insert);
	sqlite3_reset(stmt_insert);
}

void
webfilterlog_write(webfilterlog_t *webfilterlog)
{
	pthread_mutex_lock(&sqlite_mutex);
	{
		webfilterlog_insert(webfilterlog);
	}
	pthread_mutex_unlock(&sqlite_mutex);
}


void
webfilterlog_print(webfilterlog_t *webfilterlog)
{
	const char* color =
		strcmp(webfilterlog->action, "allow") == 0 ? GREEN :
		strcmp(webfilterlog->action, "block") == 0 ? RED   :
		RESET;

	const int path_length = strlen(webfilterlog->path);
	const bool show_ellipsis = path_length >= 30;
	const int path_show_length = show_ellipsis ? 27 : path_length;
	const char* ellipses = show_ellipsis ? "..." : "";


	printf(
		"\n==========   LOG   ==========\n"
		"   SrcIP:                   %s%s%s\n"
		"   SrcPort:                 %s%u%s\n"
		"   DstIP:                   %s%s%s\n"
		"   DstPort:                 %s%u%s\n"
		"   Proto:                   %s%s%s\n"
		"   Domain:                  %s%s%s\n"
		"   Path:                    %s%.*s%s%s\n"
		"   Action:                  %s%s%s\n"
		"   Category:                %s%d%s\n"
		"   LogTime:                 %s%lu%s\n"
		"=============================\n\n",
	       	BOLDCYAN,                  webfilterlog->srcip,          RESET,
	       	BOLDRED,                   webfilterlog->srcport,        RESET,
	       	BOLDCYAN,                  webfilterlog->dstip,          RESET,
	       	BOLDRED,                   webfilterlog->dstport,        RESET,
	       	BOLDGREEN,                 webfilterlog->proto,          RESET,
		BLUE,                      webfilterlog->domain,         RESET,
		MAGENTA, path_show_length, webfilterlog->path, ellipses, RESET,
		color,                     webfilterlog->action,         RESET,
		color,                     webfilterlog->category,       RESET,
		YELLOW,                    webfilterlog->logtime,        RESET
	);
}
