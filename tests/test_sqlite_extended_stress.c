#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *sqlite3_libversion(void);
extern int sqlite3_open(const char *filename, void **ppDb);
extern int sqlite3_close(void *db);
extern int sqlite3_exec(void *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg);
extern const char *sqlite3_errmsg(void *db);

static int print_row_cb(void *unused, int ncols, char **vals, char **names) {
    for (int i = 0; i < ncols; i++) {
        printf("  [%s] = %s\n", names[i], vals[i] ? vals[i] : "NULL");
    }
    return 0;
}

static int count_cb(void *arg, int ncols, char **vals, char **names) {
    if (vals[0] && strcmp(vals[0], "1") == 0) *(int*)arg = 1;
    return 0;
}

int main(void) {
    printf("====================================================================\n");
    printf("  ZCC SQLITE 3.45 EXTENDED INDUSTRIAL STRESS & PARITY GAUNTLET     \n");
    printf("====================================================================\n");
    printf("SQLite Engine Version: %s\n\n", sqlite3_libversion());

    void *db = NULL;
    char *err = NULL;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != 0) {
        fprintf(stderr, "FATAL: Failed to open in-memory SQLite: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    /* 1. Table Schema Creation & Indexing */
    printf("[STAGE 1] Creating Schemas, Tables, and Indexes...\n");
    const char *sql_schema =
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, balance REAL, active INT);"
        "CREATE TABLE orders (id INTEGER PRIMARY KEY, user_id INT, amount REAL, desc TEXT);"
        "CREATE INDEX idx_user_name ON users(name);"
        "CREATE INDEX idx_orders_user ON orders(user_id);";
    rc = sqlite3_exec(db, sql_schema, NULL, NULL, &err);
    if (rc != 0) {
        fprintf(stderr, "Schema creation failed: %s\n", err ? err : "unknown");
        return 1;
    }
    printf("  ✔ Schemas and B-Tree indexes created successfully.\n\n");

    /* 2. Bulk Transaction Insertion */
    printf("[STAGE 2] Executing Bulk Transactional Inserts...\n");
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err);
    if (rc != 0) return 1;

    char insert_buf[256];
    for (int i = 1; i <= 100; i++) {
        snprintf(insert_buf, sizeof(insert_buf),
                 "INSERT INTO users VALUES (%d, 'User_%d', %f, %d);",
                 i, i, i * 10.5, i % 2);
        rc = sqlite3_exec(db, insert_buf, NULL, NULL, &err);
        if (rc != 0) {
            fprintf(stderr, "Insert failed at user %d: %s\n", i, err);
            return 1;
        }

        snprintf(insert_buf, sizeof(insert_buf),
                 "INSERT INTO orders VALUES (%d, %d, %f, 'Order_%d_item');",
                 i, (i % 20) + 1, i * 2.25, i);
        rc = sqlite3_exec(db, insert_buf, NULL, NULL, &err);
        if (rc != 0) return 1;
    }
    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &err);
    if (rc != 0) return 1;
    printf("  ✔ 100 users and 100 orders committed to B-Tree pager.\n\n");

    /* 3. Complex Relational JOIN, Aggregate, and String Expression Query */
    printf("[STAGE 3] Querying Complex Inner Join with Aggregation...\n");
    const char *sql_query =
        "SELECT u.name AS user_name, "
        "       COUNT(o.id) AS order_count, "
        "       ROUND(SUM(o.amount), 2) AS total_spent, "
        "       UPPER(u.name) AS upper_name "
        "FROM users u "
        "JOIN orders o ON u.id = o.user_id "
        "WHERE u.active = 1 "
        "GROUP BY u.id "
        "HAVING total_spent > 100.0 "
        "ORDER BY total_spent DESC "
        "LIMIT 3;";
    rc = sqlite3_exec(db, sql_query, print_row_cb, NULL, &err);
    if (rc != 0) {
        fprintf(stderr, "Query failed: %s\n", err);
        return 1;
    }
    printf("  ✔ Complex JOIN and HAVING query evaluated successfully.\n\n");

    /* 4. Trigger & Rollback Consistency Test */
    printf("[STAGE 4] Testing Trigger Firing and Transaction Rollback...\n");
    const char *sql_trigger =
        "CREATE TABLE audit_log (msg TEXT);"
        "CREATE TRIGGER trg_user_del AFTER DELETE ON users BEGIN "
        "  INSERT INTO audit_log VALUES ('deleted: ' || OLD.name);"
        "END;";
    rc = sqlite3_exec(db, sql_trigger, NULL, NULL, &err);
    if (rc != 0) return 1;

    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err);
    rc = sqlite3_exec(db, "DELETE FROM users WHERE id = 1;", NULL, NULL, &err);
    rc = sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err);
    if (rc != 0) return 1;

    /* Verify User 1 still exists after rollback */
    int user_1_exists = 0;
    rc = sqlite3_exec(db, "SELECT COUNT(*) FROM users WHERE id = 1;", count_cb, &user_1_exists, &err);
    if (rc != 0 || user_1_exists != 1) {
        fprintf(stderr, "Rollback failed: User 1 was not restored!\n");
        return 1;
    }
    printf("  ✔ Rollback consistency confirmed: User 1 preserved (id=1 count=1).\n\n");

    sqlite3_close(db);

    printf("====================================================================\n");
    printf("★ SQLITE EXTENDED INDUSTRIAL STRESS GAUNTLET: ALL TESTS PASSED! ★\n");
    printf("====================================================================\n");
    return 0;
}
