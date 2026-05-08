#include <stddef.h>
typedef struct { int dummy; int follows_pNode; } Limbox_aux;
typedef union {
  int *lastfree;
  char padding[offsetof(Limbox_aux, follows_pNode)];
} Limbox;
