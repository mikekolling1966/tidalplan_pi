# tidalplan_pi — OpenCPN Plugin

An OpenCPN plugin that reads any route from your chart planner, sends it to a
[TidalPlan](https://github.com/mikekolling1966/tidalplan) server, and displays
the top 20 ranked departure windows directly inside OpenCPN — no browser needed.

## How it works

```
OpenCPN route (any — no need to activate for navigation)
        │  plugin reads waypoints via GetRoute_Plugin()
        ▼
POST /api/route/analyse-json  ──→  TidalPlan server (Raspberry Pi or localhost)
        │                              CMEMS 1.5 km hydrodynamic model
        │                              UKHO Admiralty tidal API
        │                              TICON-4 harmonic constants
        ▼
Ranked departure windows shown in floating panel, colour-coded by score
```

## Features

- **Route dropdown** — pick any saved route; no need to right-click → Activate
- **Date + time range** — set start/end date (limited to next 7 days) and hour
- **Vessel speed** — adjustable knot spinner
- **GRIB wind** — load a `.grb`/`.grib` file directly from the panel; uploaded to the TidalPlan server automatically, no curl required
- **Results list** — top 20 windows ranked by tidal score with ★ rating and colour coding
- **Leg breakdown** — double-click any row for a per-leg summary showing tidal stream, data source (`Station / CMEMS` or just `Station` for harmonic fallback), and wind speed/direction when a GRIB is loaded
- **Embedded icon** — compass/wave toolbar button baked into the binary

## Panel layout

```
┌─ Route ────────────────────────────────────────────────────────┐
│  [Queenborough to Ramsgate 2025                    ▼]  [↺]    │
├─ Server ───────────────────────────────────────────────────────┤
│  [http://192.168.1.30:8081                     ]  [Apply]      │
├─ GRIB Wind (optional) ─────────────────────────────────────────┤
│  [GRIB: METEOCONSULT12Z_VENT_0520…grb      ] [Load GRIB…] [✕] │
├─ Analysis window ──────────────────────────────────────────────┤
│  Speed: [10.0] kt                                              │
│  From: [21/05/2026] [06] :00   To: [21/05/2026] [22] :00      │
├────────────────────────────────────────────────────────────────┤
│  [▶ Analyse Route]                                             │
├────────────────────────────────────────────────────────────────┤
│  Best: Thu 21 May 16:00  (10 windows tested)                   │
├───┬──────────────────┬──────────────────┬──────┬──────────────┤
│ # │ Depart           │ ETA              │ Hrs  │ Score        │
│ 1 │ Thu 21 May 16:00 │ Thu 21 May 19:04 │  3.1 │ ★★★★  100   │
│ 2 │ Thu 21 May 16:30 │ Thu 21 May 19:33 │  3.0 │ ★★★★  100   │
│...│                  │                  │      │              │
└───┴──────────────────┴──────────────────┴──────┴──────────────┘
```

### Leg detail popup (double-click a row)

```
Departure:  2026-05-21T15:30
ETA:        2026-05-21T18:37
Passage:    3.1 hours
Score:      99  (Excellent)

Legs:
  Leg 1: 1.1 nm  hdg 20°  tide +0.5 kt fair  [SHEERNESS / CMEMS]  wind 4 kt from S
  Leg 2: 0.7 nm  hdg 62°  tide +0.9 kt fair  [SHEERNESS / CMEMS]  wind 5 kt from S
  Leg 5: 10.0 nm hdg 105° tide +0.7 kt fair  [Herne Bay / CMEMS]  wind 6 kt from SSW
  ...
```

Source label shows `Station / CMEMS` when the tidal current came from the CMEMS model, or just `Station` when the server fell back to harmonic data (e.g. very shallow inshore legs outside the CMEMS grid). Wind line only appears when a GRIB is loaded.

## Requirements

| Component | Details |
|-----------|---------|
| OpenCPN | 5.6 or later (plugin API 1.18) |
| macOS | Apple Silicon or Intel (arm64 / x86_64) |
| TidalPlan server | Running on your Pi or locally — see [tidalplan](https://github.com/mikekolling1966/tidalplan) |
| CMake | 3.15+ |
| wxWidgets | 3.2 (Homebrew `wxwidgets@3.2`) — headers only at build time |
| C++ | 17 |

## Build & Install (Linux / Raspberry Pi)

Tested on Raspberry Pi OS Bookworm ARM64 (Debian 12) with OpenCPN 5.14 built from source.

```bash
# 1. Install build dependencies
sudo apt install -y cmake git libwxgtk3.2-dev

# 2. Clone and build
git clone https://github.com/mikekolling1966/tidalplan_pi.git
cd tidalplan_pi
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Produces: build/libtidalplan_pi.so  (lib prefix is required on Linux)

# 3. Copy to the user plugin directory (NOT /usr/local/lib/opencpn/)
mkdir -p ~/.local/lib/opencpn/
cp build/libtidalplan_pi.so ~/.local/lib/opencpn/

# 4. Create install data so OpenCPN recognises it
mkdir -p ~/.opencpn/plugins/install_data
printf '/home/pi/.local/lib/\n/home/pi/.local/lib/opencpn/\n/home/pi/.local/lib/opencpn/libtidalplan_pi.so\n' \
  > ~/.opencpn/plugins/install_data/tidalplan.files
printf 'lib: /home/pi/.local/lib\nbin: /home/pi/.local/bin\n' \
  > ~/.opencpn/plugins/install_data/tidalplan.dirs
echo '1.0.0' > ~/.opencpn/plugins/install_data/tidalplan.version

# 5. Add a catalog entry (while OpenCPN is closed)
python3 - <<'PY'
entry = """  <plugin version="1">
    <name>TidalPlan</name><version>1.0.0</version><release>0</release>
    <summary>Tidal departure window analyser</summary>
    <api-version>1.16</api-version><open-source>yes</open-source>
    <target>debian-arm64</target><target-version>12</target-version>
    <target-arch>arm64</target-arch>
    <tarball-url>file:///home/pi/.local/lib/opencpn/libtidalplan_pi.so</tarball-url>
  </plugin>\n"""
import os; path = os.path.expanduser('~/.opencpn/ocpn-plugins.xml')
c = open(path).read()
if 'TidalPlan' not in c:
    open(path,'w').write(c.replace('</plugins>', entry+'</plugins>'))
    print('Added')
PY

# 6. Start OpenCPN — TidalPlan appears in Preferences → Plugins, enable it
```

> **Key gotcha:** The plugin must be in `~/.local/lib/opencpn/` (user dir), not
> `/usr/local/lib/opencpn/` (system dir). Plugins in the system dir without a
> catalog entry are silently skipped after the candidate scan (log line 505)
> and never reach the compatibility check (line 596). Also `opencpn.conf` is
> rewritten on every OpenCPN exit — only edit it while OpenCPN is closed.

## Build (macOS)

```bash
# 1. Install build dependencies
brew install cmake wxwidgets@3.2

# 2. Clone
git clone https://github.com/mikekolling1966/tidalplan_pi.git
cd tidalplan_pi

# 3. Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 4. Deploy
cp build/libtidalplan_pi.dylib \
   ~/Library/Application\ Support/OpenCPN/Contents/PlugIns/
```

> **Note:** The plugin links directly against OpenCPN's bundled wxWidgets dylibs
> (`/Applications/OpenCPN.app/Contents/Frameworks/libwx_*.dylib`) — no separate
> wxWidgets install is needed at runtime.

## Install (macOS)

1. Copy `libtidalplan_pi.dylib` to `~/Library/Application Support/OpenCPN/Contents/PlugIns/`
2. Start OpenCPN
3. Go to **OpenCPN menu → Preferences → Plugins**
4. Find **TidalPlan** and tick **Enable**
5. The compass/wave toolbar button appears — click it to open the panel

## Usage

1. Open the TidalPlan panel from the toolbar
2. Select a route from the dropdown (click **↺** to refresh if you just drew one)
3. Set the server URL if needed (default: `http://192.168.1.30:8081`)
4. *(Optional)* Click **Load GRIB…** to pick a wind forecast file — the same `.grb`/`.grib` file you'd load in OpenCPN's GRIB plugin. It is uploaded to the TidalPlan server automatically and wind speed/direction will appear in the leg detail popup
5. Set vessel speed, date range, and the departure hour window
6. Click **▶ Analyse Route**
7. Top 20 departure windows appear, ranked and colour-coded:
   - 🟢 Dark green — excellent (score ≥ 75)
   - 🟡 Dark yellow — good (score ≥ 50)
   - 🟠 Dark orange — fair (score ≥ 25)
   - 🔴 Dark red — poor
8. Double-click any row for a per-leg breakdown: tidal stream, data source, and wind (if GRIB loaded)

## JSON request format

The plugin POSTs to `/api/route/analyse-json` on the TidalPlan server:

```json
{
  "waypoints":        [[50.37, -4.14], [50.15, -5.07]],
  "vessel_speed":     10.0,
  "start_datetime":   "2026-05-21T06:00:00",
  "end_datetime":     "2026-05-21T22:59:00",
  "interval_minutes": 30,
  "top_n":            20
}
```

## Project structure

```
tidalplan_pi/
├── CMakeLists.txt              # Build configuration
├── include/
│   └── ocpn_plugin.h           # OpenCPN plugin API header (from OpenCPN 5.14.0)
├── src/
│   ├── tidalplan_pi.h/.cpp     # Plugin entry point (toolbar, settings, messaging)
│   ├── tidalplan_panel.h/.cpp  # Floating panel UI and HTTP logic
│   └── tidalplan_icon_data.h   # Embedded 32×32 PNG icon (auto-generated)
└── data/
    └── tidalplan_icon.png      # Source icon (compass rose + wave on teal)
```

## Related

- [tidalplan](https://github.com/mikekolling1966/tidalplan) — the server that does the actual tidal analysis
