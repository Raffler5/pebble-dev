# pebble-dev

Monorepo for Pebble watchface & app projects targeting **Pebble Time (basalt)** and **Pebble Time 2 (emery)**.

## Projects

| Folder | App | Platforms | Status |
|--------|-----|-----------|--------|
| [`bildschirmli/`](bildschirmli/) | Swiss transit departures (dot-matrix amber style) | basalt + emery | **Active** |
| [`heart-coherence/`](heart-coherence/) | HRV coherence + breathing guide | emery only | Planned |
| [`pt2-bp/`](pt2-bp/) | Cuffless blood pressure classification | emery only | Research |

## Docs

| File | Content |
|------|---------|
| [`docs/PEBBLE_DEV_GUIDE.md`](docs/PEBBLE_DEV_GUIDE.md) | SDK setup, platform specs, do's and don'ts, publishing |
| [`docs/HARDWARE_COMPARISON.md`](docs/HARDWARE_COMPARISON.md) | Pebble Time (2015) vs PT2 (2026) side-by-side specs |
| [`docs/KUMPAN_BLE_KEY.md`](docs/KUMPAN_BLE_KEY.md) | BLE scooter key concept (blocked — needs PebbleOS BLE server API) |

## Platforms

| Codename | Watch | Resolution | Colors | Supported |
|----------|-------|------------|--------|-----------|
| basalt | Pebble Time / Time Steel (2015) | 144x168 | 64 | Yes |
| emery | Pebble Time 2 (2026) | 200x228 | 64 | Yes |

## Dev Environment

Quickest start: [CloudPebble](https://cloudpebble.repebble.com/) (browser IDE, no install).

Local: see [`docs/PEBBLE_DEV_GUIDE.md`](docs/PEBBLE_DEV_GUIDE.md) for SDK setup.
