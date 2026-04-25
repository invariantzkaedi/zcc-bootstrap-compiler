#include "zcc_layout_dump.h"
#include "zcc_layout.h"

void zcc_dump_record_layout(FILE *out, Type *type) {
    TypeLayout layout = zcc_get_layout(type, LAYOUT_PHASE_INIT);

    if (!layout.valid)
        return;

    const char *kind = type->kind == TYPE_UNION ? "union" : "struct";

    fprintf(out, "record=%s %s\n", kind, type->name ? type->name : "<anonymous>");
    fprintf(out, "sizeof=%zu\n", layout.size);
    fprintf(out, "alignof=%zu\n", layout.align);

    if (type->kind == TYPE_STRUCT) {
        for (size_t i = 0; i < type->struct_.num_members; ++i) {
            Member *member = &type->struct_.members[i];
            fprintf(out, "field.%s=%zu\n", member->name, member->offset);
        }
    }

    if (type->kind == TYPE_UNION) {
        for (size_t i = 0; i < type->union_.num_members; ++i) {
            Member *member = &type->union_.members[i];
            fprintf(out, "field.%s=0\n", member->name);
        }
    }
}