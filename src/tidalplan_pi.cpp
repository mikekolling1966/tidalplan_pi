#include "tidalplan_pi.h"
#include "tidalplan_panel.h"
#include "tidalplan_icon_data.h"

#include <wx/filename.h>
#include <wx/fileconf.h>
#include <wx/mstream.h>

// ---------------------------------------------------------------------------
// OpenCPN entry points — must be C-linkage
// ---------------------------------------------------------------------------
extern "C" {
    DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
        return new TidalPlanPlugin(ppimgr);
    }
    DECL_EXP void destroy_pi(opencpn_plugin* p) {
        delete p;
    }
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
TidalPlanPlugin::TidalPlanPlugin(void* ppimgr)
    : opencpn_plugin_118(ppimgr)
    , m_toolbar_item_id(-1)
    , m_icon_bmp(nullptr)
    , m_panel(nullptr)
    , m_server_url("http://192.168.1.30:8081")
    , m_vessel_speed_kts(6.5)
{
    // GetPlugInBitmap() may be called by OpenCPN BEFORE Init() during the
    // catalog import flow.  Create a valid fallback bitmap now so it never
    // returns nullptr.  Init() will replace it with the real icon if the PNG
    // data file is present.
    m_icon_bmp = new wxBitmap(32, 32);
    wxMemoryDC dc(*m_icon_bmp);
    dc.SetBackground(wxBrush(wxColour(0, 128, 128)));  // teal placeholder
    dc.Clear();
}

TidalPlanPlugin::~TidalPlanPlugin() {
    delete m_icon_bmp;
}

// ---------------------------------------------------------------------------
// Init — called once when OpenCPN loads the plugin
// ---------------------------------------------------------------------------
int TidalPlanPlugin::Init() {
    LoadSettings();

    // Load icon from the embedded PNG bytes (no external file needed).
    {
        wxMemoryInputStream mis(kTidalPlanIconPng, kTidalPlanIconPngSize);
        wxImage img(mis, wxBITMAP_TYPE_PNG);
        if (img.IsOk()) {
            delete m_icon_bmp;
            m_icon_bmp = new wxBitmap(img);
        }
    }

    // Add toolbar button (toggle style)
    m_toolbar_item_id = InsertPlugInTool(
        "",                                     // label
        m_icon_bmp, m_icon_bmp,                 // normal / rollover bitmap
        wxITEM_CHECK,                           // toggle button
        "TidalPlan",                            // short description (tooltip)
        "Open TidalPlan tidal departure analyser",  // long description
        nullptr,                                // client data
        -1,                                     // position (-1 = append)
        0,                                      // tool id (0 = auto-assign)
        this
    );

    // Panel is created lazily on first toolbar click — GetOCPNCanvasWindow()
    // returns null during startup loading so we must NOT call it here.

    // WANTS_PLUGIN_MESSAGING causes OpenCPN to call SetPluginMessage() for ALL
    // plugin messages — no explicit subscription needed.
    return WANTS_TOOLBAR_CALLBACK |
           WANTS_PLUGIN_MESSAGING |
           WANTS_CONFIG;
}

// ---------------------------------------------------------------------------
// DeInit — cleanup before unload
// ---------------------------------------------------------------------------
bool TidalPlanPlugin::DeInit() {
    SaveSettings();

    if (m_panel) {
        m_panel->Destroy();
        m_panel = nullptr;
    }

    RemovePlugInTool(m_toolbar_item_id);
    return true;
}

// ---------------------------------------------------------------------------
// Plugin metadata
// ---------------------------------------------------------------------------
wxBitmap* TidalPlanPlugin::GetPlugInBitmap() {
    return m_icon_bmp;
}

wxString TidalPlanPlugin::GetShortDescription() {
    return "Tidal departure window analyser using TidalPlan server.";
}

wxString TidalPlanPlugin::GetLongDescription() {
    return
        "TidalPlan plugin reads the active OpenCPN route, sends it to a "
        "TidalPlan server (running on your Pi or locally), and displays "
        "the best departure windows ranked by tidal stream score.\n\n"
        "Configure the server URL in the plugin panel (default: "
        "http://192.168.1.30:8081).\n\n"
        "Data sources: CMEMS 1.5 km hydrodynamic model, UKHO Admiralty API, "
        "TICON-4 harmonic constants.";
}

// ---------------------------------------------------------------------------
// Toolbar callback — toggle the panel
// ---------------------------------------------------------------------------
void TidalPlanPlugin::OnToolbarToolCallback(int id) {
    if (id != m_toolbar_item_id) return;

    // Lazy panel creation — safe here because the canvas is fully initialised
    // by the time the user can click a toolbar button.
    if (!m_panel) {
        wxWindow* canvas = GetOCPNCanvasWindow();
        if (!canvas) return;   // should never happen post-startup, but guard anyway
        m_panel = new TidalPlanPanel(
            canvas,
            m_server_url,
            m_vessel_speed_kts,
            m_active_route_guid
        );
        m_panel->Hide();
    }

    if (m_panel->IsShown()) {
        m_panel->Hide();
        SetToolbarItemState(id, false);
    } else {
        m_panel->Show();
        m_panel->Raise();
        SetToolbarItemState(id, true);
    }
}

// ---------------------------------------------------------------------------
// Plugin message handler — track active route
//
// OpenCPN sends JSON messages when routes are activated/deactivated.
// Message body for OCPN_ROUTE_ACTIVATED looks like:
//   {"GUID": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx", "Name": "My Route"}
// ---------------------------------------------------------------------------
void TidalPlanPlugin::SetPluginMessage(wxString& message_id,
                                        wxString& message_body) {
    if (message_id == "OCPN_ROUTE_ACTIVATED") {
        // Simple JSON key extraction — avoids adding a JSON lib dependency.
        // If you have wxJSON available: use wxJSONReader instead.
        int guid_start = message_body.Find("\"GUID\"");
        if (guid_start != wxNOT_FOUND) {
            int colon = message_body.find(':', guid_start);
            if (colon != wxNOT_FOUND) {
                int q1 = message_body.find('"', colon + 1);
                int q2 = message_body.find('"', q1 + 1);
                if (q1 != wxNOT_FOUND && q2 != wxNOT_FOUND) {
                    m_active_route_guid = message_body.Mid(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        // Notify panel so it can update the status label
        if (m_panel) m_panel->OnRouteActivated();

    } else if (message_id == "OCPN_ROUTE_DEACTIVATED") {
        m_active_route_guid.Clear();
        if (m_panel) m_panel->OnRouteDeactivated();
    }
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------
void TidalPlanPlugin::LoadSettings() {
    wxFileConfig* cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath("/PlugIns/TidalPlan");
    cfg->Read("ServerURL",       &m_server_url,        "http://192.168.1.30:8081");
    cfg->Read("VesselSpeedKnots", &m_vessel_speed_kts, 6.5);
}

void TidalPlanPlugin::SaveSettings() {
    wxFileConfig* cfg = GetOCPNConfigObject();
    if (!cfg) return;
    cfg->SetPath("/PlugIns/TidalPlan");

    // Pull current values back from the panel before saving
    if (m_panel) {
        m_server_url       = m_panel->GetServerUrl();
        m_vessel_speed_kts = m_panel->GetVesselSpeed();
    }

    cfg->Write("ServerURL",        m_server_url);
    cfg->Write("VesselSpeedKnots", m_vessel_speed_kts);
}
