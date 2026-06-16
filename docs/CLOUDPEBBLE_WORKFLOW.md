# CloudPebble Workflow

How to develop Pebble apps with GitHub + CloudPebble.

---

## CloudPebble Expects Root-Level Projects

CloudPebble imports from GitHub expect `appinfo.json` and `src/` at the **repository root**. It cannot import a subfolder from a monorepo.

**Consequence:** We maintain two repos:

| Repo | Purpose | Structure |
|------|---------|-----------|
| `Raffler5/pebble-dev` | Monorepo: all projects + docs | `bildschirmli/`, `docs/`, etc. |
| `Raffler5/bildschirmli-pebble` | CloudPebble build repo | `appinfo.json` at root |

### Sync Workflow

After editing in the monorepo:

```bash
# From the CloudPebble repo directory
cd /config/Desktop/bildschirmli-pebble
cp /config/Desktop/pebble-dev/bildschirmli/src/c/*.{c,h} src/c/
cp /config/Desktop/pebble-dev/bildschirmli/src/js/app.js src/js/
cp /config/Desktop/pebble-dev/bildschirmli/appinfo.json .
git add -A && git commit -m "Sync from monorepo" && git push
```

Then in CloudPebble: pull from GitHub and rebuild.

### .gitignore in Monorepo

The CloudPebble repo is cloned inside the monorepo directory. Add it to `.gitignore`:

```
bildschirmli-pebble/
```

---

## CloudPebble Import Steps

1. Go to [cloudpebble.repebble.com](https://cloudpebble.repebble.com/)
2. **Import** → **Import from GitHub**
3. Select `Raffler5/bildschirmli-pebble`
4. CloudPebble reads `appinfo.json` and sets up the project
5. **Build** (Ctrl+B or top-right button)
6. **Run** → select emulator (basalt or emery)

### Emulator Limitations

- No phone GPS → station search returns "GPS unavailable"
- No Bluetooth → AppMessage doesn't work
- To test full data flow: install on a real watch paired with a phone

### Emulator Platforms

| Platform | Watch | Use for |
|----------|-------|---------|
| **basalt** | Pebble Time (2015) | Primary testing — tightest pixel budget |
| **emery** | Pebble Time 2 (2026) | Secondary — verify larger layout |
| aplite | NOT supported by our app | Don't test on this |
| chalk | NOT supported | Round display, different UX |

**Always test basalt first** — if it looks good on 144×168, it'll look great on emery's 200×228.

---

## CloudPebble Settings

### Capabilities

In CloudPebble's Settings tab, the following capabilities should be checked:
- **location** — enables GPS via PKJS
- **configurable** — shows gear icon for settings page

These correspond to `"capabilities"` in `appinfo.json`.

### App Keys

CloudPebble shows app keys in Settings → PebbleKit JS Message Keys. These must match `"appKeys"` in `appinfo.json`:

| Key | ID | Purpose |
|-----|----|---------|
| KEY_STATION | 0 | Station list (phone → watch) |
| KEY_DEPARTURES | 1 | Departure data (phone → watch) |
| KEY_ERROR | 2 | Error messages (phone → watch) |
| KEY_FETCH | 3 | Request departures (watch → phone) |
| KEY_CFG_DEFAULT_STATION | 4 | Settings (phone → watch) |
| KEY_CFG_MAX_DISTANCE | 5 | Settings (phone → watch) |

---

## Publishing from CloudPebble

CloudPebble can auto-generate screenshots for all platforms:

```bash
pebble build
pebble publish
```

Or use the CloudPebble UI: Build → Publish. It generates GIFs/screenshots and uploads them to the Pebble appstore.

### Manual Submission

1. Build → download `.pbw`
2. Go to [rebble.io/submit](https://rebble.io/submit)
3. Fill in details from `APPSTORE.md`
4. Upload `.pbw`

---

*Learned during Bildschirmli development, June 2026*
