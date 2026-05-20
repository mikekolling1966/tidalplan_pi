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
- **Results list** — top 20 windows ranked by tidal score with ★ rating and colour coding
- **Leg breakdown** — double-click any row for a per-leg tidal stream summary
- **Embedded icon** — compass/wave toolbar button baked into the binary

## Panel layout

```
┌─ Route ────────────────────────────────────────────────────────┐
│  [Queenborough to Ramsgate 2025                    ▼]  [↺]    │
├─ Server ───────────────────────────────────────────────────────┤
│  [http://192.168.1.30:8081                     ]  [Apply]      │
├─ Analysis window ──────────────────────────────────────────────┤
│  Speed: [10.0] kt                                              │
│  From: [21/05/2026] [06] :00   To: [21/05/2026] [22] :00      │
├────────────────────────────────────────────────────────────────┤
│  [▶ Analyse Route]                                             │
├────────────────────────────────────────────────────────────────┤
│  Best: Thu 21 May 04:00  (48 windows tested)                   │
├───┬──────────────────┬──────────────────┬──────┬──────────────┤
│ # │ Depart           │ ETA              │ Hrs  │ Score        │
│ 1 │ Thu 21 May 04:00 │ Thu 21 May 07:06 │  3.0 │ ★★★★  100   │
│ 2 │ Thu 21 May 04:30 │ Thu 21 May 07:36 │  3.1 │ ★★★★  100   │
│...│                  │                  │      │              │
└───┴──────────────────┴──────────────────┴──────┴──────────────┘
```

## Requirements

| Component | Details |
|-----------|---------|
| OpenCPN | 5.6 or later (plugin API 1.18) |
| macOS | Apple Silicon or Intel (arm64 / x86_64) |
| TidalPlan server | Running on your Pi or locally — see [tidalplan](https://github.com/mikekolling1966/tidalplan) |
| CMake | 3.15+ |
| wxWidgets | 3.2 (Homebrew `wxwidgets@3.2`) — headers only at build time |
| C++ | 17 |

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

## Install

1. Copy `libtidalplan_pi.dylib` to `~/Library/Application Support/OpenCPN/Contents/PlugIns/`
2. Start OpenCPN
3. Go to **OpenCPN menu → Preferences → Plugins**
4. Find **TidalPlan** and tick **Enable**
5. The compass/wave toolbar button appears — click it to open the panel

## Usage

1. Open the TidalPlan panel from the toolbar
2. Select a route from the dropdown (click **↺** to refresh if you just drew one)
3. Set the server URL if needed (default: `http://192.168.1.30:8081`)
4. Set vessel speed, date range, and the departure hour window
5. Click **▶ Analyse Route**
6. Top 20 departure windows appear, ranked and colour-coded:
   - 🟢 Dark green — excellent (score ≥ 75)
   - 🟡 Dark yellow — good (score ≥ 50)
   - 🟠 Dark orange — fair (score ≥ 25)
   - 🔴 Dark red — poor
7. Double-click any row for a per-leg tidal stream breakdown

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
