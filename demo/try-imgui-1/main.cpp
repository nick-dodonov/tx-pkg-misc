#include "Boot/Boot.h"
#include "Log/Log.h"
#include "imgui.h"

int main(int argc, const char** argv)
{
    Boot::LogHeader(argc, argv);
    Log::Info("ImGUI 1st try demo");
    Log::Info("ImGUI version: {}", ImGui::GetVersion());
    return 0;
}
