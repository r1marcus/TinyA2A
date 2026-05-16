"""Offline envelope queue — Pythonic wrapper."""
from __future__ import annotations
import ctypes
from ._native import lib, CEnvelope, TA2A_OK, _check
from .envelope import Envelope


class Queue:
    """Fixed-capacity ring buffer of Envelopes (paper Sec. 4.5)."""

    def __init__(self, capacity: int = 16):
        if capacity <= 0:
            raise ValueError("capacity must be > 0")
        self._handle = lib.ta2a_queue_create(capacity)
        if not self._handle:
            raise MemoryError("queue_create failed")

    def __del__(self):
        try: self.close()
        except Exception: pass

    def close(self):
        if self._handle:
            lib.ta2a_queue_destroy(self._handle)
            self._handle = None

    def __enter__(self): return self
    def __exit__(self, *a): self.close()

    def push(self, env: Envelope) -> None:
        c = env._to_c()
        rc = lib.ta2a_queue_push(self._handle, ctypes.byref(c))
        if rc == -2:
            raise OverflowError("queue is full")
        _check(rc, "queue_push")

    def pop(self) -> Envelope | None:
        c = CEnvelope()
        rc = lib.ta2a_queue_pop(self._handle, ctypes.byref(c))
        if rc == -3:  # NOT_FOUND
            return None
        _check(rc, "queue_pop")
        return Envelope._from_c(c)

    def peek(self, i: int = 0) -> Envelope | None:
        c = CEnvelope()
        rc = lib.ta2a_queue_peek(self._handle, i, ctypes.byref(c))
        if rc == -3:
            return None
        _check(rc, "queue_peek")
        return Envelope._from_c(c)

    def clear(self) -> None:
        lib.ta2a_queue_clear(self._handle)

    def __len__(self) -> int:
        return lib.ta2a_queue_size(self._handle)

    @property
    def capacity(self) -> int:
        return lib.ta2a_queue_capacity(self._handle)
