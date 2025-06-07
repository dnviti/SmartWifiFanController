#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "config.h"

// FIX: Replaced all LCD functions with a single, comprehensive display update function for the OLED.
/**
 * @brief Updates the OLED display.
 * * This function is the single point of entry for all display updates.
 * It checks the current system state (e.g., isInMenuMode, currentMenuScreen)
 * and renders the appropriate screen on the OLED.
 */
void updateDisplay();

#endif // DISPLAY_HANDLER_H
