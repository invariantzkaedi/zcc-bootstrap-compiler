#include <stdio.h>
#include <stdlib.h>
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_exec(sqlite3*, const char *sql, int (*callback)(void*,int,char**,char**), void *, char **errmsg);
int sqlite3_close(sqlite3*);

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s | ", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

__asm__(".global __atomic_store_n\n__atomic_store_n:\nmovq %rsi, (%rdi)\nret\n");
__asm__(".global __atomic_load_n\n__atomic_load_n:\nmovq (%rdi), %rax\nret\n");

void execute(sqlite3 *db, const char *sql) {
    char *zErrMsg = 0;
    printf("Executing: %s\n", sql);
    int rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != 0) {
        printf("SQL error: %s\n", zErrMsg);
    }
    printf("---\n");
}

int main() {
    sqlite3 *db;
    int rc = sqlite3_open(":memory:", &db);
    if(rc != 0){
        printf("Can't open database\n");
        return 1;
    }

    execute(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, val INTEGER);");
    execute(db, "INSERT INTO t VALUES(1,'hello',314);");
    execute(db, "SELECT * FROM t;");

    for (int i = 0; i < 10; i++) {
        execute(db, "INSERT INTO t (name, val) SELECT name, val FROM t;"); // Duplicates rows to 1024
    }
    execute(db, "SELECT count(*), avg(val) FROM t;");

    execute(db, "CREATE INDEX idx ON t(name);");
    execute(db, "SELECT * FROM t WHERE name='hello' LIMIT 5;");

    execute(db, "CREATE TABLE t2(id INTEGER, ref INTEGER REFERENCES t(id));");
    execute(db, "INSERT INTO t2 VALUES(100, 1);");
    execute(db, "SELECT t.name, t2.id FROM t JOIN t2 ON t.id=t2.ref LIMIT 5;");

    execute(db, "CREATE TRIGGER tr AFTER INSERT ON t BEGIN INSERT INTO t2 VALUES(NEW.id, NEW.id); END;");
    execute(db, "INSERT INTO t (name, val) VALUES('trigger_test', 999);");
    execute(db, "SELECT count(*) FROM t2;");

    execute(db, "EXPLAIN QUERY PLAN SELECT * FROM t WHERE name LIKE 'h%';");

    sqlite3_close(db);
    printf("All progressed tests completed!\n");
    return 0;
}
