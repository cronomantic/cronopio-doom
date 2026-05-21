// [cronopio] Port-local input stub for Crispy Doom.
// The real src/i_input.c is excluded (SDL keyboard/mouse). Cronopio feeds
// input through its own path; here we define the config globals the kept
// engine code references and stub the I_*Input/text-input entry points.

#include "doomtype.h"
#include "m_config.h"

int novert = 1;
// vanilla_keyboard_mapping is defined in i_video_cron.c
extern int vanilla_keyboard_mapping;
float mouse_acceleration = 2.0f;
int mouse_threshold = 10;
float mouse_acceleration_y = 1.0f;
int mouse_threshold_y = 0;
int mouse_y_invert = 0;
int runcentering = 1;

void I_BindInputVariables(void)
{
    M_BindFloatVariable("mouse_acceleration",      &mouse_acceleration);
    M_BindIntVariable("mouse_threshold",           &mouse_threshold);
    M_BindIntVariable("vanilla_keyboard_mapping",  &vanilla_keyboard_mapping);
    M_BindIntVariable("novert",                    &novert);
    M_BindFloatVariable("mouse_acceleration_y",    &mouse_acceleration_y);
    M_BindIntVariable("mouse_threshold_y",         &mouse_threshold_y);
    M_BindIntVariable("mouse_y_invert",            &mouse_y_invert);
}

void I_StartTextInput(int x1, int y1, int x2, int y2)
{
    (void)x1; (void)y1; (void)x2; (void)y2;
}

void I_StopTextInput(void)
{
}

void I_ReadMouse(void)
{
}

// [cronopio] Mouse acceleration. The vendored i_input.c versions return double
// (rejected by the translator: no f64) and read accumulated mouse motion. With
// no mouse on Cronopio the accumulated motion is always 0, so acceleration
// never engages and the result is just (float)val. Declared `float` to match
// i_input.h (the .c's `double` definition is a vendor inconsistency).
float I_AccelerateMouse(int val)
{
    return (float)val;
}

float I_AccelerateMouseY(int val)
{
    return (float)val;
}
