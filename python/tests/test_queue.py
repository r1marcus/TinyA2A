import pytest
from tinya2a import Queue, Envelope


def _env(seq):
    return Envelope(intent="event", src="dev://t", seq=seq, payload={"i": seq})


def test_push_pop_fifo():
    with Queue(capacity=4) as q:
        for i in range(4):
            q.push(_env(i))
        assert len(q) == 4
        outs = [q.pop().seq for _ in range(4)]
        assert outs == [0, 1, 2, 3]
        assert q.pop() is None


def test_overflow():
    with Queue(capacity=2) as q:
        q.push(_env(1))
        q.push(_env(2))
        with pytest.raises(OverflowError):
            q.push(_env(3))


def test_peek_does_not_remove():
    with Queue(capacity=4) as q:
        q.push(_env(10))
        q.push(_env(20))
        assert q.peek(0).seq == 10
        assert q.peek(1).seq == 20
        assert len(q) == 2


def test_clear():
    with Queue(capacity=4) as q:
        for i in range(3): q.push(_env(i))
        q.clear()
        assert len(q) == 0
        assert q.pop() is None


def test_round_trip_payload():
    with Queue(capacity=2) as q:
        e = Envelope(intent="exec_action", src="dev://a",
                     payload={"action_id": "BRAKE", "duration_ms": 100})
        q.push(e)
        out = q.pop()
        assert out.intent == "exec_action"
        assert out.payload["action_id"] == "BRAKE"
        assert out.payload["duration_ms"] == 100
