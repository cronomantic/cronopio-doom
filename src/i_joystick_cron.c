// [cronopio] Port-local joystick stub for Crispy Doom.
// The real src/i_joystick.c is excluded (pulls in SDL gamecontroller).
// Cronopio has no joystick input, so we define only the config globals the
// kept engine code actually references (the analog-sensitivity knobs used in
// g_game.c) and stub the I_*Joystick entry points called from d_main.c.

#include "doomtype.h"
#include "m_config.h"

// Analog joystick config vars referenced by g_game.c. Defaults match Crispy.
int use_analog = 1;
int joystick_turn_sensitivity = 10;
int joystick_move_sensitivity = 10;
int joystick_look_sensitivity = 10;

void I_InitJoystick(void)
{
    // No joystick on Cronopio.
}

void I_ShutdownJoystick(void)
{
}

void I_UpdateJoystick(void)
{
}

void I_BindJoystickVariables(void)
{
    // Only register the knobs we keep so saved configs round-trip cleanly.
    M_BindIntVariable("use_analog", &use_analog);
    M_BindIntVariable("joystick_turn_sensitivity", &joystick_turn_sensitivity);
    M_BindIntVariable("joystick_move_sensitivity", &joystick_move_sensitivity);
    M_BindIntVariable("joystick_look_sensitivity", &joystick_look_sensitivity);
}
