// Minimal smoke-test plugin — no panel, no wx calls, no Init() logic.
// If this loads, the build/ABI is correct and the crash is in our real code.
// If this also crashes, the problem is fundamental (linking, vtable, etc.).

#include "ocpn_plugin.h"

class TidalPlanPlugin : public opencpn_plugin_118 {
public:
    explicit TidalPlanPlugin(void* ppimgr)
        : opencpn_plugin_118(ppimgr) {}

    int  Init()                  override { return WANTS_TOOLBAR_CALLBACK; }
    bool DeInit()                override { return true; }

    int GetAPIVersionMajor()     override { return 1; }
    int GetAPIVersionMinor()     override { return 18; }
    int GetPlugInVersionMajor()  override { return 1; }
    int GetPlugInVersionMinor()  override { return 0; }

    wxBitmap* GetPlugInBitmap()  override {
        static wxBitmap bmp(32, 32);
        return &bmp;
    }

    wxString GetCommonName()         override { return "TidalPlan"; }
    wxString GetShortDescription()   override { return "TidalPlan tidal analyser"; }
    wxString GetLongDescription()    override { return "TidalPlan"; }

    void OnToolbarToolCallback(int) override {}
};

extern "C" {
    DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
        return new TidalPlanPlugin(ppimgr);
    }
    DECL_EXP void destroy_pi(opencpn_plugin* p) {
        delete p;
    }
}
