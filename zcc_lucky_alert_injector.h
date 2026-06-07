// zcc_lucky_alert_injector.h — Fully Mapped 125 Alerts — Lucky Skill v777
// ZKAEDI PRIME — Compile with stage3, header order strict

#ifndef ZCC_LUCKY_ALERT_INJECTOR_H
#define ZCC_LUCKY_ALERT_INJECTOR_H

#include <stdint.h>
#include <stddef.h>

#define ALERT_TIP       0x01
#define ALERT_IMPORTANT 0x02
#define ALERT_WARNING   0x04
#define ALERT_NOTE      0x08
#define ALERT_CAUTION   0x10

typedef struct {
    uint8_t level;
    const char* title;
    const char* body;
} ZkaediAlert;

extern const ZkaediAlert zkaedi_alerts[];
extern const size_t zkaedi_alert_count;

void lucky_alert_emit(uint8_t mask, const char* context);

#endif
