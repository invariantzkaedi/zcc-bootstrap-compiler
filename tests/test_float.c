#include <stdio.h>
#include "sqlite3.h"
int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t(x REAL);", 0, 0, 0);
    sqlite3_exec(db, "INSERT INTO t VALUES(3.14);", 0, 0, 0);

    // Read as text to see what SQLite actually stored
    sqlite3_prepare_v2(db, "SELECT typeof(x), x, CAST(x AS TEXT) FROM t;", -1, &stmt, 0);
    sqlite3_step(stmt);
    printf("type=%s  col_double=%.6f  col_text=%s  cast=%s\n",
        sqlite3_column_text(stmt,0),
        sqlite3_column_double(stmt,1),
        sqlite3_column_text(stmt,1),
        sqlite3_column_text(stmt,2));
    sqlite3_finalize(stmt);

    // Try binding a double directly
    sqlite3_prepare_v2(db, "INSERT INTO t VALUES(?);", -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, 2.718);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db, "SELECT x FROM t;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("text=%s double=%.6f\n", sqlite3_column_text(stmt,0), sqlite3_column_double(stmt,1));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
