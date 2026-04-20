#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);

    // Use a computed float, not a literal
    sqlite3_exec(db, "CREATE TABLE t(x);", 0, 0, 0);
    sqlite3_exec(db, "INSERT INTO t VALUES(1+1);", 0, 0, 0);
    sqlite3_exec(db, "INSERT INTO t VALUES(10/3);", 0, 0, 0);
    sqlite3_exec(db, "INSERT INTO t VALUES(10.0/3.0);", 0, 0, 0);

    // Also bind directly
    sqlite3_prepare_v2(db, "INSERT INTO t VALUES(?);", -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, 3.14);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db, "SELECT typeof(x), x FROM t;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("type=%-8s text=%-15s int=%lld double=%.6f\n",
            sqlite3_column_text(stmt,0),
            sqlite3_column_text(stmt,1),
            sqlite3_column_int64(stmt,1),
            sqlite3_column_double(stmt,1));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
