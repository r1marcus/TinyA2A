"""TinyA2A envelope — Pythonic wrapper around the C ta2a_envelope_t."""
from __future__ import annotations
import ctypes
import json as _json
from dataclasses import dataclass, field
from typing import Optional, Any
from ._native import (
    lib, CEnvelope,
    ID_LEN, INTENT_LEN, URI_LEN, PAYLOAD_LEN, ENVELOPE_JSON_MAX,
    TA2A_OK, _check,
)


# Standard intent constants
INTENTS = {
    "set_goal":        "set_goal",
    "exec_action":     "exec_action",
    "cancel_goal":     "cancel_goal",
    "report_state":    "report_state",
    "event":           "event",
    "register_agent":  "register_agent",
    "heartbeat":       "heartbeat",
    "advertise_cap":   "advertise_cap",
    "sync_req":        "sync_req",
    "sync_delta":      "sync_delta",
    "sync_ack":        "sync_ack",
}


@dataclass
class Envelope:
    """TinyA2A envelope (paper Sec. 4.2)."""
    intent: str
    src:    str
    dst:    str = ""
    qos:    int = 0
    ttl_ms: int = 5000
    seq:    int = 0
    payload: Any = field(default_factory=dict)
    idempotency_key: str = ""
    offline: bool = False
    id:     str = ""
    ts_ms:  int = 0

    # ---------- C struct round-trip ----------
    def _to_c(self) -> CEnvelope:
        c = CEnvelope()
        # let the C side seed defaults
        rc = lib.ta2a_envelope_init(ctypes.byref(c),
                                    self.intent.encode("utf-8"),
                                    self.src.encode("utf-8"))
        _check(rc, "envelope_init")
        # caller-supplied fields override
        if self.id:    c.id    = self.id.encode("utf-8")[:ID_LEN-1]
        if self.ts_ms: c.ts_ms = self.ts_ms
        c.dst    = self.dst.encode("utf-8")[:URI_LEN-1]
        c.qos    = self.qos & 0xFF
        c.ttl_ms = self.ttl_ms & 0xFFFFFFFF
        c.seq    = self.seq & 0xFFFFFFFFFFFFFFFF
        c.idempotency_key = self.idempotency_key.encode("utf-8")[:ID_LEN-1]
        c.offline = bool(self.offline)
        # payload becomes JSON string
        pj = self.payload if isinstance(self.payload, str) else _json.dumps(self.payload, separators=(",", ":"))
        if len(pj.encode("utf-8")) >= PAYLOAD_LEN:
            raise ValueError(f"payload exceeds {PAYLOAD_LEN-1} bytes")
        c.payload = pj.encode("utf-8")
        return c

    @classmethod
    def _from_c(cls, c: CEnvelope) -> "Envelope":
        try:
            payload_str = c.payload.decode("utf-8")
            payload = _json.loads(payload_str) if payload_str.strip() else {}
        except Exception:
            payload = c.payload.decode("utf-8", errors="replace")
        return cls(
            intent=c.intent.decode("utf-8"),
            src=c.src.decode("utf-8"),
            dst=c.dst.decode("utf-8"),
            qos=int(c.qos),
            ttl_ms=int(c.ttl_ms),
            seq=int(c.seq),
            payload=payload,
            idempotency_key=c.idempotency_key.decode("utf-8"),
            offline=bool(c.offline),
            id=c.id.decode("utf-8"),
            ts_ms=int(c.ts_ms),
        )

    # ---------- JSON ----------
    def to_json(self) -> str:
        c = self._to_c()
        # if id/ts not set, the C init filled them; mirror back
        if not self.id:    self.id    = c.id.decode("utf-8")
        if not self.ts_ms: self.ts_ms = int(c.ts_ms)
        out = ctypes.create_string_buffer(ENVELOPE_JSON_MAX)
        n = lib.ta2a_envelope_to_json(ctypes.byref(c), out, ENVELOPE_JSON_MAX)
        _check(n, "envelope_to_json")
        return out.value.decode("utf-8")

    @classmethod
    def from_json(cls, s: str | bytes) -> "Envelope":
        if isinstance(s, str):
            s = s.encode("utf-8")
        c = CEnvelope()
        rc = lib.ta2a_envelope_from_json(ctypes.byref(c), s, len(s))
        _check(rc, "envelope_from_json")
        return cls._from_c(c)

    # ---------- convenience ----------
    def __repr__(self) -> str:
        return (f"Envelope(intent={self.intent!r}, src={self.src!r}, "
                f"dst={self.dst!r}, seq={self.seq}, qos={self.qos}, "
                f"id={self.id[:12]+'..' if len(self.id)>12 else self.id})")
