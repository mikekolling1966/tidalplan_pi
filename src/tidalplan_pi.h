#pragma once

#include "ocpn_plugin.h"

class TidalPlanPanel;

// ---------------------------------------------------------------------------
// Version constants
// ---------------------------------------------------------------------------
static const int TIDALPLAN_API_VERSION_MAJOR = 1;
static const int TIDALPLAN_API_VERSION_MINOR = 18;  // Requires OpenCPN 5.6+
static const int TIDALPLAN_VERSION_MAJOR     = 1;
static const int TIDALPLAN_VERSION_MINOR     = 0;

// ---------------------------------------------------------------------------
// TidalPlanPlugin
//
// Main plugin class. Responsibilities:
//   - Register toolbar button
//   - Listen for OCPN_ROUTE_ACTIVATED / DEACTIVATED messages to track the
//     GUID of the currently active route
//   - Create and show/hide the TidalPlanPanel side panel
//   - Persist settings (server URL, vessel speed) to OpenCPN config
// ---------------------------------------------------------------------------
class TidalPlanPlugin : public opencpn_plugin_118 {
public:
    explicit TidalPlanPlugin(void* ppimgr);
    ~TidalPlanPlugin() override;

    // --- Required plugin interface ---
    int     Init()                      override;
    bool    DeInit()                    override;

    int     GetAPIVersionMajor()        override { return TIDALPLAN_API_VERSION_MAJOR; }
    int     GetAPIVersionMinor()        override { return TIDALPLAN_API_VERSION_MINOR; }
    int     GetPlugInVersionMajor()     override { return TIDALPLAN_VERSION_MAJOR; }
    int     GetPlugInVersionMinor()     override { return TIDALPLAN_VERSION_MINOR; }

    wxBitmap* GetPlugInBitmap()         override;
    wxString  GetCommonName()           override { return "TidalPlan"; }
    wxString  GetShortDescription()     override;
    wxString  GetLongDescription()      override;

    // --- Event callbacks ---
    void OnToolbarToolCallback(int id)  override;

    // --- Plugin messaging (route activation events) ---
    void SetPluginMessage(wxString& message_id, wxString& message_body) override;

    // --- Shared state (read by TidalPlanPanel) ---
    wxString m_active_route_guid;   // GUID of currently active OpenCPN route; "" if none
    wxString m_server_url;          // e.g. "http://192.168.1.30:8081"
    double   m_vessel_speed_kts;    // knots, default 6.5

private:
    int             m_toolbar_item_id;
    wxBitmap*       m_icon_bmp;
    TidalPlanPanel* m_panel;

    void LoadSettings();
    void SaveSettings();
};
