#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);

    // No table — just evaluate a float expression
    sqlite3_prepare_v2(db, "SELECT 3.14;", -1, &stmt, 0);
    sqlite3_step(stmt);
    printf("SELECT 3.14: type=%d double=%.6f text=%s\n",
        sqlite3_column_type(stmt, 0),
        sqlite3_column_double(stmt, 0),
        sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    // Integer for comparison
    sqlite3_prepare_v2(db, "SELECT 314;", -1, &stmt, 0);
    sqlite3_step(stmt);
    printf("SELECT 314: type=%d int=%lld text=%s\n",
        sqlite3_column_type(stmt, 0),
        sqlite3_column_int64(stmt, 0),
        sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    // Float arithmetic
    sqlite3_prepare_v2(db, "SELECT 1.0 + 2.0;", -1, &stmt, 0);
    sqlite3_step(stmt);
    printf("SELECT 1+2: type=%d double=%.6f text=%s\n",
        sqlite3_column_type(stmt, 0),
        sqlite3_column_double(stmt, 0),
        sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    return 0;
}
