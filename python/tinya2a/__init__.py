"""TinyA2A — Python bindings for the libtinya2a C reference implementation.

High-level API:
    Envelope   : TinyA2A JSON envelope (build, serialize, parse)
    Journal    : idempotency journal
    Queue      : offline queue
    ZFrame     : A2A-Z 64-byte deterministic binary frame

Low-level access via tinya2a._native if you need to call the C ABI directly.
"""
from .envelope import Envelope, INTENTS
from .journal import Journal
from .queue import Queue
from .frame import ZFrame, ZIntent, ZFlag, ZQoS, q16_16_from_float, q16_16_to_float
from ._native import lib, LIB_VERSION

__all__ = [
    "Envelope", "INTENTS",
    "Journal",
    "Queue",
    "ZFrame", "ZIntent", "ZFlag", "ZQoS", "q16_16_from_float", "q16_16_to_float",
    "lib", "LIB_VERSION",
]
__version__ = "0.1.0"
