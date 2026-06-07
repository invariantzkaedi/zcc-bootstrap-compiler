import json
import os

def escape_c_string(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')

def main():
    json_path = "constitution_alerts.json"
    c_path = "zcc_lucky_alert_injector.c"
    h_path = "zcc_lucky_alert_injector.h"

    if not os.path.exists(json_path):
        print(f"Error: {json_path} not found.")
        return

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    alerts = data.get("alerts", [])

    # Write .h file
    h_content = """// zcc_lucky_alert_injector.h — Fully Mapped 125 Alerts — Lucky Skill v777
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
"""
    with open(h_path, "w", encoding="utf-8") as f:
        f.write(h_content)
    print(f"Generated {h_path}")

    # Write .c file
    c_header = """// zcc_lucky_alert_injector.c — Supercharged 125-item array — Lucky Coupler 777
// Architect: ZKAEDI PRIME Hamiltonian Flow

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "zcc_lucky_alert_injector.h"
#include "src/zcc_oracle_substrate.h"

const ZkaediAlert zkaedi_alerts[] = {
"""
    
    level_map = {
        "TIP": "ALERT_TIP",
        "IMPORTANT": "ALERT_IMPORTANT",
        "WARNING": "ALERT_WARNING",
        "NOTE": "ALERT_NOTE",
        "CAUTION": "ALERT_CAUTION"
    }

    c_entries = []
    for item in alerts:
        lvl = item.get("level", "TIP")
        macro = level_map.get(lvl, "ALERT_TIP")
        title = escape_c_string(item.get("title", ""))
        body = escape_c_string(item.get("body", ""))
        c_entries.append(f'    {{{macro}, "{title}", "{body}"}}')

    c_body = ",\n".join(c_entries)

    c_footer = """
};

const size_t zkaedi_alert_count = sizeof(zkaedi_alerts) / sizeof(zkaedi_alerts[0]);

void lucky_alert_emit(uint8_t mask, const char* context) {
    for (size_t i = 0; i < zkaedi_alert_count; ++i) {
        if (zkaedi_alerts[i].level & mask) {
            zcc_oracle_log_event(context, zkaedi_alerts[i].title, zkaedi_alerts[i].body);
        }
    }
}
"""

    with open(c_path, "w", encoding="utf-8") as f:
        f.write(c_header + c_body + c_footer)
    print(f"Generated {c_path}")

if __name__ == "__main__":
    main()
