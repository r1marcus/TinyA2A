/* frame.c — A2A-Z 64-byte fixed binary frame.
 *
 * Wire layout (little-endian, total 64 bytes):
 *   off  size  field
 *    0    1    ver
 *    1    1    intent_id
 *    2    1    flags
 *    3    1    qos
 *    4    4    seq          (uint32_le)
 *    8    4    ts_us        (uint32_le)
 *   12    4    src          (uint32_le)
 *   16    4    dst          (uint32_le)
 *   20    8    param0       (uint64_le)
 *   28    8    param1       (uint64_le)
 *   36    8    param2       (uint64_le)
 *   44    8    nonce        (uint64_le)
 *   52    8    tag          (uint64_le)   AEAD tag truncated to 8B
 *   60    4    crc32        (over bytes 0..59)
 *
 * Constant-time pack/unpack: no branches on payload, no dynamic alloc.
 */
#include "tinya2a.h"
#include <string.h>

static inline void put_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void put_u64_le(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static inline uint32_t get_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0])       |
           ((uint32_t)p[1] << 8)  |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint64_t get_u64_le(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* CRC32 IEEE 802.3 (poly 0xEDB88320), table-less for compactness.
 * Performance is fine for 60-byte frames. */
uint32_t ta2az_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

ta2a_result_t ta2az_frame_pack(uint8_t out[TA2AZ_FRAME_SIZE],
                               const ta2az_frame_t *f)
{
    if (!out || !f) return TA2A_ERR_INVALID_ARG;
    out[0] = f->ver;
    out[1] = f->intent_id;
    out[2] = f->flags;
    out[3] = f->qos;
    put_u32_le(out + 4,  f->seq);
    put_u32_le(out + 8,  f->ts_us);
    put_u32_le(out + 12, f->src);
    put_u32_le(out + 16, f->dst);
    put_u64_le(out + 20, f->param0);
    put_u64_le(out + 28, f->param1);
    put_u64_le(out + 36, f->param2);
    put_u64_le(out + 44, f->nonce);
    put_u64_le(out + 52, f->tag);
    uint32_t crc = ta2az_crc32(out, 60);
    put_u32_le(out + 60, crc);
    return TA2A_OK;
}

ta2a_result_t ta2az_frame_unpack(ta2az_frame_t *out,
                                 const uint8_t in[TA2AZ_FRAME_SIZE])
{
    if (!out || !in) return TA2A_ERR_INVALID_ARG;
    uint32_t expected = ta2az_crc32(in, 60);
    uint32_t actual   = get_u32_le(in + 60);
    if (expected != actual) return TA2A_ERR_PARSE;
    out->ver       = in[0];
    out->intent_id = in[1];
    out->flags     = in[2];
    out->qos       = in[3];
    out->seq       = get_u32_le(in + 4);
    out->ts_us     = get_u32_le(in + 8);
    out->src       = get_u32_le(in + 12);
    out->dst       = get_u32_le(in + 16);
    out->param0    = get_u64_le(in + 20);
    out->param1    = get_u64_le(in + 28);
    out->param2    = get_u64_le(in + 36);
    out->nonce     = get_u64_le(in + 44);
    out->tag       = get_u64_le(in + 52);
    out->crc32     = actual;
    return TA2A_OK;
}
