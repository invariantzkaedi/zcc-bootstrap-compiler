#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    char *err = 0;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    printf("open: rc=%d\n", rc);

    rc = sqlite3_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, val REAL);", 0, 0, &err);
    printf("create: rc=%d err=%s\n", rc, err ? err : "none");

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(1,'hello',3.14);", 0, 0, &err);
    printf("insert: rc=%d err=%s\n", rc, err ? err : "none");

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, "SELECT * FROM t;", -1, &stmt, 0);
    printf("prepare: rc=%d\n", rc);

    rc = sqlite3_step(stmt);
    printf("step: rc=%d (want 100=SQLITE_ROW)\n", rc);

    if (rc == 100) {
        printf("id=%d name=%s val=%.2f\n", sqlite3_column_int(stmt,0), sqlite3_column_text(stmt,1), sqlite3_column_double(stmt,2));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
