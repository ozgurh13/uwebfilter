#include <stddef.h>
#include <sqlite3.h>

#include "utils.h"
#include "uwebfilterlog.h"


#define  DBPATH   "/var/log/uwebfilter.db"
static sqlite3 *db;

static const char* sql_table_create =
	"CREATE TABLE IF NOT EXISTS blocktypes (\n"
	"    id          INTEGER PRIMARY KEY,   \n"
	"    name        TEXT                   \n"
	");                                     \n"
	"INSERT OR IGNORE INTO blocktypes VALUES\n"
	"    (0, 'none'),                       \n"
	"    (1, 'application'),                \n"
	"    (2, 'category'),                   \n"
	"    (3, 'user');                       \n"
	"                                       \n"
	"CREATE TABLE IF NOT EXISTS logs (      \n"
	"    srcip              TEXT,           \n"
	"    srcport            INTEGER,        \n"
	"    dstip              TEXT,           \n"
	"    dstport            INTEGER,        \n"
	"    proto              TEXT,           \n"
	"    domain             TEXT,           \n"
	"    path               TEXT,           \n"
	"    action             TEXT,           \n"
	"    category           INTEGER,        \n"
	"    application        INTEGER,        \n"
	"    blocktype          INTEGER         \n"
	"        REFERENCES blocktype(id),      \n"
	"    logtime            INTEGER         \n"
	");                                     \n";

static sqlite3_stmt *stmt_insert;
static const char* sql_insert =
	"INSERT INTO logs VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

static void __attribute__((constructor))
db_init()
{
	if (sqlite3_open(DBPATH, &db) != SQLITE_OK) {
		die("cannot open database: %s", sqlite3_errmsg(db));
	}

	if (sqlite3_exec(db, sql_table_create, 0, 0, NULL) != SQLITE_OK) {
		die("cannot create table: %s", sqlite3_errmsg(db));
	}

	if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt_insert, NULL) != SQLITE_OK) {
		die("prepare failed: %s", sqlite3_errmsg(db));
	}
}

void
_uwebfilterlog_insert(uwebfilterlog_t *uwebfilterlog)
{
	sqlite3_bind_text(stmt_insert,  1, uwebfilterlog->srcip,    -1, NULL);
	sqlite3_bind_int (stmt_insert,  2, uwebfilterlog->srcport);
	sqlite3_bind_text(stmt_insert,  3, uwebfilterlog->dstip,    -1, NULL);
	sqlite3_bind_int (stmt_insert,  4, uwebfilterlog->dstport);
	sqlite3_bind_text(stmt_insert,  5, uwebfilterlog->proto,    -1, NULL);
	sqlite3_bind_text(stmt_insert,  6, uwebfilterlog->domain,   -1, NULL);
	sqlite3_bind_text(stmt_insert,  7, uwebfilterlog->path,     -1, NULL);
	sqlite3_bind_text(stmt_insert,  8, uwebfilterlog->action,   -1, NULL);
	sqlite3_bind_int (stmt_insert,  9, uwebfilterlog->category);
	sqlite3_bind_int (stmt_insert, 10, uwebfilterlog->application);
	sqlite3_bind_int (stmt_insert, 11, uwebfilterlog->blocktype);
	sqlite3_bind_int (stmt_insert, 12, uwebfilterlog->logtime);

	sqlite3_step (stmt_insert);
	sqlite3_reset(stmt_insert);
}
