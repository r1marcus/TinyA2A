/* envelope.c — TinyA2A JSON envelope build / parse helpers */
#define _POSIX_C_SOURCE 200809L
#include "tinya2a.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Portable monotonic-ish millis().
 *
 * Resolution order (first match wins):
 *   1. STM32 HAL          — defined when USE_HAL_DRIVER is set (CubeMX default)
 *                           or when -DTA2A_USE_STM32_HAL is passed.
 *                           Uses HAL_GetTick(); applies to STM32F/L/G/H/U/WB/WL
 *                           Cortex-M targets and STM32MP1/MP2 on the M4/M33.
 *   2. STM32MP Linux      — Cortex-A on OpenSTLinux, falls through to POSIX.
 *   3. CMSIS-RTOS / FreeRTOS — opt in via -DTA2A_USE_FREERTOS, uses xTaskGetTickCount.
 *   4. ESP-IDF             — ESP_PLATFORM defined.
 *   5. Arduino             — ARDUINO defined.
 *   6. POSIX               — Linux / macOS host build.
 *   7. Bare-metal fallback — user provides strong ta2a_port_now_ms() override.
 */
#if defined(USE_HAL_DRIVER) || defined(TA2A_USE_STM32_HAL)
   /* STM32 HAL: include the family-specific header. CubeMX defines
    * STM32F1, STM32F4, STM32L4, STM32G4, STM32H7, STM32U5, STM32WB, etc.
    * If your project doesn't pull in stm32xxxx_hal.h transitively, add
    * -DTA2A_STM32_HAL_HEADER=\"stm32xxxx_hal.h\" or include it before us. */
#  if defined(TA2A_STM32_HAL_HEADER)
#    include TA2A_STM32_HAL_HEADER
#  endif
   /* HAL_GetTick is declared by stm32xxxx_hal.h; declare here as a safety net
    * so the library compiles even if the user includes us before the HAL. */
   extern uint32_t HAL_GetTick(void);
#  define TA2A_NOW_MS_IMPL() ((uint64_t)HAL_GetTick())
#elif defined(TA2A_USE_FREERTOS)
#  include "FreeRTOS.h"
#  include "task.h"
#  define TA2A_NOW_MS_IMPL() ((uint64_t)(xTaskGetTickCount() * (1000U / configTICK_RATE_HZ)))
#elif defined(ESP_PLATFORM) || defined(ESP32)
#  include "esp_timer.h"
#  define TA2A_NOW_MS_IMPL() ((uint64_t)(esp_timer_get_time() / 1000))
#elif defined(ARDUINO)
#  include <Arduino.h>
#  define TA2A_NOW_MS_IMPL() ((uint64_t)millis())
#else
   /* POSIX (Linux / macOS / STM32MP-OpenSTLinux Cortex-A) */
#  include <time.h>
static uint64_t _ta2a_clock_realtime_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}
#  define TA2A_NOW_MS_IMPL() (_ta2a_clock_realtime_ms())
#endif

/* Weak port hook: a target may override this to use any clock source.
 * Provide a strong definition in your project to win the link. */
__attribute__((weak)) uint64_t ta2a_port_now_ms(void) { return TA2A_NOW_MS_IMPL(); }

/* ULID-ish: 32 hex-chars based on time(ms) + random — not cryptographic. */
void ta2a_make_id(char out[TA2A_ID_LEN])
{
    uint64_t ts = ta2a_now_ms();
    static uint32_t state = 0x12345678u;
    /* xorshift32 */
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    uint32_t r1 = state;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    uint32_t r2 = state;
    /* 16 hex (ts, 8 bytes) + 8 hex (r1) + 8 hex (r2) = 32 chars + null */
    snprintf(out, TA2A_ID_LEN,
             "%016llx%08x%08x",
             (unsigned long long)ts, (unsigned)r1, (unsigned)r2);
    out[TA2A_ID_LEN - 1] = '\0';
}

uint64_t ta2a_now_ms(void)
{
    return ta2a_port_now_ms();
}

ta2a_result_t ta2a_envelope_init(ta2a_envelope_t *env,
                                 const char *intent,
                                 const char *src)
{
    if (!env || !intent || !src) return TA2A_ERR_INVALID_ARG;
    memset(env, 0, sizeof(*env));
    ta2a_make_id(env->id);
    env->ts_ms = ta2a_now_ms();
    strncpy(env->intent, intent, TA2A_INTENT_LEN - 1);
    strncpy(env->src,    src,    TA2A_URI_LEN  - 1);
    env->qos    = 0;
    env->ttl_ms = 5000;
    env->seq    = 0;
    env->offline = false;
    strcpy(env->payload, "{}");
    return TA2A_OK;
}

/* Tiny JSON-string escaper. Writes into out (max out_len), returns chars
 * written (excluding null) or negative on overflow. */
static int json_escape(const char *in, char *out, size_t out_len)
{
    size_t i = 0, j = 0;
    if (out_len == 0) return -1;
    while (in[i] && j + 2 < out_len) {
        char c = in[i++];
        if (c == '"' || c == '\\') {
            if (j + 2 >= out_len) return -1;
            out[j++] = '\\'; out[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= out_len) return -1;
            out[j++] = '\\'; out[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= out_len) return -1;
            out[j++] = '\\'; out[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= out_len) return -1;
            out[j++] = '\\'; out[j++] = 't';
        } else if ((unsigned char)c < 0x20) {
            if (j + 6 >= out_len) return -1;
            j += snprintf(out + j, out_len - j, "\\u%04x", (unsigned)c);
        } else {
            out[j++] = c;
        }
    }
    if (j >= out_len) return -1;
    out[j] = '\0';
    return (int)j;
}

int ta2a_envelope_to_json(const ta2a_envelope_t *env, char *out, size_t out_len)
{
    if (!env || !out || out_len < 32) return TA2A_ERR_INVALID_ARG;

    char src_esc[TA2A_URI_LEN * 2 + 8];
    char dst_esc[TA2A_URI_LEN * 2 + 8];
    char int_esc[TA2A_INTENT_LEN * 2 + 8];
    if (json_escape(env->src,    src_esc, sizeof(src_esc)) < 0) return TA2A_ERR_TOO_LONG;
    if (json_escape(env->dst,    dst_esc, sizeof(dst_esc)) < 0) return TA2A_ERR_TOO_LONG;
    if (json_escape(env->intent, int_esc, sizeof(int_esc)) < 0) return TA2A_ERR_TOO_LONG;

    /* payload is assumed to be valid JSON already (object literal, "{}", "null", etc.) */
    const char *payload = env->payload[0] ? env->payload : "{}";

    int n = snprintf(out, out_len,
        "{"
        "\"proto\":\"" TINYA2A_PROTO "\","
        "\"id\":\"%s\","
        "\"ts\":%llu,"
        "\"intent\":\"%s\","
        "\"qos\":%u,"
        "\"ttl\":%u,"
        "\"seq\":%llu,"
        "\"src\":\"%s\","
        "\"dst\":\"%s\","
        "%s%s%s"        /* idempotency_key (optional) */
        "%s"            /* offline flag (optional, only when true) */
        "\"payload\":%s"
        "}",
        env->id,
        (unsigned long long)env->ts_ms,
        int_esc,
        (unsigned)env->qos,
        (unsigned)env->ttl_ms,
        (unsigned long long)env->seq,
        src_esc,
        dst_esc,
        env->idempotency_key[0] ? "\"idempotency_key\":\"" : "",
        env->idempotency_key[0] ? env->idempotency_key   : "",
        env->idempotency_key[0] ? "\","                  : "",
        env->offline ? "\"offline\":true," : "",
        payload);
    if (n < 0 || (size_t)n >= out_len) return TA2A_ERR_TOO_LONG;
    return n;
}

/* ---- Minimal JSON parsing: enough for fixed-shape envelopes ----
 * We do not implement a full JSON parser. We scan known keys and extract
 * their string/number values. Payload is captured as a raw substring so
 * applications can re-parse it with their preferred JSON library. */

static const char *find_key(const char *json, const char *key, size_t json_len)
{
    /* search for "key" allowing surrounding whitespace */
    size_t key_len = strlen(key);
    for (size_t i = 0; i + key_len + 2 < json_len; i++) {
        if (json[i] == '"' && memcmp(json + i + 1, key, key_len) == 0
            && json[i + 1 + key_len] == '"') {
            /* skip past closing quote and colon */
            const char *p = json + i + 2 + key_len;
            while (p < json + json_len && (*p == ' ' || *p == ':' || *p == '\t')) p++;
            return p;
        }
    }
    return NULL;
}

static int parse_str(const char *p, const char *end, char *out, size_t out_len)
{
    if (!p || p >= end || *p != '"') return -1;
    p++;
    size_t j = 0;
    while (p < end && *p != '"') {
        if (j + 1 >= out_len) return -1;
        if (*p == '\\' && p + 1 < end) {
            p++;
            switch (*p) {
                case 'n': out[j++] = '\n'; break;
                case 'r': out[j++] = '\r'; break;
                case 't': out[j++] = '\t'; break;
                case '"': out[j++] = '"';  break;
                case '\\': out[j++] = '\\'; break;
                default: out[j++] = *p; break;
            }
            p++;
        } else {
            out[j++] = *p++;
        }
    }
    if (p >= end) return -1;
    out[j] = '\0';
    return (int)j;
}

static int parse_uint(const char *p, const char *end, uint64_t *out)
{
    if (!p || p >= end) return -1;
    while (p < end && isspace((unsigned char)*p)) p++;
    uint64_t v = 0;
    int n = 0;
    while (p < end && isdigit((unsigned char)*p)) {
        v = v * 10 + (uint64_t)(*p - '0');
        p++; n++;
    }
    if (n == 0) return -1;
    *out = v;
    return n;
}

/* Capture a JSON value (object/string/number/etc.) as a raw substring. */
static int parse_value_raw(const char *p, const char *end, char *out, size_t out_len)
{
    if (!p || p >= end) return -1;
    while (p < end && isspace((unsigned char)*p)) p++;
    const char *start = p;
    int depth = 0;
    bool in_str = false;
    bool esc = false;
    while (p < end) {
        char c = *p;
        if (in_str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
        } else {
            if (c == '"') in_str = true;
            else if (c == '{' || c == '[') depth++;
            else if (c == '}' || c == ']') {
                if (depth == 0) break;
                depth--;
            } else if ((c == ',' || isspace((unsigned char)c)) && depth == 0) {
                break;
            }
        }
        p++;
    }
    size_t n = (size_t)(p - start);
    if (n + 1 > out_len) return -1;
    memcpy(out, start, n);
    out[n] = '\0';
    return (int)n;
}

ta2a_result_t ta2a_envelope_from_json(ta2a_envelope_t *env,
                                      const char *json, size_t json_len)
{
    if (!env || !json) return TA2A_ERR_INVALID_ARG;
    memset(env, 0, sizeof(*env));
    const char *end = json + json_len;
    const char *p;
    uint64_t u;

    p = find_key(json, "id", json_len);
    if (p && parse_str(p, end, env->id, TA2A_ID_LEN) < 0) return TA2A_ERR_PARSE;

    p = find_key(json, "ts", json_len);
    if (p && parse_uint(p, end, &u) > 0) env->ts_ms = u;

    p = find_key(json, "intent", json_len);
    if (p && parse_str(p, end, env->intent, TA2A_INTENT_LEN) < 0) return TA2A_ERR_PARSE;

    p = find_key(json, "qos", json_len);
    if (p && parse_uint(p, end, &u) > 0) env->qos = (uint8_t)u;

    p = find_key(json, "ttl", json_len);
    if (p && parse_uint(p, end, &u) > 0) env->ttl_ms = (uint32_t)u;

    p = find_key(json, "seq", json_len);
    if (p && parse_uint(p, end, &u) > 0) env->seq = u;

    p = find_key(json, "src", json_len);
    if (p && parse_str(p, end, env->src, TA2A_URI_LEN) < 0) return TA2A_ERR_PARSE;

    p = find_key(json, "dst", json_len);
    if (p && parse_str(p, end, env->dst, TA2A_URI_LEN) < 0) return TA2A_ERR_PARSE;

    p = find_key(json, "idempotency_key", json_len);
    if (p && parse_str(p, end, env->idempotency_key, TA2A_ID_LEN) < 0) return TA2A_ERR_PARSE;

    p = find_key(json, "offline", json_len);
    if (p) {
        while (p < end && isspace((unsigned char)*p)) p++;
        env->offline = (p < end && *p == 't');
    }

    p = find_key(json, "payload", json_len);
    if (p) {
        if (parse_value_raw(p, end, env->payload, TA2A_PAYLOAD_LEN) < 0)
            return TA2A_ERR_PARSE;
    } else {
        strcpy(env->payload, "{}");
    }
    return TA2A_OK;
}
