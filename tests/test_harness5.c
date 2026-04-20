#include <stdio.h>
#include "sqlite3.h"
int main() {
    sqlite3 *db;
    char *err = 0;
    int rc;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, val INTEGER);", 0, 0, &err);

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(1,'hello',314);", 0, 0, &err);
    printf("insert int: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(2,'world',3.14);", 0, 0, &err);
    printf("insert float: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT * FROM t;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("id=%d name=%s val=%s\n", sqlite3_column_int(stmt,0), sqlite3_column_text(stmt,1), sqlite3_column_text(stmt,2));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
