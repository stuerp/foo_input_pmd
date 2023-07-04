
/** $VER: Component.cpp (2023.07.04) **/

#pragma warning(disable: 5045)

#include <sdk/foobar2000-lite.h>
#include <sdk/componentversion.h>

#include "framework.h"

#include "Resources.h"

#pragma hdrstop

namespace
{
    #pragma warning(disable: 4265 4625 4626 5026 5027 26433 26436 26455)
    DECLARE_COMPONENT_VERSION
    (
        STR_COMPONENT_NAME,
        STR_COMPONENT_VERSION,
        STR_COMPONENT_BASENAME " " STR_COMPONENT_VERSION "\n"
            STR_COMPONENT_COPYRIGHT "\n"
            STR_COMPONENT_COMMENTS "\n"
            "\n"
            STR_COMPONENT_DESCRIPTION "\n"
            "\n"
            "Built with foobar2000 SDK " TOSTRING(FOOBAR2000_SDK_VERSION) "\n"
            "on " __DATE__ " " __TIME__ ".\n"
    )
    VALIDATE_COMPONENT_FILENAME(STR_COMPONENT_FILENAME)
}
