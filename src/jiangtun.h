#ifndef JIANGTUN_H_
#define JIANGTUN_H_

#include "Bluewhale.h"

namespace jiangtun
{

    enum class ResetAction
    {
        Press,
        Release,
        PressRelease,
        Nothing
    };

    // One deserialized serial command, queued from core0 to core1.
    // Each queued command is guaranteed to be written to the GameCube
    // at least once.
    struct Command
    {
        Gamecube_Report_t report;
        ResetAction action;
    };

}

#endif // JIANGTUN_H_
