#ifndef DWM_POPUP_H
#define DWM_POPUP_H 1

#include "window.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define POPUP_HEIGHT 125
#define POPUP_WIDTH 325
#define POPUP_BUTTON_AREA_HEIGHT 50
#define POPUP_BUTTON_HEIGHT 32
#define POPUP_BUTTON_WIDTH 100

typedef enum
{
    POPUP_OK,
    POPUP_RETRY_CANCEL,
    POPUP_YES_NO
} popup_type_t;

typedef enum
{
    POPUP_RES_OK,
    POPUP_RES_RETRY,
    POPUP_RES_CANCEL,
    POPUP_RES_YES,
    POPUP_RES_NO,
    POPUP_RES_CLOSE,
    POPUP_RES_ERROR
} popup_result_t;

popup_result_t popup_open(const char* text, const char* title, popup_type_t type);

#if defined(__cplusplus)
}
#endif

#endif
