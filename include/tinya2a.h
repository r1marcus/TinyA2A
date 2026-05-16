/* tinya2a.h — TinyA2A + A2A-Z reference C library
 *
 * Two profiles of the A2A intent vocabulary, sharing a single intent model:
 *   - TinyA2A : JSON envelope, MQTT/HTTP, idempotency journal, offline queue, delta-sync
 *   - A2A-Z   : 64-byte fixed binary frame, deterministic L2 (CAN-FD/TSN/UWB)
 *
 * Design constraints:
 *   - No dynamic allocation in the deterministic path.
 *   - Fixed-size structs with compile-time tunable buffers.
 *   - Caller-owned memory; library uses opaque handles only for journal/queue.
 *
 * Apache License 2.0.
 */
#ifndef TINYA2A_H
#define TINYA2A_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Build-time configuration ---- */
#ifndef TA2A_ID_LEN
#define TA2A_ID_LEN          33      /* ULID/UUID-style + null */
#endif
#ifndef TA2A_INTENT_LEN
#define TA2A_INTENT_LEN      32
#endif
#ifndef TA2A_URI_LEN
#define TA2A_URI_LEN         96
#endif
#ifndef TA2A_PAYLOAD_LEN
#define TA2A_PAYLOAD_LEN     512
#endif
#ifndef TA2A_ENVELOPE_JSON_MAX
#define TA2A_ENVELOPE_JSON_MAX (TA2A_PAYLOAD_LEN + 512)
#endif

/* ---- Version ---- */
#define TINYA2A_PROTO        "A2A/1.0"
#define TINYA2A_LIB_VERSION  "0.1.0"

/* ---- Result codes ---- */
typedef enum {
    TA2A_OK              = 0,
    TA2A_ERR_INVALID_ARG = -1,
    TA2A_ERR_BUFFER_FULL = -2,
    TA2A_ERR_NOT_FOUND   = -3,
    TA2A_ERR_PARSE       = -4,
    TA2A_ERR_NO_MEMORY   = -5,
    TA2A_ERR_TOO_LONG    = -6,
} ta2a_result_t;

/* ============================================================
 * TinyA2A — JSON envelope + helpers
 * ============================================================ */

/* Standard A2A intents (Sec. 3.2 of paper) — applications may use custom strings. */
#define TA2A_INTENT_SET_GOAL        "set_goal"
#define TA2A_INTENT_EXEC_ACTION     "exec_action"
#define TA2A_INTENT_CANCEL_GOAL     "cancel_goal"
#define TA2A_INTENT_REPORT_STATE    "report_state"
#define TA2A_INTENT_EVENT           "event"
#define TA2A_INTENT_REGISTER_AGENT  "register_agent"
#define TA2A_INTENT_HEARTBEAT       "heartbeat"
#define TA2A_INTENT_ADVERTISE_CAP   "advertise_cap"
#define TA2A_INTENT_SYNC_REQ        "sync_req"
#define TA2A_INTENT_SYNC_DELTA      "sync_delta"
#define TA2A_INTENT_SYNC_ACK        "sync_ack"

typedef struct {
    char     id[TA2A_ID_LEN];                  /* ULID-like unique id */
    uint64_t ts_ms;                            /* unix ms */
    char     intent[TA2A_INTENT_LEN];
    uint8_t  qos;                              /* 0 or 1 */
    uint32_t ttl_ms;                           /* 0 = no ttl */
    uint64_t seq;                              /* per-stream sequence */
    char     src[TA2A_URI_LEN];                /* sender URI */
    char     dst[TA2A_URI_LEN];                /* receiver URI ("" allowed) */
    char     idempotency_key[TA2A_ID_LEN];     /* "" if unused */
    bool     offline;                          /* sent while offline */
    /* payload is JSON object source as a string. The library does not parse it.
     * Apps embed any JSON they want here, or "{}" for empty. */
    char     payload[TA2A_PAYLOAD_LEN];
} ta2a_envelope_t;

/* Initialize an envelope with sane defaults: fresh id, current ts, qos 0,
 * ttl 5000 ms. Caller fills in intent/src/dst/payload. */
ta2a_result_t ta2a_envelope_init(ta2a_envelope_t *env,
                                 const char *intent,
                                 const char *src);

/* Serialize an envelope to JSON. Returns bytes written (>0) or negative error. */
int ta2a_envelope_to_json(const ta2a_envelope_t *env,
                          char *out, size_t out_len);

/* Parse a JSON string into envelope. Returns TA2A_OK or negative on error. */
ta2a_result_t ta2a_envelope_from_json(ta2a_envelope_t *env,
                                      const char *json, size_t json_len);

/* Helpers */
uint64_t ta2a_now_ms(void);
void     ta2a_make_id(char out[TA2A_ID_LEN]);

/* ============================================================
 * Idempotency Journal — fixed-capacity ring of seen keys
 * ============================================================ */
typedef struct ta2a_journal ta2a_journal_t;

ta2a_journal_t *ta2a_journal_create(size_t capacity);
void            ta2a_journal_destroy(ta2a_journal_t *j);
bool            ta2a_journal_seen(ta2a_journal_t *j, const char *key);
ta2a_result_t   ta2a_journal_record(ta2a_journal_t *j,
                                    const char *key, uint64_t ts_ms);
size_t          ta2a_journal_size(const ta2a_journal_t *j);
size_t          ta2a_journal_capacity(const ta2a_journal_t *j);

/* ============================================================
 * Offline Queue — ring buffer of envelopes
 * ============================================================ */
typedef struct ta2a_queue ta2a_queue_t;

ta2a_queue_t  *ta2a_queue_create(size_t capacity);
void           ta2a_queue_destroy(ta2a_queue_t *q);
ta2a_result_t  ta2a_queue_push(ta2a_queue_t *q, const ta2a_envelope_t *env);
ta2a_result_t  ta2a_queue_pop(ta2a_queue_t *q, ta2a_envelope_t *out);
ta2a_result_t  ta2a_queue_peek(const ta2a_queue_t *q, size_t i, ta2a_envelope_t *out);
size_t         ta2a_queue_size(const ta2a_queue_t *q);
size_t         ta2a_queue_capacity(const ta2a_queue_t *q);
void           ta2a_queue_clear(ta2a_queue_t *q);

/* ============================================================
 * A2A-Z — fixed 64-byte binary frame
 * ============================================================ */
#define TA2AZ_FRAME_SIZE 64

/* z_intent_id values (Sec. 3.6 of paper). 0..15 reserved for A2A-Z core. */
typedef enum {
    TA2AZ_INTENT_RESERVED   = 0,
    TA2AZ_INTENT_SET_GOAL   = 1,
    TA2AZ_INTENT_EXEC       = 2,
    TA2AZ_INTENT_STATE      = 3,
    TA2AZ_INTENT_HEARTBEAT  = 4,
    TA2AZ_INTENT_ACK        = 5,
    /* 16..255 application-defined */
} ta2az_intent_id_t;

/* Flags */
#define TA2AZ_FLAG_PRIORITY  0x01
#define TA2AZ_FLAG_ACK_REQ   0x02
#define TA2AZ_FLAG_ENC       0x04   /* AEAD on (tag valid) */

/* QoS classes */
#define TA2AZ_QOS_BEST_EFFORT 0
#define TA2AZ_QOS_CRITICAL    1

/* Logical frame fields. The wire layout is fixed (see ta2az_frame_pack). */
typedef struct {
    uint8_t  ver;          /* 0x01 */
    uint8_t  intent_id;
    uint8_t  flags;
    uint8_t  qos;
    uint32_t seq;
    uint32_t ts_us;
    uint32_t src;
    uint32_t dst;
    uint64_t param0;
    uint64_t param1;
    uint64_t param2;
    uint64_t nonce;        /* AEAD nonce or 0 */
    uint64_t tag;          /* AEAD tag truncated to 8B, or 0 */
    uint32_t crc32;        /* set by pack(), validated by unpack() */
} ta2az_frame_t;

/* Pack a logical frame into 64 bytes (little-endian). Computes CRC32.
 * out must be 64 bytes. Returns TA2A_OK or error. */
ta2a_result_t ta2az_frame_pack(uint8_t out[TA2AZ_FRAME_SIZE],
                               const ta2az_frame_t *frame);

/* Unpack 64 bytes into logical frame. Validates CRC32. */
ta2a_result_t ta2az_frame_unpack(ta2az_frame_t *out,
                                 const uint8_t in[TA2AZ_FRAME_SIZE]);

/* CRC32 (IEEE 802.3 polynomial 0xEDB88320). Public for tests/extensions. */
uint32_t ta2az_crc32(const uint8_t *data, size_t len);

/* q16.16 fixed-point helpers for numeric params on the deterministic path. */
static inline int32_t  ta2a_q16_16_from_float(double v) { return (int32_t)(v * 65536.0); }
static inline double   ta2a_q16_16_to_float(int32_t q)  { return (double)q / 65536.0; }

#ifdef __cplusplus
}
#endif
#endif /* TINYA2A_H */
