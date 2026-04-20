/* Pattern 1: paren-wrapped function name, pointer return */
typedef struct S S;
extern S *(func1)(int x);

/* Pattern 2: unparenthesized (should already work) */
extern S *func2(int x);

/* Pattern 3: paren-wrapped function name, plain return */
extern int (func3)(int x);

/* Pattern 4: double-pointer return, paren-wrapped name */
extern S **(func4)(int x);

/* Pattern 5: function returning function pointer (non-paren) */
extern int (*func5(int x))(int);
