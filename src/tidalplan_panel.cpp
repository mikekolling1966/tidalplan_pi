#include "tidalplan_panel.h"
#include "ocpn_plugin.h"

#include <wx/sizer.h>
#include <wx/protocol/http.h>
#include <wx/sstream.h>
#include <wx/tokenzr.h>
#include <wx/datetime.h>

// ---------------------------------------------------------------------------
// Custom event IDs
// ---------------------------------------------------------------------------
enum {
    ID_REFRESH_ROUTES = wxID_HIGHEST + 1,
    ID_ROUTE_CHOICE,
};

// ---------------------------------------------------------------------------
// Event table
// ---------------------------------------------------------------------------
wxBEGIN_EVENT_TABLE(TidalPlanPanel, wxFrame)
    EVT_BUTTON(wxID_APPLY,        TidalPlanPanel::OnApplySettings)
    EVT_BUTTON(wxID_EXECUTE,      TidalPlanPanel::OnAnalyse)
    EVT_BUTTON(ID_REFRESH_ROUTES, TidalPlanPanel::OnRefreshRoutes)
    EVT_CHOICE(ID_ROUTE_CHOICE,   TidalPlanPanel::OnRouteSelected)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, TidalPlanPanel::OnListItemActivated)
    EVT_CLOSE(TidalPlanPanel::OnClose)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
// Constructor — build the full panel layout
// ---------------------------------------------------------------------------
TidalPlanPanel::TidalPlanPanel(wxWindow* parent,
                                const wxString& server_url,
                                double vessel_speed_kts,
                                const wxString& active_route_guid)
    : wxFrame(parent, wxID_ANY, "TidalPlan",
              wxDefaultPosition, wxSize(440, 520),
              wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT)
    , m_route_guid(active_route_guid)
{
    wxPanel* panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // ── Route selector strip ───────────────────────────────────────────────
    wxStaticBoxSizer* route_box =
        new wxStaticBoxSizer(wxHORIZONTAL, panel, "Route");

    m_route_choice = new wxChoice(panel, ID_ROUTE_CHOICE,
                                  wxDefaultPosition, wxDefaultSize);
    m_route_choice->Append("(loading routes…)");
    m_route_choice->SetSelection(0);

    wxButton* refresh_btn = new wxButton(panel, ID_REFRESH_ROUTES,
                                         wxString::FromUTF8("\xe2\x86\xba"),  // ↺
                                         wxDefaultPosition, wxDefaultSize,
                                         wxBU_EXACTFIT);
    refresh_btn->SetToolTip("Refresh route list from OpenCPN");

    route_box->Add(m_route_choice, 1, wxEXPAND | wxALL, 4);
    route_box->Add(refresh_btn,    0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
    main_sizer->Add(route_box, 0, wxEXPAND | wxALL, 4);

    // ── Settings strip ─────────────────────────────────────────────────────
    wxStaticBoxSizer* settings_box =
        new wxStaticBoxSizer(wxHORIZONTAL, panel, "Server");

    m_url_ctrl = new wxTextCtrl(panel, wxID_ANY, server_url,
                                wxDefaultPosition, wxSize(240, -1));
    wxButton* apply_btn = new wxButton(panel, wxID_APPLY, "Apply",
                                       wxDefaultPosition, wxDefaultSize,
                                       wxBU_EXACTFIT);

    settings_box->Add(m_url_ctrl,  1, wxEXPAND | wxALL, 4);
    settings_box->Add(apply_btn,   0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
    main_sizer->Add(settings_box, 0, wxEXPAND | wxALL, 4);

    // ── Analysis parameters strip ──────────────────────────────────────────
    wxStaticBoxSizer* params_box =
        new wxStaticBoxSizer(wxVERTICAL, panel, "Analysis window");

    // Row 1: speed
    wxBoxSizer* speed_row = new wxBoxSizer(wxHORIZONTAL);
    m_speed_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY,
                                        wxString::Format("%.1f", vessel_speed_kts),
                                        wxDefaultPosition, wxSize(65, -1),
                                        wxSP_ARROW_KEYS, 1.0, 25.0,
                                        vessel_speed_kts, 0.5);
    speed_row->Add(new wxStaticText(panel, wxID_ANY, "Speed:"),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    speed_row->Add(m_speed_ctrl, 0, wxALIGN_CENTER_VERTICAL);
    speed_row->Add(new wxStaticText(panel, wxID_ANY, "kt"),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    params_box->Add(speed_row, 0, wxALL, 4);

    // Row 2: date + time range
    wxBoxSizer* range_row = new wxBoxSizer(wxHORIZONTAL);

    wxDateTime now  = wxDateTime::Now().ToUTC();
    wxDateTime then = now + wxDateSpan::Days(2);

    // Limit date pickers to today → today+7
    wxDateTime range_min = now;
    range_min.SetHour(0); range_min.SetMinute(0); range_min.SetSecond(0);
    wxDateTime range_max = range_min + wxDateSpan::Days(7);

    m_start_date = new wxDatePickerCtrl(panel, wxID_ANY, now,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxDP_DEFAULT | wxDP_SHOWCENTURY);
    m_start_date->SetRange(range_min, range_max);

    m_start_hour = new wxSpinCtrl(panel, wxID_ANY, "0",
                                  wxDefaultPosition, wxSize(52, -1),
                                  wxSP_ARROW_KEYS | wxSP_WRAP, 0, 23, 0);

    m_end_date   = new wxDatePickerCtrl(panel, wxID_ANY, then,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxDP_DEFAULT | wxDP_SHOWCENTURY);
    m_end_date->SetRange(range_min, range_max);

    m_end_hour   = new wxSpinCtrl(panel, wxID_ANY, "23",
                                  wxDefaultPosition, wxSize(52, -1),
                                  wxSP_ARROW_KEYS | wxSP_WRAP, 0, 23, 23);

    range_row->Add(new wxStaticText(panel, wxID_ANY, "From:"),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    range_row->Add(m_start_date, 0, wxALIGN_CENTER_VERTICAL);
    range_row->Add(m_start_hour, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    range_row->Add(new wxStaticText(panel, wxID_ANY, ":00  To:"),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    range_row->Add(m_end_date,   0, wxALIGN_CENTER_VERTICAL);
    range_row->Add(m_end_hour,   0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    range_row->Add(new wxStaticText(panel, wxID_ANY, ":00"),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
    params_box->Add(range_row, 0, wxALL, 4);

    main_sizer->Add(params_box, 0, wxEXPAND | wxALL, 4);

    // ── Analyse button ─────────────────────────────────────────────────────
    m_analyse_btn = new wxButton(panel, wxID_EXECUTE,
                                 wxString::FromUTF8("\xe2\x96\xb6  Analyse Route"));
    m_analyse_btn->SetToolTip(
        "Send the selected route to the TidalPlan server and rank departure windows");
    main_sizer->Add(m_analyse_btn, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    // ── Status label ───────────────────────────────────────────────────────
    m_status_lbl = new wxStaticText(panel, wxID_ANY,
                                    "Select a route above, then click Analyse.",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxST_ELLIPSIZE_END);
    m_status_lbl->SetForegroundColour(wxColour(180, 180, 180));
    main_sizer->Add(m_status_lbl, 0, wxEXPAND | wxALL, 6);

    // ── Results list ───────────────────────────────────────────────────────
    m_results_list = new wxListCtrl(panel, wxID_ANY,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxLC_REPORT | wxLC_SINGLE_SEL |
                                    wxLC_HRULES | wxLC_VRULES);

    m_results_list->InsertColumn(0, "#",          wxLIST_FORMAT_CENTER, 28);
    m_results_list->InsertColumn(1, "Depart",     wxLIST_FORMAT_LEFT,  120);
    m_results_list->InsertColumn(2, "ETA",        wxLIST_FORMAT_LEFT,  110);
    m_results_list->InsertColumn(3, "Hrs",        wxLIST_FORMAT_RIGHT,  38);
    m_results_list->InsertColumn(4, "Score",      wxLIST_FORMAT_LEFT,   80);

    main_sizer->Add(m_results_list, 1, wxEXPAND | wxALL, 4);

    // ── Footer ─────────────────────────────────────────────────────────────
    m_footer_lbl = new wxStaticText(panel, wxID_ANY,
                                    "Double-click a row for leg breakdown.",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxST_ELLIPSIZE_END);
    m_footer_lbl->SetForegroundColour(wxColour(120, 120, 120));
    wxFont small = m_footer_lbl->GetFont();
    small.SetPointSize(small.GetPointSize() - 1);
    m_footer_lbl->SetFont(small);
    main_sizer->Add(m_footer_lbl, 0, wxEXPAND | wxALL, 4);

    panel->SetSizer(main_sizer);
    panel->Layout();

    // Populate the route dropdown now (panel is fully constructed)
    RefreshRouteList();
}

// ---------------------------------------------------------------------------
// Route activation notifications (called by TidalPlanPlugin)
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnRouteActivated() {
    // A route was navigation-activated — refresh so it's pre-selected
    RefreshRouteList();
}

void TidalPlanPanel::OnRouteDeactivated() {
    // Route deactivated from navigation — refresh (route still exists, just not active)
    m_route_guid.Clear();
    RefreshRouteList();
    m_results_list->DeleteAllItems();
}

// ---------------------------------------------------------------------------
// RefreshRouteList — populate m_route_choice from OpenCPN route database
// ---------------------------------------------------------------------------
void TidalPlanPanel::RefreshRouteList() {
    m_route_choice->Clear();
    m_route_guids.Clear();

    wxArrayString guids = GetRouteGUIDArray();
    for (const wxString& guid : guids) {
        auto route = GetRoute_Plugin(guid);
        if (!route) continue;
        wxString name = route->m_NameString.Trim();
        if (name.IsEmpty()) name = "Unnamed route";
        m_route_choice->Append(name);
        m_route_guids.Add(guid);
    }

    if (m_route_guids.IsEmpty()) {
        m_route_choice->Append("(no routes — draw one in OpenCPN)");
        m_route_choice->SetSelection(0);
        m_route_choice->Disable();
        m_analyse_btn->Disable();
        m_status_lbl->SetLabel("No routes found — draw a route in OpenCPN first.");
        m_status_lbl->SetForegroundColour(wxColour(200, 100, 100));
        return;
    }

    m_route_choice->Enable();
    m_analyse_btn->Enable();

    // Pre-select: prefer the navigation-active route, fall back to last known GUID
    wxString active_guid = GetActiveRouteGUID();
    if (active_guid.IsEmpty()) active_guid = m_route_guid;

    int sel = 0;  // default to first route
    if (!active_guid.IsEmpty()) {
        for (size_t i = 0; i < m_route_guids.GetCount(); ++i) {
            if (m_route_guids[i] == active_guid) {
                sel = static_cast<int>(i);
                break;
            }
        }
    }

    m_route_choice->SetSelection(sel);
    m_route_guid = m_route_guids[sel];

    m_status_lbl->SetLabel(
        wxString::Format("Ready — %s", m_route_choice->GetString(sel)));
    m_status_lbl->SetForegroundColour(wxColour(180, 220, 180));
}

// ---------------------------------------------------------------------------
// ↺ Refresh button
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnRefreshRoutes(wxCommandEvent&) {
    RefreshRouteList();
}

// ---------------------------------------------------------------------------
// Route choice dropdown changed
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnRouteSelected(wxCommandEvent&) {
    int sel = m_route_choice->GetSelection();
    if (sel == wxNOT_FOUND || sel >= (int)m_route_guids.GetCount()) return;
    m_route_guid = m_route_guids[sel];
    m_status_lbl->SetLabel(
        wxString::Format("Ready — %s", m_route_choice->GetString(sel)));
    m_status_lbl->SetForegroundColour(wxColour(180, 220, 180));
    m_results_list->DeleteAllItems();
}

// ---------------------------------------------------------------------------
// Settings button
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnApplySettings(wxCommandEvent&) {
    wxString url = m_url_ctrl->GetValue().Trim();
    if (url.IsEmpty()) {
        wxMessageBox("Server URL cannot be empty.", "TidalPlan",
                     wxOK | wxICON_WARNING, this);
        return;
    }
    // Nothing else to do — the URL is read from m_url_ctrl whenever needed.
    m_status_lbl->SetLabel(
        wxString::Format("Server set to %s", url));
    m_status_lbl->SetForegroundColour(wxColour(100, 200, 100));
}

// ---------------------------------------------------------------------------
// Analyse button — core workflow
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnAnalyse(wxCommandEvent&) {
    // Sync GUID from the dropdown in case the user changed it without triggering EVT_CHOICE
    int sel = m_route_choice->GetSelection();
    if (m_route_guids.IsEmpty() || sel == wxNOT_FOUND ||
        sel >= (int)m_route_guids.GetCount()) {
        wxMessageBox(
            "No route selected.\n\n"
            "Click \xe2\x86\xba to refresh the route list, then select a route.",
            "TidalPlan", wxOK | wxICON_INFORMATION, this);
        return;
    }
    m_route_guid = m_route_guids[sel];

    m_analyse_btn->Disable();
    m_status_lbl->SetLabel("Contacting TidalPlan server…");
    m_status_lbl->SetForegroundColour(wxColour(200, 200, 100));
    wxYield();  // Let the UI repaint before blocking on HTTP

    wxString json_body = BuildRequestJson();
    if (json_body.IsEmpty()) {
        m_analyse_btn->Enable();
        return;
    }

    wxString response;
    if (!PostToServer(json_body, response)) {
        m_analyse_btn->Enable();
        return;
    }

    m_last_response = response;
    PopulateResults(response);
    m_analyse_btn->Enable();
}

// ---------------------------------------------------------------------------
// BuildRequestJson
//
// Reads waypoints from the currently active OpenCPN route via the plugin API
// and builds the JSON body expected by /api/route/analyse-json:
//
//   {
//     "waypoints":        [[lat, lon], ...],
//     "vessel_speed":     6.5,
//     "start_datetime":   "2026-05-21T06:00:00",
//     "end_datetime":     "2026-05-23T06:00:00",
//     "interval_minutes": 30,
//     "top_n":            20
//   }
// ---------------------------------------------------------------------------
wxString TidalPlanPanel::BuildRequestJson() {
    // GetRoute_Plugin returns std::unique_ptr<PlugIn_Route> — auto-deleted on scope exit.
    // PlugIn_Route::pWaypointList is Plugin_WaypointList* (WX_DECLARE_LIST of PlugIn_Waypoint).
    auto route = GetRoute_Plugin(m_route_guid);
    if (!route) {
        wxMessageBox(
            "Could not read the active route.\n"
            "It may have been deleted or OpenCPN API version mismatch.",
            "TidalPlan", wxOK | wxICON_ERROR, this);
        return "";
    }

    if (!route->pWaypointList || route->pWaypointList->GetCount() < 2) {
        wxMessageBox("Route needs at least 2 waypoints.", "TidalPlan",
                     wxOK | wxICON_WARNING, this);
        return "";
    }

    // Build waypoints JSON array by walking the Plugin_WaypointList linked list.
    // Plugin_WaypointList is a WX_DECLARE_LIST(PlugIn_Waypoint,...) — iterate
    // with compatibility_iterator (the standard wxList node iterator).
    wxString wps;
    Plugin_WaypointList::compatibility_iterator node =
        route->pWaypointList->GetFirst();
    while (node) {
        PlugIn_Waypoint* wp = node->GetData();
        if (wp) {
            if (!wps.IsEmpty()) wps += ",";
            wps += wxString::Format("[%.6f,%.6f]", wp->m_lat, wp->m_lon);
        }
        node = node->GetNext();
    }
    // unique_ptr auto-deletes route here

    // Departure window: apply user-selected hours to the chosen dates
    wxDateTime start_wx = m_start_date->GetValue();
    wxDateTime end_wx   = m_end_date->GetValue();
    start_wx.SetHour(m_start_hour->GetValue()); start_wx.SetMinute(0); start_wx.SetSecond(0);
    end_wx  .SetHour(m_end_hour->GetValue());   end_wx  .SetMinute(59); end_wx.SetSecond(0);

    wxString start_iso = start_wx.Format("%Y-%m-%dT%H:%M:%S");
    wxString end_iso   = end_wx  .Format("%Y-%m-%dT%H:%M:%S");
    double   speed     = m_speed_ctrl->GetValue();

    wxString json = wxString::Format(
        "{"
        "\"waypoints\":[%s],"
        "\"vessel_speed\":%.2f,"
        "\"start_datetime\":\"%s\","
        "\"end_datetime\":\"%s\","
        "\"interval_minutes\":30,"
        "\"top_n\":20"
        "}",
        wps, speed, start_iso, end_iso
    );

    return json;
}

// ---------------------------------------------------------------------------
// PostToServer — HTTP POST using wxHTTP (part of wxWidgets net component)
// ---------------------------------------------------------------------------
bool TidalPlanPanel::PostToServer(const wxString& json_body,
                                   wxString& response) {
    wxString url_str = GetServerUrl();

    // Strip trailing slash
    if (url_str.EndsWith("/")) url_str.RemoveLast();

    // Parse out host and optional port
    // Expected format: http://host:port  or  http://host
    wxString host = url_str;
    int port = 8081;

    host.Replace("http://",  "", false);
    host.Replace("https://", "", false);

    int colon = host.Find(':');
    if (colon != wxNOT_FOUND) {
        long p;
        if (host.Mid(colon + 1).ToLong(&p)) port = static_cast<int>(p);
        host = host.Left(colon);
    }

    wxHTTP http;
    http.SetHeader("Content-Type", "application/json");
    http.SetHeader("Accept",       "application/json");
    http.SetPostText("application/json", json_body);
    http.SetTimeout(30);  // seconds

    if (!http.Connect(host, static_cast<unsigned short>(port))) {
        wxMessageBox(
            wxString::Format(
                "Cannot connect to TidalPlan server at %s:%d\n\n"
                "Check that:\n"
                "  • The Pi is powered on and reachable\n"
                "  • You are on the boat's Wi-Fi network\n"
                "  • The server URL is correct",
                host, port),
            "TidalPlan — Connection Error", wxOK | wxICON_ERROR, this);
        return false;
    }

    wxInputStream* stream = http.GetInputStream("/api/route/analyse-json");
    if (!stream) {
        wxMessageBox("TidalPlan server returned no response.",
                     "TidalPlan", wxOK | wxICON_ERROR, this);
        return false;
    }

    int http_code = http.GetResponse();
    wxStringOutputStream ss(&response);
    stream->Read(ss);
    delete stream;

    if (http_code != 200) {
        wxString detail = response.Left(256);
        wxMessageBox(
            wxString::Format("Server error %d:\n%s", http_code, detail),
            "TidalPlan", wxOK | wxICON_ERROR, this);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// PopulateResults — parse JSON response and fill the wxListCtrl
//
// The /api/route/analyse-json response looks like:
//   {
//     "waypoint_count": 4,
//     "windows_tested": 97,
//     "results": [
//       {
//         "rank": 1,
//         "departure": "2026-05-21T06:00:00+00:00",
//         "eta":       "2026-05-21T14:30:00+00:00",
//         "passage_hours": 8.5,
//         "score": 82.3,
//         "score_label": "Excellent",
//         "legs": [...]
//       },
//       ...
//     ]
//   }
// ---------------------------------------------------------------------------
void TidalPlanPanel::PopulateResults(const wxString& json) {
    m_results_list->DeleteAllItems();

    if (json.find("\"results\"") == wxString::npos) {
        m_status_lbl->SetLabel("Unexpected response from server.");
        return;
    }

    // Extract windows_tested count for the status line
    wxString tested = "?";
    {
        size_t k = json.find("\"windows_tested\"");
        if (k != wxString::npos) {
            size_t colon = json.find(':', k);
            size_t comma = json.find(',', colon + 1);
            if (colon != wxString::npos && comma != wxString::npos)
                tested = json.Mid(colon + 1, comma - colon - 1).Trim();
        }
    }

    // Walk results array — each entry is delimited by "rank" keys.
    // Use size_t throughout to avoid int/npos sign-mismatch warnings.
    int    row         = 0;
    size_t search_from = json.find("\"results\"");

    while (true) {
        size_t next_rank = json.find("\"rank\"", search_from);
        if (next_rank == wxString::npos) break;

        size_t chunk_end = json.find("\"rank\"", next_rank + 6);
        wxString chunk   = (chunk_end == wxString::npos)
                           ? json.Mid(next_rank)
                           : json.Mid(next_rank, chunk_end - next_rank);

        int    rank    = (int)JsonGetDouble(chunk, "rank",         row + 1);
        double score   = JsonGetDouble(chunk, "score",             0.0);
        double hrs     = JsonGetDouble(chunk, "passage_hours",     0.0);
        wxString depart = JsonGetString(chunk, "departure");
        wxString eta    = JsonGetString(chunk, "eta");

        // ISO → "Wed 21 Jun 06:00"
        // Strip timezone suffix (+00:00 or Z) AND sub-seconds (.ffffff)
        // before handing to wxDateTime, which doesn't understand either.
        auto shorten_iso = [](const wxString& iso) -> wxString {
            wxDateTime dt;
            wxString clean = iso.BeforeFirst('+').BeforeFirst('Z').BeforeFirst('.');
            if (dt.ParseISOCombined(clean))
                return dt.Format("%a %d %b %H:%M");
            return iso.Left(16);
        };

        // ★ rating
        wxString stars;
        if      (score >= 75) stars = wxString::FromUTF8("\xe2\x98\x85\xe2\x98\x85\xe2\x98\x85\xe2\x98\x85"); // ★★★★
        else if (score >= 50) stars = wxString::FromUTF8("\xe2\x98\x85\xe2\x98\x85\xe2\x98\x85\xe2\x98\x86"); // ★★★☆
        else if (score >= 25) stars = wxString::FromUTF8("\xe2\x98\x85\xe2\x98\x85\xe2\x98\x86\xe2\x98\x86"); // ★★☆☆
        else                  stars = wxString::FromUTF8("\xe2\x98\x85\xe2\x98\x86\xe2\x98\x86\xe2\x98\x86"); // ★☆☆☆

        long idx = m_results_list->InsertItem(row, wxString::Format("%d", rank));
        m_results_list->SetItem(idx, 1, shorten_iso(depart));
        m_results_list->SetItem(idx, 2, shorten_iso(eta));
        m_results_list->SetItem(idx, 3, wxString::Format("%.1f", hrs));
        m_results_list->SetItem(idx, 4, wxString::Format("%s %.0f", stars, score));

        wxColour c;
        if      (score >= 75) c = wxColour(40, 100, 40);   // dark green
        else if (score >= 50) c = wxColour(80, 80,  20);   // dark yellow
        else if (score >= 25) c = wxColour(80, 50,  20);   // dark orange
        else                  c = wxColour(80, 30,  30);   // dark red

        m_results_list->SetItemBackgroundColour(idx, c);
        m_results_list->SetItemTextColour(idx, *wxWHITE);

        ++row;
        search_from = (chunk_end == wxString::npos) ? json.Length() : chunk_end;
    }

    if (row == 0) {
        m_status_lbl->SetLabel("No departure windows returned by server.");
        return;
    }

    wxListItem li;
    li.SetId(0); li.SetColumn(1); li.SetMask(wxLIST_MASK_TEXT);
    m_results_list->GetItem(li);

    m_status_lbl->SetLabel(wxString::Format(
        "Best: %s  (%s windows tested)", li.GetText(), tested));
    m_status_lbl->SetForegroundColour(wxColour(100, 220, 100));
    m_footer_lbl->SetLabel(
        wxString::Format("%d results shown. Double-click for leg details.", row));
}

// ---------------------------------------------------------------------------
// Double-click a result row — show leg breakdown
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnListItemActivated(wxListEvent& event) {
    if (m_last_response.IsEmpty()) return;

    long row_idx = event.GetIndex();

    // Re-extract the Nth result object from the stored JSON
    size_t search_from = 0;
    wxString chunk;
    for (long r = 0; r <= row_idx; ++r) {
        size_t next_rank = m_last_response.find("\"rank\"", search_from);
        if (next_rank == wxString::npos) return;
        size_t chunk_end = m_last_response.find("\"rank\"", next_rank + 6);
        chunk = (chunk_end == wxString::npos)
                ? m_last_response.Mid(next_rank)
                : m_last_response.Mid(next_rank, chunk_end - next_rank);
        search_from = (chunk_end == wxString::npos)
                      ? m_last_response.Length()
                      : chunk_end;
    }

    // Build human-readable leg summary
    wxString summary;
    summary += wxString::Format("Departure:  %s\n", JsonGetString(chunk, "departure").Left(16).BeforeFirst('.'));
    summary += wxString::Format("ETA:        %s\n", JsonGetString(chunk, "eta").Left(16).BeforeFirst('.'));
    summary += wxString::Format("Passage:    %.1f hours\n", JsonGetDouble(chunk, "passage_hours"));
    summary += wxString::Format("Score:      %.0f  (%s)\n\n",
                                JsonGetDouble(chunk, "score"),
                                JsonGetString(chunk, "score_label"));
    summary += "Legs:\n";

    // Find each leg block
    size_t leg_search = 0;
    int    leg_num    = 1;
    while (true) {
        size_t leg_pos = chunk.find("\"leg\":", leg_search);
        if (leg_pos == wxString::npos) break;

        size_t next_leg  = chunk.find("\"leg\":", leg_pos + 6);
        wxString leg_chunk = (next_leg == wxString::npos)
                             ? chunk.Mid(leg_pos)
                             : chunk.Mid(leg_pos, next_leg - leg_pos);

        double   dist  = JsonGetDouble(leg_chunk, "distance_nm");
        double   hdg   = JsonGetDouble(leg_chunk, "heading");
        double   comp  = JsonGetDouble(leg_chunk, "stream_component_kt");
        wxString src   = JsonGetString(leg_chunk, "source");

        wxString tide_str = (comp >= 0)
            ? wxString::Format("+%.1f kt fair", comp)
            : wxString::Format("%.1f kt foul",  comp);

        summary += wxString::Format(
            "  Leg %d: %.1f nm  hdg %.0f\xc2\xb0  tide %s  [%s]\n",
            leg_num++, dist, hdg, tide_str, src);

        leg_search = (next_leg == wxString::npos) ? chunk.Length() : next_leg;
    }

    wxMessageBox(summary, "TidalPlan — Leg Details",
                 wxOK | wxICON_INFORMATION, this);
}

// ---------------------------------------------------------------------------
// Close handler — hide rather than destroy so the plugin pointer stays valid.
// Destroying the wxFrame would leave m_panel in the plugin as a dangling
// pointer, crashing OpenCPN on the next toolbar click.
// ---------------------------------------------------------------------------
void TidalPlanPanel::OnClose(wxCloseEvent& event) {
    if (event.CanVeto()) {
        Hide();
        event.Veto();   // cancel the destroy
    } else {
        Destroy();      // forced close (e.g. OS shutdown) — allow it
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
wxString TidalPlanPanel::GetServerUrl() const {
    return m_url_ctrl->GetValue().Trim();
}

double TidalPlanPanel::GetVesselSpeed() const {
    return m_speed_ctrl->GetValue();
}

// ---------------------------------------------------------------------------
// Minimal JSON helpers
// Extracts values from flat JSON strings.  For nested structures these are
// not general-purpose — they just look for the key and extract the value.
// ---------------------------------------------------------------------------
wxString TidalPlanPanel::JsonGetString(const wxString& json, const wxString& key) {
    wxString search = "\"" + key + "\"";
    size_t key_pos = json.find(search);
    if (key_pos == wxString::npos) return "";

    size_t colon = json.find(':', key_pos + search.length());
    if (colon == wxString::npos) return "";

    // Skip whitespace
    size_t val_start = colon + 1;
    while (val_start < json.length() && json[val_start] == ' ') ++val_start;

    if (json[val_start] != '"') return "";
    size_t q1 = val_start + 1;
    size_t q2 = json.find('"', q1);
    if (q2 == wxString::npos) return "";

    return json.Mid(q1, q2 - q1);
}

double TidalPlanPanel::JsonGetDouble(const wxString& json, const wxString& key,
                                      double fallback) {
    wxString search = "\"" + key + "\"";
    size_t key_pos = json.find(search);
    if (key_pos == wxString::npos) return fallback;

    size_t colon = json.find(':', key_pos + search.length());
    if (colon == wxString::npos) return fallback;

    size_t val_start = colon + 1;
    while (val_start < json.length() &&
           (json[val_start] == ' ' || json[val_start] == '\n')) {
        ++val_start;
    }

    // Read until comma, close-brace, or close-bracket
    size_t val_end = val_start;
    while (val_end < json.length() &&
           json[val_end] != ',' && json[val_end] != '}' && json[val_end] != ']') {
        ++val_end;
    }

    double result = fallback;
    json.Mid(val_start, val_end - val_start).ToCDouble(&result);
    return result;
}
