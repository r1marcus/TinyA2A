import pytest
from tinya2a import ZFrame, ZIntent, ZFlag, ZQoS, q16_16_from_float, q16_16_to_float


def test_pack_unpack_round_trip():
    f = ZFrame(
        intent_id=ZIntent.SET_GOAL,
        flags=ZFlag.PRIORITY | ZFlag.ACK_REQ,
        qos=ZQoS.CRITICAL,
        seq=12345, ts_us=987654, src=0x11, dst=0x22,
        param0=q16_16_from_float(45.5),
        param1=50, param2=0,
        nonce=0xDEADBEEF, tag=0,
    )
    raw = f.pack()
    assert len(raw) == 64
    out = ZFrame.unpack(raw)
    assert out.intent_id == ZIntent.SET_GOAL
    assert out.flags == ZFlag.PRIORITY | ZFlag.ACK_REQ
    assert out.qos == ZQoS.CRITICAL
    assert out.seq == 12345
    assert out.ts_us == 987654
    assert out.src == 0x11
    assert out.dst == 0x22
    assert q16_16_to_float(out.param0) == pytest.approx(45.5)
    assert out.param1 == 50
    assert out.crc32 != 0


def test_corrupt_frame_rejected():
    f = ZFrame(intent_id=ZIntent.STATE, src=1, dst=2, param0=42)
    raw = bytearray(f.pack())
    raw[20] ^= 0xFF  # corrupt param0 byte
    with pytest.raises(ValueError, match="CRC"):
        ZFrame.unpack(bytes(raw))


def test_q16_16_signed_negative():
    q = q16_16_from_float(-12.25)
    assert q16_16_to_float(q) == pytest.approx(-12.25)


def test_frame_size_constant():
    """Paper claim: every A2A-Z frame is exactly 64 bytes (Sec. 3.6)."""
    for fields in [
        dict(intent_id=ZIntent.SET_GOAL),
        dict(intent_id=ZIntent.STATE, param0=2**63),
        dict(intent_id=ZIntent.HEARTBEAT, ts_us=0xFFFFFFFF),
    ]:
        assert len(ZFrame(**fields).pack()) == 64


def test_crc_helper():
    # known-good CRC32 over "123456789" = 0xCBF43926
    assert ZFrame.crc32(b"123456789") == 0xCBF43926
