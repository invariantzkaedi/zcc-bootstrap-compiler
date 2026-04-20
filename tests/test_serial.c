#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t(x);", 0, 0, 0);

    // Bind double directly (bypasses SQL parsing)
    sqlite3_prepare_v2(db, "INSERT INTO t VALUES(?);", -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, 3.14);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Read back - compare text vs double
    sqlite3_prepare_v2(db, "SELECT x, typeof(x) FROM t;", -1, &stmt, 0);
    sqlite3_step(stmt);
    printf("text=%s type=%s double=%.6f\n",
        sqlite3_column_text(stmt, 0),
        sqlite3_column_text(stmt, 1),
        sqlite3_column_double(stmt, 0));
    sqlite3_finalize(stmt);

    // Also: does SELECT 3.14 still work?
    sqlite3_prepare_v2(db, "SELECT 3.14;", -1, &stmt, 0);
    sqlite3_step(stmt);
    printf("direct: double=%.6f text=%s\n",
        sqlite3_column_double(stmt, 0),
        sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    return 0;
}
