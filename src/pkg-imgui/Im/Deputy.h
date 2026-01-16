#pragma once
#include "imgui.h"

namespace Im
{
    class Deputy
    {
    public:
        Deputy();
        ~Deputy();
    private:
        ImGuiIO* _imGuiIO = nullptr;
    };
}
