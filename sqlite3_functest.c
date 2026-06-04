#include <stdio.h>
extern const char *sqlite3_libversion(void);
extern int sqlite3_open(const char*, void**);
extern int sqlite3_exec(void*, const char*, void*, void*, char**);
extern const char *sqlite3_errmsg(void*);
extern int sqlite3_close(void*);

static int callback(void *unused, int ncols, char **vals, char **names){
    int i;
    for(i = 0; i < ncols; i++){
        printf("%s = %s\n", names[i], vals[i] ? vals[i] : "NULL");
    }
    printf("---\n");
    return 0;
}

int main(){
    void *db;
    char *err = 0;
    int rc;
    printf("SQLite %s compiled by ZCC\n", sqlite3_libversion());
    
    rc = sqlite3_open(":memory:", &db);
    printf("open rc=%d\n", rc);
    if(rc){ printf("OPEN FAILED: %s\n", sqlite3_errmsg(db)); return 1; }
    
    rc = sqlite3_exec(db, "SELECT 1;", callback, 0, &err);
    printf("SELECT 1 rc=%d err=%s\n", rc, err ? err : "none");
    
    rc = sqlite3_exec(db, "CREATE TABLE t1(x);", callback, 0, &err);
    printf("CREATE TABLE rc=%d err=%s\n", rc, err ? err : "none");
    
    rc = sqlite3_exec(db, "INSERT INTO t1 VALUES(42);", callback, 0, &err);
    printf("INSERT rc=%d err=%s\n", rc, err ? err : "none");
    
    rc = sqlite3_exec(db, "SELECT x FROM t1;", callback, 0, &err);
    printf("SELECT rc=%d err=%s\n", rc, err ? err : "none");
    
    sqlite3_close(db);
    return 0;
}
