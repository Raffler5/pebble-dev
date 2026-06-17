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

## UUID Must Be Valid Hex

The UUID in `appinfo.json` must be strictly hexadecimal (`0-9`, `a-f`). CloudPebble parses it to set the project UUID. If the UUID contains invalid hex characters (like `l`, `o`, `g`), CloudPebble silently generates a different UUID, causing mismatches on every build.

See COMMON_PITFALLS.md §7 for the full story.

**After first publish, the UUID is permanent.** Changing it makes the store treat it as a different app — no updates, duplicate installs.

---

## CloudPebble Import Steps

1. Go to [cloudpebble.repebble.com](https://cloudpebble.repebble.com/)
2. **Import** → **Import from GitHub**
3. Select `Raffler5/bildschirmli-pebble`
4. CloudPebble reads `appinfo.json` and sets up the project
5. **Verify UUID**: Settings → App UUID must match `appinfo.json` exactly
6. **Build** (Ctrl+B or top-right button)
7. **Run** → select emulator (basalt or emery)

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
| KEY_CFG_COLOR | 6 | Text color ARGB8 (phone → watch) |
| KEY_CFG_BG_COLOR | 7 | Background color ARGB8 (phone → watch) |

---

## Publishing from CloudPebble

### First Release

1. Build → download `.pbw`
2. Go to the Rebble developer portal (dev-portal.rebble.io)
3. Create new app → fill in metadata, screenshots, description
4. Upload `.pbw`

### Publishing Updates

1. **Bump version** in `appinfo.json` (e.g. `0.2.0` → `0.3.0`)
2. **Sync to CloudPebble repo** and push to GitHub
3. In CloudPebble: pull from GitHub
4. **Verify UUID** matches the published version (Settings → App UUID)
5. Build → download `.pbw`
6. On the dev portal: create a new release for the existing app → upload `.pbw`

**Critical:** The UUID in the `.pbw` must match the originally published UUID. If it doesn't, the store rejects the update. This is the most common publishing failure — see the UUID section above.

### Version Numbering

Use semver in `appinfo.json`'s `versionLabel`:
- **Major** (1.0.0): breaking changes, new architecture
- **Minor** (0.2.0): new features (color settings, favorites)
- **Patch** (0.2.1): bug fixes (charset fix, race condition fix)

---

*Learned during Bildschirmli development, June 2026*
