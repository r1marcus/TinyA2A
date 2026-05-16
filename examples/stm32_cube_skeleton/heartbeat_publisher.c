/* heartbeat_publisher.c — drop into a CubeMX-generated STM32 project.
 *
 * Publishes a TinyA2A `heartbeat` envelope every second over UART (or any
 * transport you swap in). Demonstrates the minimal call sequence; no MQTT
 * client / TLS stack required to compile this file.
 *
 * Wire it into Core/Src/main.c:
 *   - Include "tinya2a.h" at the top.
 *   - Call ta2a_init_heartbeat() once after HAL_Init().
 *   - Call ta2a_tick_heartbeat() from the SysTick / 1 Hz task.
 *
 * CubeMX defines USE_HAL_DRIVER, so the library auto-uses HAL_GetTick().
 */
#include "tinya2a.h"
#include <stdio.h>

/* Replace with your transport. */
extern int my_uart_puts(const char *s);   /* user-provided */

#define BOARD_URI "device://nucleo-h753"

static ta2a_journal_t *s_journal = NULL;
static uint32_t        s_last_tick = 0;
static uint64_t        s_seq = 0;

void ta2a_init_heartbeat(void) {
    /* Optional: pre-allocate journal so we can dedupe inbound commands.
     * Drop this if you only publish (no inbound idempotency needed). */
    s_journal = ta2a_journal_create(32);
}

void ta2a_tick_heartbeat(void) {
    uint32_t now = (uint32_t)ta2a_now_ms();
    if ((now - s_last_tick) < 1000) return;        /* 1 Hz */
    s_last_tick = now;

    ta2a_envelope_t e;
    ta2a_envelope_init(&e, TA2A_INTENT_HEARTBEAT, BOARD_URI);
    e.qos = 0;
    e.seq = ++s_seq;
    /* tiny payload, fits well inside TA2A_PAYLOAD_LEN */
    snprintf(e.payload, sizeof e.payload,
             "{\"uptime_ms\":%lu}", (unsigned long)now);

    char buf[TA2A_ENVELOPE_JSON_MAX];
    int n = ta2a_envelope_to_json(&e, buf, sizeof buf);
    if (n > 0) {
        my_uart_puts(buf);
        my_uart_puts("\r\n");
    }
}

/* Inbound (e.g. parsed from MQTT/UART): demonstrate idempotency check. */
int ta2a_handle_inbound(const char *json, size_t len) {
    ta2a_envelope_t e;
    if (ta2a_envelope_from_json(&e, json, len) != TA2A_OK) return -1;

    if (s_journal && e.idempotency_key[0] &&
        ta2a_journal_seen(s_journal, e.idempotency_key)) {
        return 0;                                  /* duplicate, suppress */
    }
    /* TODO: dispatch on e.intent ... */
    if (s_journal && e.idempotency_key[0]) {
        ta2a_journal_record(s_journal, e.idempotency_key, e.ts_ms);
    }
    return 1;                                      /* processed */
}
