#include "Boot/Boot.h"
#include "Log/Log.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

int main(int argc, const char** argv)
{
    Boot::LogHeader({argc, argv});

    Log::Info("FOO: {}", STR(FOO));
    Log::Info("WORKSPACE_NAME: {}", STR(WORKSPACE_NAME));
    Log::Info("LABEL_NAME: {}", STR(LABEL_NAME));
    Log::Info("LABEL_PACKAGE_NAME: {}", STR(LABEL_PACKAGE_NAME));
    Log::Info("LABEL_WORKSPACE_NAME: {}", STR(LABEL_WORKSPACE_NAME));
    Log::Info("LABEL_REPO_NAME: {}", STR(LABEL_REPO_NAME));
    Log::Info("LABEL_WORKSPACE_ROOT: {}", STR(LABEL_WORKSPACE_ROOT));
    return argc - 1;
}
