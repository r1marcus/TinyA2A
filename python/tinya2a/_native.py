"""Low-level ctypes bindings to libtinya2a."""
from __future__ import annotations
import ctypes
import os
import sys
from pathlib import Path

# Constants must match include/tinya2a.h
ID_LEN              = 33
INTENT_LEN          = 32
URI_LEN             = 96
PAYLOAD_LEN         = 512
ENVELOPE_JSON_MAX   = PAYLOAD_LEN + 512
ZFRAME_SIZE         = 64

LIB_VERSION         = "0.1.0"

# --- Result codes ---
TA2A_OK              =  0
TA2A_ERR_INVALID_ARG = -1
TA2A_ERR_BUFFER_FULL = -2
TA2A_ERR_NOT_FOUND   = -3
TA2A_ERR_PARSE       = -4
TA2A_ERR_NO_MEMORY   = -5
TA2A_ERR_TOO_LONG    = -6


def _find_lib() -> Path:
    here = Path(__file__).resolve()
    candidates = [
        here.parents[2] / "build" / "libtinya2a.dylib",
        here.parents[2] / "build" / "libtinya2a.so",
        Path("/usr/local/lib/libtinya2a.dylib"),
        Path("/usr/local/lib/libtinya2a.so"),
        Path("/usr/lib/libtinya2a.so"),
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(
        "libtinya2a not found. Run `make` in tinya2a/ first. "
        f"Searched: {[str(c) for c in candidates]}"
    )


lib = ctypes.CDLL(str(_find_lib()))


# --- C struct: ta2a_envelope_t ---
class CEnvelope(ctypes.Structure):
    _fields_ = [
        ("id",              ctypes.c_char * ID_LEN),
        ("ts_ms",           ctypes.c_uint64),
        ("intent",          ctypes.c_char * INTENT_LEN),
        ("qos",             ctypes.c_uint8),
        ("ttl_ms",          ctypes.c_uint32),
        ("seq",             ctypes.c_uint64),
        ("src",             ctypes.c_char * URI_LEN),
        ("dst",             ctypes.c_char * URI_LEN),
        ("idempotency_key", ctypes.c_char * ID_LEN),
        ("offline",         ctypes.c_bool),
        ("payload",         ctypes.c_char * PAYLOAD_LEN),
    ]


# --- C struct: ta2az_frame_t ---
class CZFrame(ctypes.Structure):
    _fields_ = [
        ("ver",       ctypes.c_uint8),
        ("intent_id", ctypes.c_uint8),
        ("flags",     ctypes.c_uint8),
        ("qos",       ctypes.c_uint8),
        ("seq",       ctypes.c_uint32),
        ("ts_us",     ctypes.c_uint32),
        ("src",       ctypes.c_uint32),
        ("dst",       ctypes.c_uint32),
        ("param0",    ctypes.c_uint64),
        ("param1",    ctypes.c_uint64),
        ("param2",    ctypes.c_uint64),
        ("nonce",     ctypes.c_uint64),
        ("tag",       ctypes.c_uint64),
        ("crc32",     ctypes.c_uint32),
    ]


# --- Function signatures ---
# helpers
lib.ta2a_now_ms.restype  = ctypes.c_uint64
lib.ta2a_make_id.argtypes = [ctypes.c_char * ID_LEN]
lib.ta2a_make_id.restype  = None

# envelope
lib.ta2a_envelope_init.argtypes = [
    ctypes.POINTER(CEnvelope), ctypes.c_char_p, ctypes.c_char_p
]
lib.ta2a_envelope_init.restype = ctypes.c_int

lib.ta2a_envelope_to_json.argtypes = [
    ctypes.POINTER(CEnvelope), ctypes.c_char_p, ctypes.c_size_t
]
lib.ta2a_envelope_to_json.restype = ctypes.c_int

lib.ta2a_envelope_from_json.argtypes = [
    ctypes.POINTER(CEnvelope), ctypes.c_char_p, ctypes.c_size_t
]
lib.ta2a_envelope_from_json.restype = ctypes.c_int

# journal
lib.ta2a_journal_create.argtypes  = [ctypes.c_size_t]
lib.ta2a_journal_create.restype   = ctypes.c_void_p
lib.ta2a_journal_destroy.argtypes = [ctypes.c_void_p]
lib.ta2a_journal_destroy.restype  = None
lib.ta2a_journal_seen.argtypes    = [ctypes.c_void_p, ctypes.c_char_p]
lib.ta2a_journal_seen.restype     = ctypes.c_bool
lib.ta2a_journal_record.argtypes  = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint64]
lib.ta2a_journal_record.restype   = ctypes.c_int
lib.ta2a_journal_size.argtypes    = [ctypes.c_void_p]
lib.ta2a_journal_size.restype     = ctypes.c_size_t
lib.ta2a_journal_capacity.argtypes = [ctypes.c_void_p]
lib.ta2a_journal_capacity.restype  = ctypes.c_size_t

# queue
lib.ta2a_queue_create.argtypes   = [ctypes.c_size_t]
lib.ta2a_queue_create.restype    = ctypes.c_void_p
lib.ta2a_queue_destroy.argtypes  = [ctypes.c_void_p]
lib.ta2a_queue_destroy.restype   = None
lib.ta2a_queue_push.argtypes     = [ctypes.c_void_p, ctypes.POINTER(CEnvelope)]
lib.ta2a_queue_push.restype      = ctypes.c_int
lib.ta2a_queue_pop.argtypes      = [ctypes.c_void_p, ctypes.POINTER(CEnvelope)]
lib.ta2a_queue_pop.restype       = ctypes.c_int
lib.ta2a_queue_peek.argtypes     = [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(CEnvelope)]
lib.ta2a_queue_peek.restype      = ctypes.c_int
lib.ta2a_queue_size.argtypes     = [ctypes.c_void_p]
lib.ta2a_queue_size.restype      = ctypes.c_size_t
lib.ta2a_queue_capacity.argtypes = [ctypes.c_void_p]
lib.ta2a_queue_capacity.restype  = ctypes.c_size_t
lib.ta2a_queue_clear.argtypes    = [ctypes.c_void_p]
lib.ta2a_queue_clear.restype     = None

# A2A-Z
lib.ta2az_frame_pack.argtypes   = [ctypes.c_uint8 * ZFRAME_SIZE, ctypes.POINTER(CZFrame)]
lib.ta2az_frame_pack.restype    = ctypes.c_int
lib.ta2az_frame_unpack.argtypes = [ctypes.POINTER(CZFrame), ctypes.c_uint8 * ZFRAME_SIZE]
lib.ta2az_frame_unpack.restype  = ctypes.c_int
lib.ta2az_crc32.argtypes        = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
lib.ta2az_crc32.restype         = ctypes.c_uint32


def _check(rc: int, op: str) -> None:
    if rc < 0:
        raise RuntimeError(f"tinya2a: {op} failed with rc={rc}")
