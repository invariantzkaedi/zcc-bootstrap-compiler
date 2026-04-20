#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);

    // Use bind_double which we know works at the API level
    sqlite3_exec(db, "CREATE TABLE t(x);", 0, 0, 0);
    sqlite3_prepare_v2(db, "INSERT INTO t VALUES(?);", -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, 3.14);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Read back as every type
    sqlite3_prepare_v2(db, "SELECT x FROM t;", -1, &stmt, 0);
    sqlite3_step(stmt);

    int typ = sqlite3_column_type(stmt, 0);
    const unsigned char *blob = sqlite3_column_blob(stmt, 0);
    int blobsz = sqlite3_column_bytes(stmt, 0);
    printf("type=%d blobsz=%d\n", typ, blobsz);
    printf("raw bytes: ");
    for (int i = 0; i < blobsz && i < 8; i++)
        printf("%02x ", blob[i]);
    printf("\n");
    printf("double=%.6f\n", sqlite3_column_double(stmt, 0));
    printf("text=%s\n", sqlite3_column_text(stmt, 0));

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
