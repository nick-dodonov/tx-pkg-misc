#include "imgui.h"

namespace Im
{
    //TODO: think to move default mono font management to Deputy instead of static caching in Ext.cpp
    ImFont* GetDefaultMonoFont();
    bool PushDefaultMonoFont();
    bool PopDefaultMonoFont();
}
