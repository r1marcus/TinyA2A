## What this changes

<!-- one paragraph -->

## Why

## Test plan

- [ ] `make` builds clean on host
- [ ] `make pytest` passes
- [ ] (if C-side change) verified Cortex-M cross-build still compiles
- [ ] Added/updated tests for new behavior
- [ ] Updated `CHANGELOG.md` if user-visible

## Footprint impact

<!-- "no change" is a valid answer. Otherwise: estimated flash/RAM delta -->

## Checklist

- [ ] No new dynamic allocation in hot paths
- [ ] No new external dependencies beyond stdlib
- [ ] Follows existing style (`-Wall -Wextra -Wpedantic` clean)
