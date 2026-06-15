#include "zcc_layout.h"
#include "zcc_diagnostics.h"
#include <stdio.h>
#include <stdarg.h>

Compiler *g_cc = 0;

void zcc_diag(DiagLevel level, ErrorCode code, int phase, SourceLoc loc, const char *fmt, ...) {
    char *name = 0;
    int line = loc;
    if (g_cc) {
        name = g_cc->filename;
        if (line <= 0) {
            line = g_cc->tk_line;
        }
    }
    if (!name) name = "<input>";

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: error: ", name, line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, " (ErrorCode=%d)\n", code);
    va_end(ap);

    if (level == DIAG_ERROR && g_cc) {
        g_cc->errors++;
    }
}

TypeLayout zcc_get_layout(Type *type, LayoutPhase phase) {
    TypeLayout layout;
    layout.size = 0;
    layout.align = 1;
    layout.padded_size = 0;
    layout.valid = false;

    if (!type) {
        return layout;
    }

    #define MAX_VISITED 1024
    static Type *visited_stack[MAX_VISITED];
    static int visited_depth = 0;

    for (int i = 0; i < visited_depth; i++) {
        if (visited_stack[i] == type) {
            zcc_diag(DIAG_ERROR, E_LAYOUT_RECURSIVE_TYPE, phase, 0, "recursive type definition");
            return layout;
        }
    }

    switch (type->kind) {
        case TY_VOID:
            layout.size = 0;
            layout.align = 1;
            layout.padded_size = 0;
            layout.valid = true;
            return layout;

        case TY_CHAR:
        case TY_UCHAR:
            layout.size = 1;
            layout.align = 1;
            layout.padded_size = 1;
            layout.valid = true;
            return layout;

        case TY_SHORT:
        case TY_USHORT:
            layout.size = 2;
            layout.align = 2;
            layout.padded_size = 2;
            layout.valid = true;
            return layout;

        case TY_INT:
        case TY_UINT:
        case TY_ENUM:
            layout.size = 4;
            layout.align = 4;
            layout.padded_size = 4;
            layout.valid = true;
            return layout;

        case TY_LONG:
        case TY_ULONG:
        case TY_LONGLONG:
        case TY_ULONGLONG:
        case TY_DOUBLE:
        case TY_PTR:
        case TY_FUNC:
            layout.size = 8;
            layout.align = 8;
            layout.padded_size = 8;
            layout.valid = true;
            return layout;

        case TY_FLOAT:
            layout.size = 4;
            layout.align = 4;
            layout.padded_size = 4;
            layout.valid = true;
            return layout;

        case TY_ARRAY: {
            TypeLayout base_layout = zcc_get_layout(type->base, phase);
            if (!base_layout.valid) {
                return layout;
            }
            size_t elem_size = base_layout.padded_size;
            size_t count = type->array_len;
            size_t total_size = 0;
            if (count > 0 && elem_size > 0) {
                if (count > (size_t)-1 / elem_size) {
                    zcc_diag(DIAG_ERROR, E_LAYOUT_SIZE_OVERFLOW, phase, 0, "array size overflow");
                    return layout;
                }
                total_size = elem_size * count;
            }
            layout.size = total_size;
            layout.align = base_layout.align;
            layout.padded_size = total_size;
            layout.valid = true;
            return layout;
        }

        case TY_STRUCT:
        case TY_UNION: {
            if (!type->fields && !type->is_complete) {
                zcc_diag(DIAG_ERROR, E_LAYOUT_INCOMPLETE_TYPE, phase, 0, "incomplete type layout");
                return layout;
            }

            if (visited_depth >= MAX_VISITED) {
                zcc_diag(DIAG_ERROR, E_LAYOUT_SIZE_OVERFLOW, phase, 0, "recursion depth exceeded");
                return layout;
            }
            visited_stack[visited_depth++] = type;

            bool is_union = (type->kind == TY_UNION);
            size_t offset = 0;
            size_t max_size = 0;
            size_t max_align = 1;

            int bf_active = 0;
            int bf_unit_size = 0;
            int bf_current_bit = 0;
            int bf_unit_offset = 0;

            StructField *field = type->fields;
            while (field) {
                if (field->type->kind == TY_VOID) {
                    zcc_diag(DIAG_ERROR, E_LAYOUT_INCOMPLETE_TYPE, phase, 0, "void struct member is invalid");
                    visited_depth--;
                    return layout;
                }

                TypeLayout f_layout = zcc_get_layout(field->type, phase);
                if (!f_layout.valid) {
                    visited_depth--;
                    return layout;
                }

                size_t falign = f_layout.align;
                if (type->is_packed) {
                    falign = 1;
                }

                if (falign > 4096) {
                    zcc_diag(DIAG_ERROR, E_ALIGNAS_INVALID, phase, 0, "invalid alignment specification");
                    visited_depth--;
                    return layout;
                }

                if (falign > max_align) {
                    max_align = falign;
                }

                if (is_union) {
                    field->offset = 0;
                    if (f_layout.padded_size > max_size) {
                        max_size = f_layout.padded_size;
                    }
                } else {
                    if (field->is_bitfield) {
                        int bf_size = field->bit_size;
                        int fsize = f_layout.padded_size;
                        if (bf_active && fsize == bf_unit_size && bf_size > 0 && bf_current_bit + bf_size <= fsize * 8) {
                            field->offset = bf_unit_offset;
                            field->bit_offset = bf_current_bit;
                            bf_current_bit += bf_size;
                        } else {
                            bf_active = 1;
                            bf_unit_size = fsize;
                            bf_current_bit = 0;
                            if (falign > 1) {
                                size_t old_offset = offset;
                                offset = (offset + falign - 1) & ~(falign - 1);
                                if (offset < old_offset) {
                                    zcc_diag(DIAG_ERROR, E_LAYOUT_SIZE_OVERFLOW, phase, 0, "offset calculation overflow");
                                    visited_depth--;
                                    return layout;
                                }
                            }
                            bf_unit_offset = offset;
                            if (bf_size > 0) {
                                field->offset = bf_unit_offset;
                                field->bit_offset = 0;
                                bf_current_bit = bf_size;
                                offset += fsize;
                            } else {
                                bf_active = 0;
                                bf_unit_size = 0;
                                bf_current_bit = 0;
                            }
                        }
                    } else {
                        bf_active = 0;
                        if (falign > 1) {
                            size_t old_offset = offset;
                            offset = (offset + falign - 1) & ~(falign - 1);
                            if (offset < old_offset) {
                                zcc_diag(DIAG_ERROR, E_LAYOUT_SIZE_OVERFLOW, phase, 0, "offset calculation overflow");
                                visited_depth--;
                                return layout;
                            }
                        }
                        field->offset = offset;
                        if (offset > (size_t)-1 - f_layout.padded_size) {
                            zcc_diag(DIAG_ERROR, E_LAYOUT_SIZE_OVERFLOW, phase, 0, "struct size overflow");
                            visited_depth--;
                            return layout;
                        }
                        offset = offset + f_layout.padded_size;
                    }
                }
                field = field->next;
            }

            size_t struct_size = is_union ? max_size : offset;
            size_t final_align = max_align;
            if (type->explicit_align > 0) {
                final_align = type->explicit_align;
            } else if (type->is_packed) {
                final_align = 1;
            }

            if (final_align > 1) {
                size_t old_size = struct_size;
                struct_size = (struct_size + final_align - 1) & ~(final_align - 1);
                if (struct_size < old_size) {
                    zcc_diag(DIAG_ERROR, E_LAYOUT_SIZE_OVERFLOW, phase, 0, "struct padding size overflow");
                    visited_depth--;
                    return layout;
                }
            }

            layout.size = struct_size;
            layout.align = final_align;
            layout.padded_size = struct_size;
            layout.valid = true;

            visited_depth--;
            return layout;
        }

        default:
            zcc_diag(DIAG_ERROR, E_LAYOUT_UNKNOWN_KIND, phase, 0, "unknown type kind");
            return layout;
    }
}

size_t zcc_sizeof(Type *type) {
    TypeLayout layout = zcc_get_layout(type, LAYOUT_PHASE_SIZEOF);
    if (layout.valid) {
        return layout.size;
    }
    return 8;
}

size_t zcc_alignof(Type *type) {
    TypeLayout layout = zcc_get_layout(type, LAYOUT_PHASE_ALIGNOF);
    if (layout.valid) {
        return layout.align;
    }
    return 8;
}
