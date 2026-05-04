/*
 * forgezero_receipt_stub.c — No-op stubs for self-host build.
 *
 * The real forgezero_receipt.c uses time(3) and standard file I/O which
 * are available to ZCC, but we keep the self-hosting build clean by
 * providing zero-dependency stubs here.  These stubs satisfy link
 * requirements when ZCC compiles itself (zcc.c) without requiring
 * forgezero_receipt.c to be in the PARTS concatenation.
 *
 * The real forgezero_receipt.c is compiled separately by GCC and
 * linked into the production binary via PASSES.
 *
 * DEFENSIVE AUDIT SCAFFOLD ONLY — no network, no payload, no exploit.
 */

#include "forgezero_receipt.h"

void fzr_emitter_init(fzr_emitter_t *em,
                      fzr_mode_t     mode,
                      FILE          *fp,
                      int            deterministic)
{
    if (!em) return;
    em->mode          = FZR_MODE_DISABLED;
    em->fp            = (void *)0;
    em->deterministic = deterministic;
    em->event_count   = 0;
    em->target_id[0]  = '\0';
    (void)mode; (void)fp;
}

int fzr_emitter_open_file(fzr_emitter_t *em, const char *path)
{
    (void)em; (void)path;
    return 0;
}

void fzr_emitter_set_target(fzr_emitter_t *em, const char *target_id)
{
    (void)em; (void)target_id;
}

int fzr_emitter_emit(fzr_emitter_t *em, const fzr_event_t *ev)
{
    (void)em; (void)ev;
    return 0;
}

void fzr_emitter_flush(fzr_emitter_t *em) { (void)em; }

void fzr_emitter_close(fzr_emitter_t *em) { (void)em; }

void fzr_event_build(fzr_event_t     *ev,
                     fzr_event_kind_t kind,
                     ir_vuln_tag_t    tag,
                     const char      *component,
                     const char      *target_id,
                     int              deterministic)
{
    (void)ev; (void)kind; (void)tag;
    (void)component; (void)target_id; (void)deterministic;
}

unsigned int fzr_event_hash(const fzr_event_t *ev)
{
    (void)ev;
    return 0u;
}

fzr_severity_t fzr_severity_from_vuln_tag(ir_vuln_tag_t tag)
{
    (void)tag;
    return FZR_SEV_INFO;
}

const char *fzr_severity_to_str(fzr_severity_t sev)
{
    (void)sev;
    return "info";
}

const char *fzr_event_kind_to_str(fzr_event_kind_t kind)
{
    (void)kind;
    return "audit_finding";
}

int fzr_emit_from_vuln_scan(fzr_emitter_t     *em,
                             const ir_module_t *mod)
{
    (void)em; (void)mod;
    return 0;
}
