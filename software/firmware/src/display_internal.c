#include "display_internal.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>
#include "display.h"
#include "keypad.h"
#include "u8g2.h"
#include "util.h"

#define MENU_KEY_POLL_MS 100

#define MY_BORDER_SIZE 1

#define MY_SPACE_BETWEEN_BUTTONS_IN_PIXEL 6
#define MY_SPACE_BETWEEN_TEXT_AND_BUTTONS_IN_PIXEL 3

extern osMutexId_t display_mutex;

bool menu_event_timeout = false;

/* Library function declarations */
void u8g2_DrawSelectionList(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, const char *s);
uint8_t u8g2_draw_button_line(u8g2_t *u8g2, u8g2_uint_t y, u8g2_uint_t w, uint8_t cursor, const char *s);

static void display_DrawSelectionList(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, const char *s, u8g2_uint_t list_width);
static u8g2_uint_t display_draw_selection_list_line(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, uint8_t idx, const char *s, u8g2_uint_t list_width);

uint16_t display_GetMenuEvent(u8x8_t *u8x8, display_menu_params_t params)
{
    uint16_t result = 0;

    /*
     * If we were called via a function that is holding the display mutex,
     * then release that mutex while blocked on the keypad queue.
     */
    osStatus_t mutex_released = osMutexRelease(display_mutex);

    int timeout;
    if (params & DISPLAY_MENU_INPUT_POLL) {
        timeout = MENU_KEY_POLL_MS;
    } else {
        timeout = ((params & DISPLAY_MENU_TIMEOUT_DISABLED) != 0) ? -1 : MENU_TIMEOUT_MS;
    }

    keypad_event_t event;
    HAL_StatusTypeDef ret = keypad_wait_for_event(&event, timeout);

    if (mutex_released == osOK) {
        osMutexAcquire(display_mutex, portMAX_DELAY);
    }

    if (ret == HAL_OK) {
        if (event.pressed) {
            /*
             * Button actions that stay within the menu are handled on
             * the press event
             */
            keypad_key_t keypad_key;
            if (event.key == KEYPAD_USB_KEYBOARD) {
                keypad_key = keypad_usb_get_keypad_equivalent(&event);
            } else {
                keypad_key = event.key;
            }
            switch (keypad_key) {
            case KEYPAD_DEC_CONTRAST:
                result = U8X8_MSG_GPIO_MENU_PREV;
                break;
            case KEYPAD_INC_CONTRAST:
                result = U8X8_MSG_GPIO_MENU_NEXT;
                break;
            case KEYPAD_INC_EXPOSURE:
                result = U8X8_MSG_GPIO_MENU_UP;
                break;
            case KEYPAD_DEC_EXPOSURE:
                result = U8X8_MSG_GPIO_MENU_DOWN;
                break;
            case KEYPAD_ENCODER_CW:
                result = ((uint16_t)event.count << 8) | U8X8_MSG_GPIO_MENU_VALUE_INC;
                break;
            case KEYPAD_ENCODER_CCW:
                result = ((uint16_t)event.count << 8) | U8X8_MSG_GPIO_MENU_VALUE_DEC;
                break;
            case KEYPAD_METER_PROBE:
                result = U8X8_MSG_GPIO_MENU_PROBE_BTN;
                break;
            case KEYPAD_DENSISTICK:
                result = U8X8_MSG_GPIO_MENU_STICK_BTN;
                break;
            default:
                break;
            }
        } else {
            /*
             * Button actions that leave the menu, such as accept and cancel
             * are handled on the release event. This is to prevent side
             * effects that can occur from other components receiving
             * release events for these keys.
             */
            if (((params & DISPLAY_MENU_ACCEPT_MENU) != 0 && event.key == KEYPAD_MENU)
                || ((params & DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT) != 0 && event.key == KEYPAD_ADD_ADJUSTMENT)
                || ((params & DISPLAY_MENU_ACCEPT_TEST_STRIP) != 0 && event.key == KEYPAD_TEST_STRIP)
                || ((params & DISPLAY_MENU_ACCEPT_ENCODER) != 0 && event.key == KEYPAD_ENCODER)) {
                result = ((uint16_t)event.key << 8) | U8X8_MSG_GPIO_MENU_SELECT;
            } else if (event.key == KEYPAD_CANCEL) {
                result = U8X8_MSG_GPIO_MENU_HOME;
            }
        }

        /*
         * Check flags that allow the meter probe or DensiStick button to
         * act as an accept button. These are implemented on the press, not
         * release, because they're an active action trigger.
         */
        if (params & DISPLAY_MENU_ACCEPT_PROBE && result == U8X8_MSG_GPIO_MENU_PROBE_BTN) {
            result = ((uint16_t)event.key << 8) | U8X8_MSG_GPIO_MENU_SELECT;
        } else if (params & DISPLAY_MENU_ACCEPT_STICK && result == U8X8_MSG_GPIO_MENU_STICK_BTN) {
            result = ((uint16_t)event.key << 8) | U8X8_MSG_GPIO_MENU_SELECT;
        }

        /*
         * Some USB keys have mappings that don't make sense in the context
         * of the above logic, or that can't easily be done generically.
         */
        if (result == 0 && event.key == KEYPAD_USB_KEYBOARD && event.pressed) {
            uint8_t keycode = keypad_usb_get_keycode(&event);
            char keychar = keypad_usb_get_ascii(&event);

            if ((params & DISPLAY_MENU_ACCEPT_MENU) != 0 && keychar == '\n') {
                result = ((uint16_t)KEYPAD_MENU << 8) | U8X8_MSG_GPIO_MENU_SELECT;
            } else if ((params & DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT) != 0 && keychar == '+') {
                result = ((uint16_t)KEYPAD_ADD_ADJUSTMENT << 8) | U8X8_MSG_GPIO_MENU_SELECT;
            } else if ((params & DISPLAY_MENU_ACCEPT_TEST_STRIP) != 0 && keychar == '*') {
                result = ((uint16_t)KEYPAD_TEST_STRIP << 8) | U8X8_MSG_GPIO_MENU_SELECT;
            } else if ((params & DISPLAY_MENU_ACCEPT_ENCODER) != 0 && keychar == '\t') {
                result = ((uint16_t)KEYPAD_ENCODER << 8) | U8X8_MSG_GPIO_MENU_SELECT;
            } else if (keycode == 0x29 /* KEY_ESCAPE */) {
                result = U8X8_MSG_GPIO_MENU_HOME;
            } else if ((params & DISPLAY_MENU_INPUT_ASCII) != 0) {
                if ((keychar >= 32 && keychar < 127) || keychar == '\n' || keychar == '\t') {
                    /* Handle normally printable characters that are correctly mapped */
                    result = ((uint16_t)keychar << 8) | U8X8_MSG_GPIO_MENU_INPUT_ASCII;
                } else if (keycode == 0x2A /* KEY_BACKSPACE */ || keycode == 0xBB /* KEY_KEYPAD_BACKSPACE */) {
                    result = ((uint16_t)'\b' << 8) | U8X8_MSG_GPIO_MENU_INPUT_ASCII;
                } else if (keycode == 0x4C /* KEY_DELETE */) {
                    result = ((uint16_t)'\x7F' << 8) | U8X8_MSG_GPIO_MENU_INPUT_ASCII;
                }
            }
        }
    } else if (ret == HAL_TIMEOUT) {
        if (params & DISPLAY_MENU_INPUT_POLL) {
            result = 0;
        } else {
            result = UINT16_MAX;
        }
    }
    return result;
}

void display_UserInterfaceStaticList(u8g2_t *u8g2, const char *title, const char *list)
{
    display_UserInterfaceStaticListDraw(u8g2, title, list, u8g2_GetDisplayWidth(u8g2));
}

void display_UserInterfaceStaticListDraw(u8g2_t *u8g2, const char *title, const char *list, u8g2_uint_t list_width)
{
    // Based off u8g2_UserInterfaceSelectionList() with changes to use
    // full frame buffer mode and to remove actual menu functionality.

    u8g2_ClearBuffer(u8g2);

    u8sl_t u8sl;
    u8g2_uint_t yy;

    u8g2_uint_t line_height = u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2) + MY_BORDER_SIZE;

    uint8_t title_lines = u8x8_GetStringLineCnt(title);
    uint8_t display_lines;

    if (title_lines > 0) {
        display_lines = (u8g2_GetDisplayHeight(u8g2) - 3) / line_height;
        u8sl.visible = display_lines;
        u8sl.visible -= title_lines;
    }
    else {
        display_lines = u8g2_GetDisplayHeight(u8g2) / line_height;
        u8sl.visible = display_lines;
    }

    u8sl.total = u8x8_GetStringLineCnt(list);
    u8sl.first_pos = 0;
    u8sl.current_pos = -1;

    u8g2_SetFontPosBaseline(u8g2);

    yy = u8g2_GetAscent(u8g2);
    if (title_lines > 0) {
        yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);
        u8g2_DrawHLine(u8g2, 0, yy - line_height - u8g2_GetDescent(u8g2) + 1, u8g2_GetDisplayWidth(u8g2));
        yy += 3;
    }
    display_DrawSelectionList(u8g2, &u8sl, yy, list, list_width);

    u8g2_SendBuffer(u8g2);
}

void display_DrawSelectionList(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, const char *s, u8g2_uint_t list_width)
{
    uint8_t i;
    for (i = 0; i < u8sl->visible; i++) {
        y += display_draw_selection_list_line(u8g2, u8sl, y, i+u8sl->first_pos, s, list_width);
    }
}

u8g2_uint_t display_draw_selection_list_line(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, uint8_t idx, const char *s, u8g2_uint_t list_width)
{
    u8g2_uint_t yy;
    uint8_t border_size = 0;
    uint8_t is_invert = 0;

    u8g2_uint_t line_height = u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2)+MY_BORDER_SIZE;

    /* calculate offset from display upper border */
    yy = idx;
    yy -= u8sl->first_pos;
    yy *= line_height;
    yy += y;

    /* check whether this is the current cursor line */
    if (idx == u8sl->current_pos) {
        border_size = MY_BORDER_SIZE;
        is_invert = 1;
    }

    /* get the line from the array */
    s = u8x8_GetStringLineStart(idx, s);

    /* draw the line */
    if (s == NULL) {
        s = "";
    }
    u8g2_DrawUTF8Line(u8g2, MY_BORDER_SIZE, y, list_width-2*MY_BORDER_SIZE, s, border_size, is_invert);
    return line_height;
}

/* v = value, d = number of digits */
const char *display_u16toa(uint16_t v, uint8_t d)
{
    // Based off u8x8_u16toa with changes to use whitespace padding
    static char buf[6];
    if (d > 5) {
        d = 5;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(buf, sizeof(buf), "%*d", d, v);
#pragma GCC diagnostic pop
    return buf;
}

static uint8_t uint_pow(uint8_t base, uint8_t exp)
{
    uint8_t result = 1;

    if (exp > 0) {
        for(uint8_t i = 0; i < exp; i++) {
            result *= base;
        }
    }

    return result;
}

/**
 * Internal state for the display_input_value support functions.
 */
typedef struct {
    uint8_t line_height;
    uint8_t title_lines;
    uint8_t body_lines;
    u8g2_uint_t pixel_height;
    u8g2_uint_t y;
    u8g2_uint_t pixel_width;
    u8g2_uint_t x;
    const char *title;
    const char *msg;
    const char *prefix;
    const char *postfix;
} display_input_value_state_t;

static void display_input_value_setup(display_input_value_state_t *state,
    u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint8_t digits, const char *postfix)
{
    memset(state, 0, sizeof(display_input_value_state_t));
    state->title = title;
    state->msg = msg;
    state->prefix = prefix;
    state->postfix = postfix;

    /* Only horizontal strings are supported, so force this here */
    u8g2_SetFontDirection(u8g2, 0);

    /* Force baseline position */
    u8g2_SetFontPosBaseline(u8g2);

    /* Calculate line height */
    state->line_height = u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2) + MY_BORDER_SIZE;
    state->title_lines = u8x8_GetStringLineCnt(title);
    state->body_lines = u8x8_GetStringLineCnt(msg) + 1;

    /* Calculate the height in pixel */
    state->pixel_height = state->body_lines * state->line_height;

    /* Calculate offset from top */
    state->y = 0;
    if (state->pixel_height < u8g2_GetDisplayHeight(u8g2)) {
        state->y = u8g2_GetDisplayHeight(u8g2);
        state->y -= state->pixel_height;
        state->y /= 2;
    }

    /* Calculate offset from left for the label */
    state->x = 0;
    state->pixel_width = u8g2_GetUTF8Width(u8g2, prefix);
    state->pixel_width += u8g2_GetUTF8Width(u8g2, "0") * digits;
    state->pixel_width += u8g2_GetUTF8Width(u8g2, postfix);
    if (state->pixel_width < u8g2_GetDisplayWidth(u8g2)) {
        state->x = u8g2_GetDisplayWidth(u8g2);
        state->x -= state->pixel_width;
        state->x /= 2;
    }
}

static void display_input_value_render(display_input_value_state_t *state, u8g2_t *u8g2, const char *value_str)
{
    u8g2_uint_t xx;
    u8g2_uint_t yy;

    u8g2_ClearBuffer(u8g2);
    yy = u8g2_GetAscent(u8g2);
    if (state->title_lines > 0) {
        yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), state->line_height, state->title);
        u8g2_DrawHLine(u8g2, 0, yy - state->line_height - u8g2_GetDescent(u8g2) + 1, u8g2_GetDisplayWidth(u8g2));
        yy += 3;
    }
    yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), state->line_height, state->msg);
    xx = state->x;
    xx += u8g2_DrawUTF8(u8g2, xx, yy, state->prefix);
    xx += u8g2_DrawUTF8(u8g2, xx, yy, value_str);
    u8g2_DrawUTF8(u8g2, xx, yy, state->postfix);
    u8g2_SendBuffer(u8g2);
}

uint8_t display_UserInterfaceInputValue(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix)
{
    // Based off u8g2_UserInterfaceInputValue() with changes to
    // the title style.

    display_input_value_state_t state;
    uint8_t local_value = *value;
    uint8_t event;
    uint8_t count;

    display_input_value_setup(&state, u8g2, title, msg, prefix, digits, postfix);

    /* Event loop */
    for (;;) {
        display_input_value_render(&state, u8g2, u8x8_u8toa(local_value, digits));

        for (;;) {
            uint16_t event_result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2), DISPLAY_MENU_ACCEPT_MENU);
            if (event_result == UINT16_MAX) {
                menu_event_timeout = true;
                event = U8X8_MSG_GPIO_MENU_HOME;
                count = 0;
            } else {
                event = (uint8_t)(event_result & 0x00FF);
                count = (uint8_t)((event_result & 0xFF00) >> 8);
            }

            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            } else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event == U8X8_MSG_GPIO_MENU_NEXT || event == U8X8_MSG_GPIO_MENU_UP || event == U8X8_MSG_GPIO_MENU_VALUE_INC) {
                int8_t amount = (event == U8X8_MSG_GPIO_MENU_VALUE_INC) ? count : 1;
                local_value = value_adjust_with_rollover_u8(local_value, amount, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_PREV || event == U8X8_MSG_GPIO_MENU_DOWN || event == U8X8_MSG_GPIO_MENU_VALUE_DEC) {
                int8_t amount = -1 * ((event == U8X8_MSG_GPIO_MENU_VALUE_DEC) ? count : 1);
                local_value = value_adjust_with_rollover_u8(local_value, amount, low, high);
                break;
            }
        }
    }
}

uint8_t display_UserInterfaceInputValueU16(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, uint8_t digits, const char *postfix)
{
    // Based off u8g2_UserInterfaceInputValue() with changes to
    // support 16-bit numbers.

    display_input_value_state_t state;
    uint16_t local_value = *value;
    uint8_t event;
    uint8_t count;

    display_input_value_setup(&state, u8g2, title, msg, prefix, digits, postfix);

    /* Event loop */
    for(;;) {
        display_input_value_render(&state, u8g2, display_u16toa(local_value, digits));

        for(;;) {
            uint16_t event_result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2), DISPLAY_MENU_ACCEPT_MENU);
            if (event_result == UINT16_MAX) {
                menu_event_timeout = true;
                event = U8X8_MSG_GPIO_MENU_HOME;
                count = 0;
            } else {
                event = (uint8_t)(event_result & 0x00FF);
                count = (uint8_t)((event_result & 0xFF00) >> 8);
            }

            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            } else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event == U8X8_MSG_GPIO_MENU_UP || event == U8X8_MSG_GPIO_MENU_VALUE_INC) {
                int8_t amount = (event == U8X8_MSG_GPIO_MENU_VALUE_INC) ? count : 1;
                local_value = value_adjust_with_rollover_u16(local_value, amount, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_NEXT) {
                local_value = value_adjust_with_rollover_u16(local_value, 10, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_DOWN || event == U8X8_MSG_GPIO_MENU_VALUE_DEC) {
                int8_t amount = -1 * ((event == U8X8_MSG_GPIO_MENU_VALUE_DEC) ? count : 1);
                local_value = value_adjust_with_rollover_u16(local_value, amount, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_PREV) {
                local_value = value_adjust_with_rollover_u16(local_value, -10, low, high);
                break;
            }
        }
    }
}

static const char *display_f1_2toa(uint16_t v, char sep)
{
    static char buf[5];

    buf[0] = '0' + (v % 1000 / 100);
    buf[1] = sep;
    buf[2] = '0' + (v % 100 / 10);
    buf[3] = '0' + (v % 10);
    buf[4] = '\0';

    return buf;
}

uint8_t display_UserInterfaceInputValueF1_2(u8g2_t *u8g2, const char *title, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, char sep, const char *postfix)
{
    /*
     * Based off u8g2_UserInterfaceInputValue() with changes to use
     * full frame buffer mode and to support the N.DD number format.
     */

    uint8_t line_height;
    uint8_t height;
    u8g2_uint_t pixel_height;
    u8g2_uint_t y, yy;
    u8g2_uint_t pixel_width;
    u8g2_uint_t x, xx;

    uint16_t local_value = *value;
    uint8_t event;
    uint8_t count;

    /* Explicitly constrain input values */
    if (high > 999) { high = 999; }
    if (low > 0 && low > high) { low = high; }
    if (local_value < low) { local_value = low; }
    else if (local_value > high) { local_value = high; }

    /* Only horizontal strings are supported, so force this here */
    u8g2_SetFontDirection(u8g2, 0);

    /* Force baseline position */
    u8g2_SetFontPosBaseline(u8g2);

    /* Calculate line height */
    line_height = u8g2_GetAscent(u8g2);
    line_height -= u8g2_GetDescent(u8g2);

    /* Calculate overall height of the input value box */
    height = 1; /* value input line */
    height += u8x8_GetStringLineCnt(title);

    /* Calculate the height in pixels */
    pixel_height = height;
    pixel_height *= line_height;

    /* Calculate offset from top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(u8g2)) {
        y = u8g2_GetDisplayHeight(u8g2);
        y -= pixel_height;
        y /= 2;
    }

    /* Calculate offset from left for the label */
    x = 0;
    pixel_width = u8g2_GetUTF8Width(u8g2, prefix);
    pixel_width += u8g2_GetUTF8Width(u8g2, "0") * 4;
    pixel_width += u8g2_GetUTF8Width(u8g2, postfix);
    if (pixel_width < u8g2_GetDisplayWidth(u8g2)) {
        x = u8g2_GetDisplayWidth(u8g2);
        x -= pixel_width;
        x /= 2;
    }

    /* Event loop */
    for(;;) {
        /* Render */
        u8g2_ClearBuffer(u8g2);
        yy = y;
        yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);
        xx = x;
        xx += u8g2_DrawUTF8(u8g2, xx, yy, prefix);
        xx += u8g2_DrawUTF8(u8g2, xx, yy, display_f1_2toa(local_value, sep));
        u8g2_DrawUTF8(u8g2, xx, yy, postfix);
        u8g2_SendBuffer(u8g2);

        for(;;) {
            uint16_t event_result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2), DISPLAY_MENU_ACCEPT_MENU);
            if (event_result == UINT16_MAX) {
                menu_event_timeout = true;
                event = U8X8_MSG_GPIO_MENU_HOME;
                count = 0;
            } else {
                event = (uint8_t)(event_result & 0x00FF);
                count = (uint8_t)((event_result & 0xFF00) >> 8);
            }

            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            } else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event == U8X8_MSG_GPIO_MENU_UP || event == U8X8_MSG_GPIO_MENU_VALUE_INC) {
                int8_t amount = (event == U8X8_MSG_GPIO_MENU_VALUE_INC) ? count : 1;
                local_value = value_adjust_with_rollover_u16(local_value, amount, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_NEXT) {
                local_value = value_adjust_with_rollover_u16(local_value, 10, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_DOWN || event == U8X8_MSG_GPIO_MENU_VALUE_DEC) {
                int8_t amount = -1 * ((event == U8X8_MSG_GPIO_MENU_VALUE_DEC) ? count : 1);
                local_value = value_adjust_with_rollover_u16(local_value, amount, low, high);
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_PREV) {
                local_value = value_adjust_with_rollover_u16(local_value, -10, low, high);
                break;
            }
        }
    }
}

static const char *display_f16toa(uint16_t val, uint8_t wdigits, uint8_t fdigits)
{
    static char buf[7];

    uint8_t fpow = uint_pow(10, fdigits);
    uint16_t wval = val / fpow;
    uint16_t fval = val % fpow;

    if (wdigits + fdigits > 5) {
        wdigits = 3;
        fdigits = 2;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(buf, sizeof(buf), "%*d.%0*d", wdigits, wval, fdigits, fval);
#pragma GCC diagnostic pop
    return buf;
}

uint8_t display_UserInterfaceInputValueF16(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix,
    display_GetMenuEvent_t event_callback, display_menu_params_t params,
    display_data_source_callback_t data_callback, void *user_data)
{
    // Based off u8g2_UserInterfaceInputValue() with changes to
    // support 16-bit numbers displayed in a fixed point format

    display_input_value_state_t state;
    uint16_t local_value = *value;

    if (wdigits + fdigits > 5) {
        return 0;
    }

    display_input_value_setup(&state, u8g2, title, msg, prefix, wdigits + fdigits + 1, postfix);

    /* Event loop */
    for(;;) {
        display_input_value_render(&state, u8g2, display_f16toa(local_value, wdigits, fdigits));

        for(;;) {
            uint8_t event_action;
            uint8_t count;
            if (event_callback) {
                uint16_t result = event_callback(u8g2_GetU8x8(u8g2), params);
                if (result == UINT16_MAX) {
                    return result;
                }
                event_action = (uint8_t)(result & 0x00FF);
                count = (uint8_t)((result & 0xFF00) >> 8);
            } else {
                uint16_t result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2), DISPLAY_MENU_ACCEPT_MENU);
                if (result == UINT16_MAX) {
                    menu_event_timeout = true;
                    event_action = U8X8_MSG_GPIO_MENU_HOME;
                    count = 0;
                } else {
                    event_action = (uint8_t)(result & 0x00FF);
                    count = (uint8_t)((result & 0xFF00) >> 8);
                }
            }

            if (event_action == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            } else if (event_action == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event_action == U8X8_MSG_GPIO_MENU_UP || event_action == U8X8_MSG_GPIO_MENU_VALUE_INC) {
                int8_t amount = (event_action == U8X8_MSG_GPIO_MENU_VALUE_INC) ? count : 1;
                local_value = value_adjust_with_rollover_u16(local_value, amount, low, high);
                break;
            } else if (event_action == U8X8_MSG_GPIO_MENU_NEXT) {
                local_value = value_adjust_with_rollover_u16(local_value, 10, low, high);
                break;
            } else if (event_action == U8X8_MSG_GPIO_MENU_DOWN || event_action == U8X8_MSG_GPIO_MENU_VALUE_DEC) {
                int8_t amount = -1 * ((event_action == U8X8_MSG_GPIO_MENU_VALUE_DEC) ? count : 1);
                local_value = value_adjust_with_rollover_u16(local_value, amount, low, high);
                break;
            } else if (event_action == U8X8_MSG_GPIO_MENU_PREV) {
                local_value = value_adjust_with_rollover_u16(local_value, -10, low, high);
                break;
            } else {
                if (data_callback) {
                    uint16_t input_value = data_callback(event_action, user_data);
                    if (input_value >= low && input_value <= high) {
                        local_value = input_value;
                        break;
                    }
                }
            }
        }
    }
}

uint8_t display_UserInterfaceInputValueCB(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
    display_input_value_callback_t callback, void *user_data)
{
    // Based off u8g2_UserInterfaceInputValue() with changes to
    // invoke a callback on value change.

    display_input_value_state_t state;
    uint8_t local_value = *value;
    uint8_t event;
    uint8_t count;

    display_input_value_setup(&state, u8g2, title, msg, prefix, digits, postfix);

    /* Event loop */
    for(;;) {
        display_input_value_render(&state, u8g2, u8x8_u8toa(local_value, digits));

        for(;;) {
            uint16_t event_result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2), DISPLAY_MENU_ACCEPT_MENU);
            if (event_result == UINT16_MAX) {
                menu_event_timeout = true;
                event = U8X8_MSG_GPIO_MENU_HOME;
                count = 0;
            } else {
                event = (uint8_t)(event_result & 0x00FF);
                count = (uint8_t)((event_result & 0xFF00) >> 8);
            }

            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            } else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event == U8X8_MSG_GPIO_MENU_NEXT || event == U8X8_MSG_GPIO_MENU_UP || event == U8X8_MSG_GPIO_MENU_VALUE_INC) {
                int8_t amount = (event == U8X8_MSG_GPIO_MENU_VALUE_INC) ? count : 1;
                local_value = value_adjust_with_rollover_u8(local_value, amount, low, high);
                if (callback) {
                    osMutexRelease(display_mutex);
                    callback(local_value, user_data);
                    osMutexAcquire(display_mutex, portMAX_DELAY);
                }
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_PREV || event == U8X8_MSG_GPIO_MENU_DOWN || event == U8X8_MSG_GPIO_MENU_VALUE_DEC) {
                int8_t amount = -1 * ((event == U8X8_MSG_GPIO_MENU_VALUE_DEC) ? count : 1);
                local_value = value_adjust_with_rollover_u8(local_value, amount, low, high);
                if (callback) {
                    osMutexRelease(display_mutex);
                    callback(local_value, user_data);
                    osMutexAcquire(display_mutex, portMAX_DELAY);
                }
                break;
            }
        }
    }
}

uint16_t display_UserInterfaceSelectionListCB(u8g2_t *u8g2, const char *title, uint8_t start_pos, const char *sl,
    display_GetMenuEvent_t event_callback, display_menu_params_t params)
{
    // Based off u8g2_UserInterfaceSelectionList() with changes to
    // support parameters for key event handling that allow for
    // multiple accept keys.

    u8sl_t u8sl;
    u8g2_uint_t yy;

    u8g2_uint_t line_height = u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2) + MY_BORDER_SIZE;

    uint8_t title_lines = u8x8_GetStringLineCnt(title);
    uint8_t display_lines;


    if (start_pos > 0) {
        start_pos--;
    }

    if (title_lines > 0) {
        display_lines = (u8g2_GetDisplayHeight(u8g2) - 3) / line_height;
        u8sl.visible = display_lines;
        u8sl.visible -= title_lines;
    } else {
        display_lines = u8g2_GetDisplayHeight(u8g2) / line_height;
        u8sl.visible = display_lines;
    }

    u8sl.total = u8x8_GetStringLineCnt(sl);
    u8sl.first_pos = 0;
    u8sl.current_pos = start_pos;

    if (u8sl.current_pos >= u8sl.total) {
        u8sl.current_pos = u8sl.total - 1;
    }
    if (u8sl.first_pos + u8sl.visible <= u8sl.current_pos) {
        u8sl.first_pos = u8sl.current_pos-u8sl.visible + 1;
    }

    u8g2_SetFontPosBaseline(u8g2);

    for(;;) {
        u8g2_ClearBuffer(u8g2);
        yy = u8g2_GetAscent(u8g2);
        if (title_lines > 0) {
            yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);
            u8g2_DrawHLine(u8g2, 0, yy - line_height - u8g2_GetDescent(u8g2) + 1, u8g2_GetDisplayWidth(u8g2));
            yy += 3;
        }
        u8g2_DrawSelectionList(u8g2, &u8sl, yy, sl);
        u8g2_SendBuffer(u8g2);

        for(;;) {
            uint8_t event_action;
            uint8_t event_keycode;

            if (event_callback) {
                uint16_t result = event_callback(u8g2_GetU8x8(u8g2), params);
                if (result == UINT16_MAX) {
                    return result;
                }
                event_action = (uint8_t)(result & 0x00FF);
                event_keycode = (uint8_t)((result & 0xFF00) >> 8);
            } else {
                event_action = u8x8_GetMenuEvent(u8g2_GetU8x8(u8g2));
                event_keycode = 0;
            }

            if (event_action == U8X8_MSG_GPIO_MENU_SELECT) {
                return ((uint16_t)event_keycode << 8) | (u8sl.current_pos + 1);
            }
            else if (event_action == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            }
            else if (event_action == U8X8_MSG_GPIO_MENU_NEXT || event_action == U8X8_MSG_GPIO_MENU_DOWN) {
                u8sl_Next(&u8sl);
                break;
            }
            else if (event_action == U8X8_MSG_GPIO_MENU_PREV || event_action == U8X8_MSG_GPIO_MENU_UP) {
                u8sl_Prev(&u8sl);
                break;
            }
        }
    }
}

uint8_t display_UserInterfaceMessageCB(u8g2_t *u8g2, const char *title1, const char *title2, const char *title3,
    const char *buttons,
    display_GetMenuEvent_t event_callback, display_menu_params_t params)
{
    /*
     * Based off u8g2_UserInterfaceMessage() with changes to
     * support parameters for key event handling that allow for
     * multiple accept keys.
     */
    uint8_t height;
    uint8_t line_height;
    u8g2_uint_t pixel_height;
    u8g2_uint_t y, yy;

    uint8_t cursor = 0;
    uint8_t button_cnt;

    /* only horizontal strings are supported, so force this here */
    u8g2_SetFontDirection(u8g2, 0);

    /* force baseline position */
    u8g2_SetFontPosBaseline(u8g2);


    /* calculate line height */
    line_height = u8g2_GetAscent(u8g2);
    line_height -= u8g2_GetDescent(u8g2);

    /* calculate overall height of the message box in lines*/
    height = 1; /* button line */
    height += u8x8_GetStringLineCnt(title1);
    if (title2 != NULL) {
        height++;
    }
    height += u8x8_GetStringLineCnt(title3);

    /* calculate the height in pixel */
    pixel_height = height;
    pixel_height *= line_height;

    /* ... and add the space between the text and the buttons */
    pixel_height += MY_SPACE_BETWEEN_TEXT_AND_BUTTONS_IN_PIXEL;

    /* calculate offset from top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(u8g2)) {
        y = u8g2_GetDisplayHeight(u8g2);
        y -= pixel_height;
        y /= 2;
    }
    y += u8g2_GetAscent(u8g2);


    for (;;) {
        u8g2_ClearBuffer(u8g2);
        yy = y;
        /* draw message box */

        yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title1);
        if (title2 != NULL) {
            u8g2_DrawUTF8Line(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), title2, 0, 0);
            yy += line_height;
        }
        yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title3);
        yy += MY_SPACE_BETWEEN_TEXT_AND_BUTTONS_IN_PIXEL;

        button_cnt = u8g2_draw_button_line(u8g2, yy, u8g2_GetDisplayWidth(u8g2), cursor, buttons);
        u8g2_SendBuffer(u8g2);

        for (;;) {
            uint8_t event_action;

            if (event_callback) {
                uint16_t result = event_callback(u8g2_GetU8x8(u8g2), params);
                if (result == UINT16_MAX) {
                    return result;
                }
                event_action = (uint8_t)(result & 0x00FF);
            } else {
                event_action = u8x8_GetMenuEvent(u8g2_GetU8x8(u8g2));
            }

            if (event_action == U8X8_MSG_GPIO_MENU_SELECT) {
                return cursor + 1;
            } else if (event_action == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event_action == U8X8_MSG_GPIO_MENU_NEXT || event_action == U8X8_MSG_GPIO_MENU_DOWN) {
                cursor++;
                if (cursor >= button_cnt) {
                    cursor = 0;
                }
                break;
            } else if (event_action == U8X8_MSG_GPIO_MENU_PREV || event_action == U8X8_MSG_GPIO_MENU_UP) {
                if (cursor == 0) {
                    cursor = button_cnt;
                }
                cursor--;
                break;
            }
        }
    }
    /* never reached */
    //return 0;
}

uint8_t display_DrawButtonLine(u8g2_t *u8g2, u8g2_uint_t y, u8g2_uint_t w, uint8_t cursor, const char *s)
{
    return u8g2_draw_button_line(u8g2, y, w, cursor, s);
}
