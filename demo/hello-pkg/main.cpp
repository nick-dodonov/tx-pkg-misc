#include "Boot/Boot.h"

int main(int argc, const char** argv)
{
    Boot::LogHeader({argc, argv});
    return argc - 1;
}
