from tinya2a import Journal


def test_seen_and_record():
    with Journal(capacity=8) as j:
        assert not j.seen("k1")
        j.record("k1")
        assert j.seen("k1")
        assert len(j) == 1
        assert j.capacity == 8


def test_eviction():
    with Journal(capacity=4) as j:
        for i in range(8):
            j.record(f"key{i}")
        # only last 4 retained
        assert j.seen("key7")
        assert j.seen("key6")
        assert j.seen("key5")
        assert j.seen("key4")
        assert not j.seen("key0")
        assert not j.seen("key1")
        assert not j.seen("key2")
        assert not j.seen("key3")


def test_dedup_protects_duplicate_actions():
    """The paper claims <0.1% duplicates with idempotency under 3% loss."""
    with Journal(capacity=128) as j:
        seen_count = 0
        for _ in range(1000):
            key = "action_42"
            if j.seen(key):
                seen_count += 1
            else:
                j.record(key)
        # All but the first call should be detected as duplicate
        assert seen_count == 999
