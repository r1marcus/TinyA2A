import json
from tinya2a import Envelope


def test_init_defaults():
    e = Envelope(intent="exec_action", src="device://foo")
    s = e.to_json()
    d = json.loads(s)
    assert d["proto"] == "A2A/1.0"
    assert d["intent"] == "exec_action"
    assert d["src"] == "device://foo"
    assert d["qos"] == 0
    assert d["ttl"] == 5000
    assert isinstance(d["id"], str) and len(d["id"]) > 0
    assert isinstance(d["ts"], int) and d["ts"] > 0


def test_payload_object():
    e = Envelope(intent="set_goal", src="cloud://orch", dst="device://board1",
                 payload={"target_yaw_q": 45.0, "deadline_ms": 50})
    s = e.to_json()
    d = json.loads(s)
    assert d["dst"] == "device://board1"
    assert d["payload"]["deadline_ms"] == 50


def test_round_trip_via_c_parser():
    src = Envelope(intent="exec_action", src="dev://a", dst="dev://b",
                   qos=1, seq=42, ttl_ms=2000,
                   idempotency_key="abc123",
                   payload={"action_id": "CLIMB", "duration_ms": 200})
    s = src.to_json()
    parsed = Envelope.from_json(s)
    assert parsed.intent == src.intent
    assert parsed.src == src.src
    assert parsed.dst == src.dst
    assert parsed.qos == src.qos
    assert parsed.seq == src.seq
    assert parsed.idempotency_key == src.idempotency_key
    assert parsed.payload["action_id"] == "CLIMB"


def test_offline_flag_emitted_only_when_true():
    e = Envelope(intent="event", src="dev://x", offline=False)
    assert "offline" not in e.to_json()
    e.offline = True
    assert '"offline":true' in e.to_json()


def test_id_uniqueness():
    ids = {Envelope(intent="event", src="x").to_json() for _ in range(50)}
    # not just 1 id reused
    assert len(ids) > 40
