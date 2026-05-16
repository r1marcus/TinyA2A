"""Echo demo: build TinyA2A envelopes, send via MQTT, parse on receive.

    python3 echo_demo.py [broker]
"""
from __future__ import annotations
import sys, time, json
import paho.mqtt.client as mqtt
from tinya2a import Envelope, Journal, ZFrame, ZIntent, q16_16_from_float

BROKER = sys.argv[1] if len(sys.argv) > 1 else "localhost"
TOPIC  = "a2a/demo/test/echo/agent/down/exec_action"

journal = Journal(capacity=64)


def on_message(client, _, msg):
    try:
        env = Envelope.from_json(msg.payload)
    except Exception as e:
        print(f"  rx: parse error {e}"); return
    print(f"  rx: {env.intent} from {env.src} payload={env.payload}")
    # idempotency check
    if env.idempotency_key:
        if journal.seen(env.idempotency_key):
            print(f"  rx: DUPLICATE (idempotency_key={env.idempotency_key}) — skip")
            return
        journal.record(env.idempotency_key)


def main():
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="tinya2a-echo")
    c.on_message = on_message
    c.connect(BROKER, 1883, 30)
    c.subscribe(TOPIC)
    c.loop_start()

    # build + send three envelopes; second is a duplicate
    e1 = Envelope(intent="exec_action", src="cloud://orch", dst="device://demo",
                  qos=1, idempotency_key="abc-001",
                  payload={"action_id": "BRAKE", "duration_ms": 200})
    e2 = Envelope(intent="exec_action", src="cloud://orch", dst="device://demo",
                  qos=1, idempotency_key="abc-001",
                  payload={"action_id": "BRAKE", "duration_ms": 200})
    e3 = Envelope(intent="report_state", src="device://demo", dst="cloud://orch",
                  payload={"sensor": "yaw", "value": 12.5})

    for env in (e1, e2, e3):
        s = env.to_json()
        print(f"tx: {env.intent} idem={env.idempotency_key or '—'} ({len(s)}B)")
        c.publish(TOPIC, s, qos=env.qos)
        time.sleep(0.5)

    print()
    print("--- A2A-Z deterministic frame demo ---")
    f = ZFrame(intent_id=ZIntent.SET_GOAL,
               seq=1, ts_us=int(time.time()*1e6) & 0xFFFFFFFF,
               src=0x01, dst=0x02,
               param0=q16_16_from_float(45.0),  # heading
               param1=50)                       # deadline ms
    raw = f.pack()
    print(f"tx Z-frame: {len(raw)}B = {raw.hex()}")
    out = ZFrame.unpack(raw)
    print(f"rx Z-frame: intent={out.intent_id} seq={out.seq} src={out.src} dst={out.dst}")

    time.sleep(1.5)
    c.loop_stop()


if __name__ == "__main__":
    main()
