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

#else // JIANGTUN_CONFIG_BOARD_PICO

#define PIN_RESET 3
#define PIN_SERVO 6
#define PIN_GAMECUBE 5

#endif

#define initLED()                       \
    do                                  \
    {                                   \
        pinMode(LED_BUILTIN, OUTPUT);   \
        digitalWrite(LED_BUILTIN, LOW); \
    } while (0)
#define turnOnLED() (digitalWrite(LED_BUILTIN, HIGH))
#define turnOffLED() (digitalWrite(LED_BUILTIN, LOW))

/*
 * core0 deserializes serial commands and pushes them into a ring queue.
 * core1 pops one command per GameCube poll and answers the poll.
 * Every queued command is written to the GameCube at least once, so
 * short presses can no longer vanish while core1 is blocked inside
 * gamecube.write() waiting for the console to poll.
 */
static mutex_t mtx;
static constexpr size_t kQueueSize = 64;
static jiangtun::Command queue_[kQueueSize];
static size_t queueHead = 0;  // next entry core1 will send
static size_t queueCount = 0; // number of queued entries

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

// core0-only parser state
static Gamecube_Report_t report_work;
static nthaka_button_state_t prev_home = NTHAKA_BUTTON_RELEASED;
static uint32_t last_byte_millis = 0;
static bool buf_pending = false;

static int64_t _turnOffLED(alarm_id_t _0, void *_1)
{
    turnOffLED();
    return 0;
}

static inline void blinkLEDAsync()
{
    turnOnLED();
    add_alarm_in_ms(100, _turnOffLED, nullptr, false);
}

static jiangtun::Command makeCommand(nthaka_gamepad_state_t &gamepad, size_t idx)
{
    jiangtun::Command cmd;

    // Determine the reset action from the home button edge.
    cmd.action = jiangtun::ResetAction::Nothing;
    nthaka_button_state_t next_home = gamepad.home;
    if (prev_home != next_home)
    {
        if (next_home == NTHAKA_BUTTON_PRESSED)
        {
            cmd.action = jiangtun::ResetAction::Press;

            for (size_t i = 0; i < auto_reset_release_idx_size; i++)
            {
                if (auto_reset_release_idx[i] == idx)
                {
                    cmd.action = jiangtun::ResetAction::PressRelease;
                    // These formats never send an explicit release, so
                    // rearm the edge detector for the next press.
                    next_home = NTHAKA_BUTTON_RELEASED;
                    break;
                }
            }
        }
        else
        {
            cmd.action = jiangtun::ResetAction::Release;
        }
        prev_home = next_home;
    }

    // Convert nthaka_gamepad_state_t to Gamecube_Report_t
    report_work.y = gamepad.y == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.b = gamepad.b == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.a = gamepad.a == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.x = gamepad.x == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.l = gamepad.l == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.r = gamepad.r == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.z = gamepad.zr == NTHAKA_BUTTON_PRESSED ? 1U : 0U;
    report_work.start = gamepad.plus == NTHAKA_BUTTON_PRESSED ? 1U : 0U;

    switch (gamepad.hat)
    {
    case NTHAKA_HAT_UP:
        report_work.dup = 1U;
        report_work.dright = 0U;
        report_work.ddown = 0U;
        report_work.dleft = 0U;
        break;

    case NTHAKA_HAT_UPRIGHT:
        report_work.dup = 1U;
        report_work.dright = 1U;
        report_work.ddown = 0U;
        report_work.dleft = 0U;
        break;

    case NTHAKA_HAT_RIGHT:
        report_work.dup = 0U;
        report_work.dright = 1U;
        report_work.ddown = 0U;
        report_work.dleft = 0U;
        break;

    case NTHAKA_HAT_DOWNRIGHT:
        report_work.dup = 0U;
        report_work.dright = 1U;
        report_work.ddown = 1U;
        report_work.dleft = 0U;
        break;

    case NTHAKA_HAT_DOWN:
        report_work.dup = 0U;
        report_work.dright = 0U;
        report_work.ddown = 1U;
        report_work.dleft = 0U;
        break;

    case NTHAKA_HAT_DOWNLEFT:
        report_work.dup = 0U;
        report_work.dright = 0U;
        report_work.ddown = 1U;
        report_work.dleft = 1U;
        break;

    case NTHAKA_HAT_LEFT:
        report_work.dup = 0U;
        report_work.dright = 0U;
        report_work.ddown = 0U;
        report_work.dleft = 1U;
        break;

    case NTHAKA_HAT_UPLEFT:
        report_work.dup = 1U;
        report_work.dright = 0U;
        report_work.ddown = 0U;
        report_work.dleft = 1U;
        break;

    case NTHAKA_HAT_NEUTRAL:
    default:
        report_work.dup = 0U;
        report_work.dright = 0U;
        report_work.ddown = 0U;
        report_work.dleft = 0U;
        break;
    }

    // There are a few games that do not handle yAxis=0 and cyAxis=0 correctly.
    report_work.xAxis = gamepad.l_stick.x;
    uint8_t y_axis = 0xFF - gamepad.l_stick.y;
    report_work.yAxis = y_axis == 0U ? 1U : y_axis;

    report_work.cxAxis = gamepad.r_stick.x;
    uint8_t cy_axis = 0xFF - gamepad.r_stick.y;
    report_work.cyAxis = cy_axis == 0U ? 1U : cy_axis;

    cmd.report = report_work;
    return cmd;
}

static void pushCommand(const jiangtun::Command &cmd)
{
    mutex_enter_blocking(&mtx);
    if (queueCount < kQueueSize)
    {
        queue_[(queueHead + queueCount) % kQueueSize] = cmd;
        queueCount++;
    }
    else
    {
        // Queue overflow: coalesce into the newest entry so that presses
        // are kept (OR) and analog values reflect the latest command.
        jiangtun::Command &last = queue_[(queueHead + queueCount - 1) % kQueueSize];
        last.report.a |= cmd.report.a;
        last.report.b |= cmd.report.b;
        last.report.x |= cmd.report.x;
        last.report.y |= cmd.report.y;
        last.report.l |= cmd.report.l;
        last.report.r |= cmd.report.r;
        last.report.z |= cmd.report.z;
        last.report.start |= cmd.report.start;
        last.report.dup |= cmd.report.dup;
        last.report.ddown |= cmd.report.ddown;
        last.report.dleft |= cmd.report.dleft;
        last.report.dright |= cmd.report.dright;
        last.report.xAxis = cmd.report.xAxis;
        last.report.yAxis = cmd.report.yAxis;
        last.report.cxAxis = cmd.report.cxAxis;
        last.report.cyAxis = cmd.report.cyAxis;
        last.report.left = cmd.report.left;
        last.report.right = cmd.report.right;
        if (cmd.action != jiangtun::ResetAction::Nothing)
        {
            last.action = cmd.action;
        }
    }
    mutex_exit(&mtx);
}

void setup()
{
    mutex_init(&mtx);

    Serial.begin(9600);

    initLED();

    dol_format_handler_init(&dol);
    nxmc2_format_handler_init(&nxmc2);
    orca_format_handler_init(&orca);
    pokecon_format_handler_init(&pokecon);
    nthaka_multi_format_handler_init(&fmt, fmts, 3);
    nthaka_buffer_init(&buf, (nthaka_format_handler_t *)&fmt);

    report_work = defaultGamecubeData.report;
}

void loop()
{
    static nthaka_gamepad_state_t out;

    // Discard a stalled partial packet so a desynced stream can recover.
    if (buf_pending && (uint32_t)(millis() - last_byte_millis) > 100)
    {
        nthaka_buffer_clear(&buf);
        buf_pending = false;
    }

    while (Serial.available() > 0)
    {
        int c = Serial.read();
        if (c < 0)
        {
            break;
        }
        last_byte_millis = millis();

        nthaka_buffer_state_t s = nthaka_buffer_append(&buf, (uint8_t)c, &out);
        if (s == NTHAKA_BUFFER_REJECTED)
        {
            nthaka_buffer_clear(&buf);
            buf_pending = false;
            continue;
        }
        if (s == NTHAKA_BUFFER_PENDING)
        {
            buf_pending = true;
            continue;
        }

        // NTHAKA_BUFFER_ACCEPTED
        size_t *idx_ = nthaka_multi_format_handler_get_last_deserialized_index(&fmt);
        size_t idx = idx_ != nullptr ? *idx_ : 0;

        pushCommand(makeCommand(out, idx));
        blinkLEDAsync();

        nthaka_buffer_clear(&buf);
        buf_pending = false;
    }
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

// core1-only. Holds status/origin and the last sent report; rumble state
// written back by gamecube.write() is preserved across polls.
static Gamecube_Data_t gc_data = defaultGamecubeData;

static void initGamecube(CGamecubeConsole &console, Gamecube_Data_t &data)
{
    data.report.a = 0;
    data.report.b = 0;
    data.report.x = 0;
    data.report.y = 0;
    data.report.start = 0;
    data.report.dleft = 0;
    data.report.dright = 0;
    data.report.ddown = 0;
    data.report.dup = 0;
    data.report.z = 0;
    data.report.r = 0;
    data.report.l = 0;
    data.report.xAxis = 128;
    data.report.yAxis = 128;
    data.report.cxAxis = 128;
    data.report.cyAxis = 128;
    data.report.left = 0;
    data.report.right = 0;

    // Magic spell to make the controller be recognized by the Gamecube
    data.report.start = 1;
    console.write(data);
    data.report.start = 0;
    console.write(data);
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
    return 0;
}

void setup1()
{
    // Wait `mutex_init(&mtx);`
    delay(10);

    initGamecube(gamecube, gc_data);

    servo.attach(PIN_SERVO, 500, 2400);
    pinMode(PIN_RESET, INPUT);
    releaseReset(0, nullptr);
}

void loop1()
{
    jiangtun::Command cmd;
    bool has_cmd = false;

    mutex_enter_blocking(&mtx);
    if (queueCount > 0)
    {
        cmd = queue_[queueHead];
        has_cmd = true;
    }
    mutex_exit(&mtx);

    if (has_cmd)
    {
        gc_data.report = cmd.report;
    }

    bool ok = gamecube.write(gc_data);

    // Advance the queue only after the report actually reached the console;
    // on failure the same command is retried on the next poll.
    if (!ok || !has_cmd)
    {
        return;
    }

    mutex_enter_blocking(&mtx);
    queueHead = (queueHead + 1) % kQueueSize;
    queueCount--;
    mutex_exit(&mtx);

    switch (cmd.action)
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
