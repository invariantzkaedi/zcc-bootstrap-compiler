#include "zcc_layout_dump.h"
#include "zcc_layout.h"

void zcc_dump_record_layout(FILE *out, Type *type) {
    TypeLayout layout = zcc_get_layout(type, LAYOUT_PHASE_INIT);

    if (!layout.valid)
        return;

    const char *kind = type->kind == TY_UNION ? "union" : "struct";

    fprintf(out, "record=%s %s\n", kind, type->tag[0] ? type->tag : "<anonymous>");
    fprintf(out, "sizeof=%zu\n", layout.size);
    fprintf(out, "alignof=%zu\n", layout.align);

    if (type->kind == TY_STRUCT) {
        StructField *field = type->fields;
        while (field) {
            fprintf(out, "field.%s=%d\n", field->name, field->offset);
            field = field->next;
        }
    }

    if (type->kind == TY_UNION) {
        StructField *field = type->fields;
        while (field) {
            fprintf(out, "field.%s=0\n", field->name);
            field = field->next;
        }
    }
}
