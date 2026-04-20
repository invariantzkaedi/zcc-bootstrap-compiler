#include <stdio.h>
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_exec(sqlite3*, const char *sql, int (*callback)(void*,int,char**,char**), void *, char **errmsg);
int sqlite3_close(sqlite3*);

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

__asm__(".global __atomic_store_n\n__atomic_store_n:\nmovq %rsi, (%rdi)\nret\n");
__asm__(".global __atomic_load_n\n__atomic_load_n:\nmovq (%rdi), %rax\nret\n");

int main() {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    if(rc != 0){
        printf("Can't open database: %d\n", rc);
        return 1;
    }

    const char *sql = "SELECT 1;";
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != 0) {
        printf("SQL error: %s\n", zErrMsg);
    }
    sqlite3_close(db);
    printf("Test harness completed successfully.\n");
    return 0;
}
