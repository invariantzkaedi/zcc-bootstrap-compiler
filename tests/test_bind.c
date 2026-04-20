#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t(x REAL);", 0, 0, 0);

    // Bind and insert
    sqlite3_prepare_v2(db, "INSERT INTO t VALUES(?);", -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, 3.14);
    printf("after bind, value=%.6f\n", sqlite3_column_double(stmt, 0));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Read back with sqlite3_column_double
    sqlite3_prepare_v2(db, "SELECT x FROM t;", -1, &stmt, 0);
    int rc = sqlite3_step(stmt);
    printf("step rc=%d\n", rc);

    // Read as int64 and double
    long long ival = sqlite3_column_int64(stmt, 0);
    double dval = sqlite3_column_double(stmt, 0);
    const char *tval = (const char*)sqlite3_column_text(stmt, 0);
    int typ = sqlite3_column_type(stmt, 0);
    printf("type=%d text=%s int64=%lld double=%.6f\n", typ, tval, ival, dval);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
