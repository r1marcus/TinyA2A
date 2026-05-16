# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security issue in TinyA2A, **please do not open a public
issue**. Instead, email **m.rueb@foresthub.ai** with:

- A description of the vulnerability and its potential impact.
- Steps to reproduce, or a proof-of-concept if possible.
- Affected versions / commit hashes.
- Your name/handle for credit (optional).

You can expect:

- An acknowledgement within **5 business days**.
- A status update at least every **14 days** until resolution.
- Coordinated disclosure: we agree on a public disclosure date with you,
  typically within 90 days.

## Scope

TinyA2A is a small reference implementation. The security-relevant surface is:

- **Envelope parser** (`src/envelope.c`) — buffer-overflow attacks via
  hostile JSON.
- **Frame parser** (`src/frame.c`) — CRC bypass, malformed inputs.
- **Idempotency journal** — race conditions, key collisions.

Out of scope for *this library* (handle at the application layer):

- Transport-layer security (TLS, DTLS — bring your own).
- AEAD for the A2A-Z frame — the frame has slots for nonce+tag but
  the cipher is application-supplied.
- Authentication / authorization of agents.

## Recognition

Reporters who follow this policy will be credited in the changelog and,
if desired, in a `SECURITY-HALL-OF-FAME.md` file.
