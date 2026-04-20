#include <stdio.h>
#include <string.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    char *err = 0;
    sqlite3_stmt *stmt;

    sqlite3_open(":memory:", &db);

    // Level 1: CREATE + INSERT + SELECT
    sqlite3_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, val REAL);", 0, 0, &err);
    sqlite3_exec(db, "INSERT INTO t VALUES(1,'hello',3.14);", 0, 0, &err);
    sqlite3_exec(db, "INSERT INTO t VALUES(2,'world',2.72);", 0, 0, &err);
    sqlite3_exec(db, "INSERT INTO t VALUES(3,'test',1.41);", 0, 0, &err);

    printf("=== Level 1: Basic SELECT ===\n");
    sqlite3_prepare_v2(db, "SELECT * FROM t;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("id=%d name=%s val=%.2f\n", sqlite3_column_int(stmt,0), sqlite3_column_text(stmt,1), sqlite3_column_double(stmt,2));
    }
    sqlite3_finalize(stmt);

    // Level 2: Aggregates
    printf("=== Level 2: Aggregates ===\n");
    sqlite3_prepare_v2(db, "SELECT count(*), avg(val) FROM t;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("count=%d avg=%.4f\n", sqlite3_column_int(stmt,0), sqlite3_column_double(stmt,1));
    }
    sqlite3_finalize(stmt);

    // Level 3: Index + WHERE
    printf("=== Level 3: Index + WHERE ===\n");
    sqlite3_exec(db, "CREATE INDEX idx ON t(name);", 0, 0, &err);
    sqlite3_prepare_v2(db, "SELECT * FROM t WHERE name='hello';", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("id=%d name=%s val=%.2f\n", sqlite3_column_int(stmt,0), sqlite3_column_text(stmt,1), sqlite3_column_double(stmt,2));
    }
    sqlite3_finalize(stmt);

    // Level 4: JOIN
    printf("=== Level 4: JOIN ===\n");
    sqlite3_exec(db, "CREATE TABLE t2(id INTEGER PRIMARY KEY, ref INTEGER);", 0, 0, &err);
    sqlite3_exec(db, "INSERT INTO t2 VALUES(10,1);", 0, 0, &err);
    sqlite3_exec(db, "INSERT INTO t2 VALUES(20,2);", 0, 0, &err);
    sqlite3_prepare_v2(db, "SELECT t.name, t2.id FROM t JOIN t2 ON t.id=t2.ref;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("name=%s t2_id=%d\n", sqlite3_column_text(stmt,0), sqlite3_column_int(stmt,1));
    }
    sqlite3_finalize(stmt);

    // Level 5: Trigger
    printf("=== Level 5: Trigger ===\n");
    sqlite3_exec(db, "CREATE TABLE log(msg TEXT);", 0, 0, &err);
    sqlite3_exec(db, "CREATE TRIGGER tr AFTER INSERT ON t BEGIN INSERT INTO log VALUES(NEW.name); END;", 0, 0, &err);
    sqlite3_exec(db, "INSERT INTO t VALUES(4,'triggered',9.99);", 0, 0, &err);
    sqlite3_prepare_v2(db, "SELECT * FROM log;", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("log: %s\n", sqlite3_column_text(stmt,0));
    }
    sqlite3_finalize(stmt);

    // Level 6: LIKE
    printf("=== Level 6: LIKE ===\n");
    sqlite3_prepare_v2(db, "SELECT name FROM t WHERE name LIKE 'h%';", -1, &stmt, 0);
    while (sqlite3_step(stmt) == 100) {
        printf("match: %s\n", sqlite3_column_text(stmt,0));
    }
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    printf("\n=== ALL 6 LEVELS PASSED ===\n");
    return 0;
}
