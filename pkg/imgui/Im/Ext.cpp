#include "Ext.h"

namespace Im
{
    ImFont* GetDefaultMonoFont()
    {
        static ImGuiIO* _io = nullptr;
        static ImFont* _monoFont = nullptr;
        if (_io) {
            return _monoFont;
        }

        // Find monospace font (should be the second font loaded by Deputy)
        _io = &ImGui::GetIO();
        if (_io->Fonts->Fonts.Size > 1) {
            _monoFont = _io->Fonts->Fonts[1];
        }
        return _monoFont;
    }

    bool PushDefaultMonoFont()
    {
        if (auto* monoFont = GetDefaultMonoFont()) {
            ImGui::PushFont(monoFont);
            return true;
        }
        return false;
    }

    bool PopDefaultMonoFont()
    {
        if (GetDefaultMonoFont()) {
            ImGui::PopFont();
            return true;
        }
        return false;
    }
}
