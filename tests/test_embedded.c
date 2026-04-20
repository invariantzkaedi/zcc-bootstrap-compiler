typedef union {
    int yy0;
    int yy394;
} YYMINORTYPE;

struct yyStackEntry {
    int _a;
    int _b;
    YYMINORTYPE minor;
};
typedef struct yyStackEntry yyStackEntry;

int main() {
    yyStackEntry *yymsp;
    yymsp[3].minor.yy394 = 5;
    return yymsp[3].minor.yy394;
}
