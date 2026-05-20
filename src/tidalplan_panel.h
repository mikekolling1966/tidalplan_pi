#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>

// ---------------------------------------------------------------------------
// TidalPlanPanel
//
// A wxPanel docked inside the OpenCPN canvas window.  Layout:
//
//   ┌──────────────────────────────────────────────────┐
//   │ Server: [http://192.168.1.30:8081____] [⚙ Apply] │  ← settings strip
//   │ Speed: [6.5 kt] From:[date] To:[date]  [▶ Analyse]│  ← analysis strip
//   ├──────────────────────────────────────────────────┤
//   │ Status: "Active route: Plymouth→Falmouth (12 WP)"│  ← status label
//   ├──────────────────────────────────────────────────┤
//   │ Rank │ Departure      │ ETA            │ Score   │
//   │  1   │ Wed 21 06:00   │ Wed 21 14:30   │ ★★★★ 84│  ← results list
//   │  2   │ Wed 21 06:30   │ Wed 21 14:55   │ ★★★  71│
//   │  ...                                             │
//   └──────────────────────────────────────────────────┘
//
// No WebView dependency — pure native wxWidgets.  Works offline once the
// TidalPlan server has cached its CMEMS data.
// ---------------------------------------------------------------------------
class TidalPlanPanel : public wxFrame {
public:
    TidalPlanPanel(wxWindow* parent,
                   const wxString& server_url,
                   double vessel_speed_kts,
                   const wxString& active_route_guid);

    // Called by TidalPlanPlugin when OpenCPN activates/deactivates a route
    void OnRouteActivated();
    void OnRouteDeactivated();

    // Accessors used by plugin to persist settings
    wxString GetServerUrl()    const;
    double   GetVesselSpeed()  const;

private:
    // --- UI event handlers ---
    void OnAnalyse(wxCommandEvent& event);
    void OnApplySettings(wxCommandEvent& event);
    void OnListItemActivated(wxListEvent& event);  // double-click → detail popup
    void OnRefreshRoutes(wxCommandEvent& event);   // ↺ button next to route choice
    void OnRouteSelected(wxCommandEvent& event);   // route dropdown selection change
    void OnClose(wxCloseEvent& event);             // hide instead of destroy

    // --- Core logic ---
    // Reads the active route from OpenCPN and builds the JSON body for
    // /api/route/analyse-json.  Returns empty string on failure.
    wxString BuildRequestJson();

    // HTTP POST to <server_url>/api/route/analyse-json.
    // Returns true and fills `response` on HTTP 200, false otherwise.
    bool PostToServer(const wxString& json_body, wxString& response);

    // Parses the JSON response and populates m_results_list.
    void PopulateResults(const wxString& json_response);

    // Simple inline JSON helpers (avoids adding a JSON library dependency)
    // Extract a string value for a key from a flat JSON object.
    static wxString JsonGetString(const wxString& json, const wxString& key);
    // Extract a double value for a key.
    static double   JsonGetDouble(const wxString& json, const wxString& key, double fallback = 0.0);

    void RefreshRouteList();   // populate m_route_choice from OpenCPN

    // --- UI widgets ---
    wxChoice*       m_route_choice;
    wxTextCtrl*     m_url_ctrl;
    wxSpinCtrlDouble* m_speed_ctrl;
    wxDatePickerCtrl* m_start_date;
    wxSpinCtrl*       m_start_hour;
    wxDatePickerCtrl* m_end_date;
    wxSpinCtrl*       m_end_hour;
    wxButton*       m_analyse_btn;
    wxStaticText*   m_status_lbl;
    wxListCtrl*     m_results_list;
    wxStaticText*   m_footer_lbl;

    // --- State ---
    wxArrayString   m_route_guids;  // parallel to m_route_choice items
    wxString        m_route_guid;   // GUID of currently selected route

    // Raw JSON of the last successful response (for detail popup)
    wxString m_last_response;

    wxDECLARE_EVENT_TABLE();
};
