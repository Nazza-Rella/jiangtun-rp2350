#include <Arduino.h>
#include <Servo.h>

#include "pico/mutex.h"
#include "hardware/timer.h"

#include "Bluewhale.h"

#include "nthaka.h"
#include "nthaka/dol.h"
#include "nthaka/nxmc2.h"
#include "nthaka/orca.h"
#include "nthaka/pokecon.h"

#include "jiangtun.h"

#ifdef JIANGTUN_CONFIG_BOARD_XIAO_RP2350

#define PIN_RESET D10
#define PIN_SERVO D6
#define PIN_GAMECUBE D5

#define initLED()                       \
    do                                  \
    {                                   \
        pinMode(LED_BUILTIN, OUTPUT);   \
        digitalWrite(LED_BUILTIN, LOW); \
    } while (0)
#define turnOnLED(_) (digitalWrite(LED_BUILTIN, HIGH))
#define turnOffLED() (digitalWrite(LED_BUILTIN, LOW))

#else // JIANGTUN_CONFIG_BOARD_PICO

#define PIN_RESET 3
#define PIN_SERVO 6
#define PIN_GAMECUBE 5

#define initLED()                       \
    do                                  \
    {                                   \
        pinMode(LED_BUILTIN, OUTPUT);   \
        digitalWrite(LED_BUILTIN, LOW); \
    } while (0)
#define turnOnLED(_) (digitalWrite(LED_BUILTIN, HIGH))
#define turnOffLED() (digitalWrite(LED_BUILTIN, LOW))

#endif

static mutex_t mtx;
jiangtun::State state{.gc_data = defaultGamecubeData,
                      .gc_reset = NTHAKA_BUTTON_RELEASED,
                      .next_action = jiangtun::ResetAction::Nothing};

/*************************************************************************
 **                                                                     **
 **                                                   .oooo.            **
 **                                                  d8P'`Y8b           **
 **           .ooooo.   .ooooo.  oooo d8b  .ooooo.  888    888          **
 **          d88' `"Y8 d88' `88b `888""8P d88' `88b 888    888          **
 **          888       888   888  888     888ooo888 888    888          **
 **          888   .o8 888   888  888     888    .o `88b  d88'          **
 **          `Y8bod8P' `Y8bod8P' d888b    `Y8bod8P'  `Y8bd8P'           **
 **                                                                     **
 **                                                                     **
 **                                                                     **
 *************************************************************************/
// figlet -t -f roman core0

static dol_format_handler_t dol;
static nxmc2_format_handler_t nxmc2;
static orca_format_handler_t orca;
static pokecon_format_handler_t pokecon;
static nthaka_format_handler_t *fmts[] = {(nthaka_format_handler_t *)&nxmc2,
                                          (nthaka_format_handler_t *)&orca,
#ifdef JIANGTUN_CONFIG_ENABLE_DOL
                                          (nthaka_format_handler_t *)&dol
#else
                                          (nthaka_format_handler_t *)&pokecon
#endif // JIANGTUN_CONFIG_ENABLE_DOL
};
static size_t auto_reset_release_idx[] = {1,
#ifdef JIANGTUN_CONFIG_ENABLE_DOL
                                          2
#endif // JIANGTUN_CONFIG_ENABLE_DOL
};
static size_t auto_reset_release_idx_size = sizeof(auto_reset_release_idx) / sizeof(auto_reset_release_idx[0]);
static nthaka_multi_format_handler_t fmt;
static nthaka_buffer_t buf;

static int64_t _turnOffLED(alarm_id_t _0, void *_1)
{
    turnOffLED();
    return 0;
}

static inline void blinkLEDAsync()
{
    turnOnLED(0);
    add_alarm_in_ms(100, _turnOffLED, nullptr, false);
}

static void updateState(nthaka_gamepad_state_t &gamepad, size_t idx)
{
    // Update the reset state and determine the next reset action.
    nthaka_button_state_t next_reset_state = gamepad.home;
    if (state.gc_reset != next_reset_state)
    {
        if (next_reset_state == NTHAKA_BUTTON_PRESSED)
        {
            state.next_action = jiangtun::ResetAction::Press;

            for (size_t i = 0; i < auto_reset_release_idx_size; i++)
            {
                if (auto_reset_release_idx[i] == idx)
                {
                    state.next_action = jiangtun::ResetAction::PressRelease;
                    break;
                }
            }
        }
        else
        {
            state.next_action = jiangtun::ResetAction::Release;
        }
        state.gc_reset = next_reset_state;
    }
    else
    {
        state.next_action = jiangtun::ResetAction::Nothing;
    }

    // Convert nthaka_gamepad_state_t to Gamecube_Data_t
    state.gc_data.report.y = gamepad.y == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.b = gamepad.b == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.a = gamepad.a == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.x = gamepad.x == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.l = gamepad.l == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.r = gamepad.r == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.z = gamepad.zr == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    state.gc_data.report.start = gamepad.plus == NTHAKA_BUTTON_PRESSED ? 1U : 0U;

    switch (gamepad.hat)
    {
    case NTHAKA_HAT_UP:
        state.gc_data.report.dup = 1U;
        state.gc_data.report.dright = 0U;
        state.gc_data.report.ddown = 0U;
        state.gc_data.report.dleft = 0U;
        break;

    case NTHAKA_HAT_UPRIGHT:
        state.gc_data.report.dup = 1U;
        state.gc_data.report.dright = 1U;
        state.gc_data.report.ddown = 0U;
        state.gc_data.report.dleft = 0U;
        break;

    case NTHAKA_HAT_RIGHT:
        state.gc_data.report.dup = 0U;
        state.gc_data.report.dright = 1U;
        state.gc_data.report.ddown = 0U;
        state.gc_data.report.dleft = 0U;
        break;

    case NTHAKA_HAT_DOWNRIGHT:
        state.gc_data.report.dup = 0U;
        state.gc_data.report.dright = 1U;
        state.gc_data.report.ddown = 1U;
        state.gc_data.report.dleft = 0U;
        break;

    case NTHAKA_HAT_DOWN:
        state.gc_data.report.dup = 0U;
        state.gc_data.report.dright = 0U;
        state.gc_data.report.ddown = 1U;
        state.gc_data.report.dleft = 0U;
        break;

    case NTHAKA_HAT_DOWNLEFT:
        state.gc_data.report.dup = 0U;
        state.gc_data.report.dright = 0U;
        state.gc_data.report.ddown = 1U;
        state.gc_data.report.dleft = 1U;
        break;

    case NTHAKA_HAT_LEFT:
        state.gc_data.report.dup = 0U;
        state.gc_data.report.dright = 0U;
        state.gc_data.report.ddown = 0U;
        state.gc_data.report.dleft = 1U;
        break;

    case NTHAKA_HAT_UPLEFT:
        state.gc_data.report.dup = 1U;
        state.gc_data.report.dright = 0U;
        state.gc_data.report.ddown = 0U;
        state.gc_data.report.dleft = 1U;
        break;

    case NTHAKA_HAT_NEUTRAL:
    default:
        state.gc_data.report.dup = 0U;
        state.gc_data.report.dright = 0U;
        state.gc_data.report.ddown = 0U;
        state.gc_data.report.dleft = 0U;
        break;
    }

    // There are a few games that do not handle yAxis=0 and cyAxis=0 correctly.
    state.gc_data.report.xAxis = gamepad.l_stick.x;
    uint8_t y_axis = 0xFF - gamepad.l_stick.y;
    state.gc_data.report.yAxis = y_axis == 0U ? 1U : y_axis;

    state.gc_data.report.cxAxis = gamepad.r_stick.x;
    uint8_t cy_axis = 0xFF - gamepad.r_stick.y;
    state.gc_data.report.cyAxis = cy_axis == 0U ? 1U : cy_axis;
}

void setup()
{
    mutex_init(&mtx);

    Serial.setTimeout(100);
    Serial.begin(9600);

    initLED();

    dol_format_handler_init(&dol);
    nxmc2_format_handler_init(&nxmc2);
    orca_format_handler_init(&orca);
    pokecon_format_handler_init(&pokecon);
    nthaka_multi_format_handler_init(&fmt, fmts, 3);
    nthaka_buffer_init(&buf, (nthaka_format_handler_t *)&fmt);
}

void loop()
{
    static nthaka_gamepad_state_t out;

    uint8_t in;
    nthaka_buffer_state_t s;
    if (Serial.readBytes(&in, 1) != 1 ||
        (s = nthaka_buffer_append(&buf, in, &out)) == NTHAKA_BUFFER_REJECTED)
    {
        nthaka_buffer_clear(&buf);
        return;
    }
    else if (s == NTHAKA_BUFFER_PENDING)
    {
        return;
    }

    size_t *idx_ = nthaka_multi_format_handler_get_last_deserialized_index(&fmt);
    size_t idx = idx_ != nullptr ? *idx_ : 0;

    mutex_enter_blocking(&mtx);
    {
        blinkLEDAsync();
        updateState(out, idx);
    }
    mutex_exit(&mtx);

    nthaka_buffer_clear(&buf);
}

/*************************************************************************
 **                                                                     **
 **                                                   .o                **
 **                                                 o888                **
 **           .ooooo.   .ooooo.  oooo d8b  .ooooo.   888                **
 **          d88' `"Y8 d88' `88b `888""8P d88' `88b  888                **
 **          888       888   888  888     888ooo888  888                **
 **          888   .o8 888   888  888     888    .o  888                **
 **          `Y8bod8P' `Y8bod8P' d888b    `Y8bod8P' o888o               **
 **                                                                     **
 **                                                                     **
 **                                                                     **
 *************************************************************************/
// figlet -t -f roman core1

static CGamecubeConsole gamecube(PIN_GAMECUBE);

static Servo servo;

static void initGamecube(CGamecubeConsole &console, jiangtun::State &state)
{
    state.gc_data.report.a = 0;
    state.gc_data.report.b = 0;
    state.gc_data.report.x = 0;
    state.gc_data.report.y = 0;
    state.gc_data.report.start = 0;
    state.gc_data.report.dleft = 0;
    state.gc_data.report.dright = 0;
    state.gc_data.report.ddown = 0;
    state.gc_data.report.dup = 0;
    state.gc_data.report.z = 0;
    state.gc_data.report.r = 0;
    state.gc_data.report.l = 0;
    state.gc_data.report.xAxis = 128;
    state.gc_data.report.yAxis = 128;
    state.gc_data.report.cxAxis = 128;
    state.gc_data.report.cyAxis = 128;
    state.gc_data.report.left = 0;
    state.gc_data.report.right = 0;

    // Magic spell to make the controller be recognized by the Gamecube
    state.gc_data.report.start = 1;
    console.write(state.gc_data);
    state.gc_data.report.start = 0;
    console.write(state.gc_data);
}

static inline void pressReset()
{
    servo.write(65);
    pinMode(PIN_RESET, OUTPUT);
    digitalWrite(PIN_RESET, LOW);
}

static inline int64_t releaseReset(alarm_id_t _0, void *_1)
{
    servo.write(90);
    pinMode(PIN_RESET, INPUT);

    if (state.gc_reset != NTHAKA_BUTTON_RELEASED)
    {
        mutex_enter_blocking(&mtx);
        {
            state.gc_reset = NTHAKA_BUTTON_RELEASED;
            blinkLEDAsync();
        }
        mutex_exit(&mtx);
    }

    return 0;
}

void setup1()
{
    // Wait `mutex_init(&mtx);`
    delay(10);

    mutex_enter_blocking(&mtx);
    {
        initGamecube(gamecube, state);
    }
    mutex_exit(&mtx);

    servo.attach(PIN_SERVO, 500, 2400);
    pinMode(PIN_RESET, INPUT);
    releaseReset(0, nullptr);
}

void loop1()
{
    Gamecube_Data_t gc_data_copy;
    jiangtun::ResetAction next_action;

    // Copy state briefly under mutex, then release so core0 can update freely
    mutex_enter_blocking(&mtx);
    {
        gc_data_copy = state.gc_data;
        next_action = state.next_action;
        state.next_action = jiangtun::ResetAction::Nothing;
    }
    mutex_exit(&mtx);

    // GC communication blocks ~6ms waiting for console poll — no mutex held
    gamecube.write(gc_data_copy);

    // Write back rumble state from GC response
    mutex_enter_blocking(&mtx);
    {
        state.gc_data.status.rumble = gc_data_copy.status.rumble;
    }
    mutex_exit(&mtx);

    switch (next_action)
    {
    case jiangtun::ResetAction::Press:
        pressReset();
        break;

    case jiangtun::ResetAction::Release:
        releaseReset(0, nullptr);
        break;

    case jiangtun::ResetAction::PressRelease:
        pressReset();
        add_alarm_in_ms(500, releaseReset, nullptr, false);
        break;

    case jiangtun::ResetAction::Nothing:
    default:
        break;
    }
}