#ifndef ZCC_ZLD_H
#define ZCC_ZLD_H

/*
 * ZCC Freestanding Static Linker (zld)
 * =================================================================
 * Binds relocatable ELF-64 objects directly into a physical segment.
 */

int zld_link(const char **obj_files, int obj_count, const char *out_path, const char *script_path);

#endif /* ZCC_ZLD_H */
