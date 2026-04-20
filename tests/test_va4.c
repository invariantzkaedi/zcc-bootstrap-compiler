#include <stdio.h>
#include <string.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t(x);", 0, 0, 0);

    sqlite3_prepare_v2(db, "INSERT INTO t VALUES(?);", -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, 3.14);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db, "SELECT x FROM t;", -1, &stmt, 0);
    sqlite3_step(stmt);
    
    double val_d = sqlite3_column_double(stmt, 0);
    const unsigned char* val_s = sqlite3_column_text(stmt, 0);
    
    unsigned long long val_bits;
    memcpy(&val_bits, &val_d, 8);
    
    printf("val_d=%f (HEX:%llx)\n", val_d, val_bits);
    printf("val_s=%s\n", val_s);
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
