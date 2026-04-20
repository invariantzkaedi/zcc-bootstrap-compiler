#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    char *err = 0;
    int rc;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE t(x REAL);", 0, 0, &err);

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(3);", 0, 0, &err);
    printf("int 3: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(0.5);", 0, 0, &err);
    printf("float 0.5: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(.5);", 0, 0, &err);
    printf("float .5: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(3.0);", 0, 0, &err);
    printf("float 3.0: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    rc = sqlite3_exec(db, "INSERT INTO t VALUES(1e2);", 0, 0, &err);
    printf("sci 1e2: rc=%d err=%s\n", rc, err ? err : "none");
    if (err) { sqlite3_free(err); err = 0; }

    sqlite3_close(db);
    return 0;
}
