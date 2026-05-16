"""A2A-Z 64-byte deterministic binary frame — Pythonic wrapper."""
from __future__ import annotations
import ctypes
from dataclasses import dataclass
from enum import IntEnum, IntFlag
from ._native import lib, CZFrame, ZFRAME_SIZE, _check


class ZIntent(IntEnum):
    RESERVED  = 0
    SET_GOAL  = 1
    EXEC      = 2
    STATE     = 3
    HEARTBEAT = 4
    ACK       = 5
    # 16..255 application-defined


class ZFlag(IntFlag):
    PRIORITY = 0x01
    ACK_REQ  = 0x02
    ENC      = 0x04


class ZQoS(IntEnum):
    BEST_EFFORT = 0
    CRITICAL    = 1


def q16_16_from_float(v: float) -> int:
    """Convert float to q16.16 fixed-point (signed 32-bit, low 32 bits of uint64)."""
    q = int(v * 65536.0)
    return q & 0xFFFFFFFFFFFFFFFF


def q16_16_to_float(q: int) -> float:
    # interpret low 32 bits as signed
    low = q & 0xFFFFFFFF
    if low & 0x80000000:
        low -= 1 << 32
    return low / 65536.0


@dataclass
class ZFrame:
    """Logical view of an A2A-Z frame (paper Sec. 4.6, Tab. 1)."""
    intent_id: int = ZIntent.RESERVED
    flags:     int = 0
    qos:       int = ZQoS.BEST_EFFORT
    seq:       int = 0
    ts_us:     int = 0
    src:       int = 0
    dst:       int = 0
    param0:    int = 0
    param1:    int = 0
    param2:    int = 0
    nonce:     int = 0
    tag:       int = 0
    ver:       int = 0x01
    # set by unpack(); ignored on pack()
    crc32:     int = 0

    def pack(self) -> bytes:
        c = self._to_c()
        out = (ctypes.c_uint8 * ZFRAME_SIZE)()
        rc = lib.ta2az_frame_pack(out, ctypes.byref(c))
        _check(rc, "z_frame_pack")
        return bytes(out)

    @classmethod
    def unpack(cls, data: bytes | bytearray) -> "ZFrame":
        if len(data) != ZFRAME_SIZE:
            raise ValueError(f"frame must be exactly {ZFRAME_SIZE} bytes (got {len(data)})")
        in_buf = (ctypes.c_uint8 * ZFRAME_SIZE)(*data)
        c = CZFrame()
        rc = lib.ta2az_frame_unpack(ctypes.byref(c), in_buf)
        if rc == -4:
            raise ValueError("CRC mismatch")
        _check(rc, "z_frame_unpack")
        return cls._from_c(c)

    def _to_c(self) -> CZFrame:
        c = CZFrame()
        c.ver       = self.ver & 0xFF
        c.intent_id = self.intent_id & 0xFF
        c.flags     = self.flags & 0xFF
        c.qos       = self.qos & 0xFF
        c.seq       = self.seq & 0xFFFFFFFF
        c.ts_us     = self.ts_us & 0xFFFFFFFF
        c.src       = self.src & 0xFFFFFFFF
        c.dst       = self.dst & 0xFFFFFFFF
        c.param0    = self.param0 & 0xFFFFFFFFFFFFFFFF
        c.param1    = self.param1 & 0xFFFFFFFFFFFFFFFF
        c.param2    = self.param2 & 0xFFFFFFFFFFFFFFFF
        c.nonce     = self.nonce & 0xFFFFFFFFFFFFFFFF
        c.tag       = self.tag & 0xFFFFFFFFFFFFFFFF
        c.crc32     = 0
        return c

    @classmethod
    def _from_c(cls, c: CZFrame) -> "ZFrame":
        return cls(
            ver=c.ver, intent_id=c.intent_id, flags=c.flags, qos=c.qos,
            seq=c.seq, ts_us=c.ts_us, src=c.src, dst=c.dst,
            param0=c.param0, param1=c.param1, param2=c.param2,
            nonce=c.nonce, tag=c.tag, crc32=c.crc32,
        )

    @staticmethod
    def crc32(data: bytes) -> int:
        """Expose ta2az_crc32 for tests / external use."""
        n = len(data)
        buf = (ctypes.c_uint8 * n)(*data)
        return int(lib.ta2az_crc32(buf, n))
