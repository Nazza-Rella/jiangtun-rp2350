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
        nthaka_button_state_t gc_reset;

        ResetAction next_action;

        uint16_t hue;
        uint32_t color;
    };

}

#endif // JIANGTUN_H_