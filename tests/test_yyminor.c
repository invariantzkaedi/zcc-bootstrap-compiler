typedef struct { int x; } Token;
typedef struct { int x; } TriggerStep;
typedef struct { int x; } Window;
typedef struct { int x; } Select;
typedef struct { int x; } SrcList;
struct TrigEvent { int x; };
typedef struct { int x; } IdList;
typedef unsigned int u32;
typedef struct { int x; } ExprList;
typedef struct { int x; } Cte;
typedef struct { int x; } Upsert;
typedef unsigned char u8;
typedef struct { int x; } With;
typedef struct { int x; } Expr;
typedef struct { int x; } OnOrUsing;
struct FrameBound { int x; };

typedef union {
  int yyinit;
  Token yy0;
  TriggerStep* yy33;
  Window* yy41;
  Select* yy47;
  SrcList* yy131;
  struct TrigEvent yy180;
  struct {int value; int mask;} yy231;
  IdList* yy254;
  u32 yy285;
  ExprList* yy322;
  Cte* yy385;
  int yy394;
  Upsert* yy444;
  u8 yy516;
  With* yy521;
  const char* yy522;
  Expr* yy528;
  OnOrUsing yy561;
  struct FrameBound yy595;
} YYMINORTYPE;

struct yyStackEntry {
  YYMINORTYPE minor;
};

int main() {
    struct yyStackEntry yymsp[10];
    yymsp[3].minor.yy394 = 5;
    return yymsp[3].minor.yy394;
}
