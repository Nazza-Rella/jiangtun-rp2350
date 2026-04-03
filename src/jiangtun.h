#ifndef JIANGTUN_H_
#define JIANGTUN_H_

#include <Servo.h>

#include <array>

#include "Bluewhale.h"
#include "nthaka.h"

namespace jiangtun
{

    enum class ResetAction
    {
        Press,
        Release,
        PressRelease,
        Nothing
    };

    struct State
    {
        Gamecube_Data_t gc_data;
        Gamecube_Data_t gc_data_held; // OR-accumulated button presses since last core1 read
        nthaka_button_state_t gc_reset;

        ResetAction next_action;
    };

}

#endif // JIANGTUN_H_