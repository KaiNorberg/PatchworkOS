#include "taskbar.h"

#include <libpatchwork/display.h>
#include <libpatchwork/element.h>
#include <libpatchwork/event.h>
#include <libpatchwork/patchwork.h>
#include <libpatchwork/window.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <time.h>

static rect_t taskbar_get_start_rect(element_t* elem)
{
    const theme_t* theme = element_get_theme(elem);
    return RECT_INIT_DIM(theme->bigPadding, theme->frameSize + theme->smallPadding, START_WIDTH,
        theme->panelSize - theme->frameSize - theme->smallPadding * 2);
}

static rect_t taskbar_get_clock_rect(element_t* elem)
{
    const theme_t* theme = element_get_theme(elem);
    rect_t rect = element_get_content_rect(elem);

    return RECT_INIT_DIM(RECT_WIDTH(&rect) - CLOCK_WIDTH - theme->bigPadding, theme->frameSize + theme->smallPadding,
        CLOCK_WIDTH, theme->panelSize - theme->frameSize - theme->smallPadding * 2);
}

static rect_t taskbar_get_left_separator_rect(element_t* elem)
{
    rect_t startRect = taskbar_get_start_rect(elem);
    const theme_t* theme = element_get_theme(elem);

    return RECT_INIT_DIM(startRect.right + theme->bigPadding, startRect.top, theme->separatorSize,
        RECT_HEIGHT(&startRect));
}

static rect_t taskbar_get_right_separator_rect(element_t* elem)
{
    rect_t clockRect = taskbar_get_clock_rect(elem);
    const theme_t* theme = element_get_theme(elem);

    return RECT_INIT_DIM(clockRect.left - theme->bigPadding - theme->separatorSize, clockRect.top, theme->separatorSize,
        RECT_HEIGHT(&clockRect));
}

static rect_t taskbar_get_task_button_rect(taskbar_t* taskbar, element_t* elem, uint64_t index)
{
    const theme_t* theme = element_get_theme(elem);

    rect_t leftSeparator = taskbar_get_left_separator_rect(elem);
    rect_t rightSeparator = taskbar_get_right_separator_rect(elem);

    uint64_t firstAvailPos = leftSeparator.right + theme->bigPadding;
    uint64_t lastAvailPos = rightSeparator.left - theme->bigPadding;
    uint64_t availLength = lastAvailPos - firstAvailPos;

    uint64_t entryCount = list_length(&taskbar->entries);

    if (entryCount == 0)
    {
        return RECT_INIT_DIM(firstAvailPos, leftSeparator.top, 0, RECT_HEIGHT(&leftSeparator));
    }

    uint64_t totalPadding = (entryCount - 1) * theme->bigPadding;
    uint64_t buttonWidth = MIN(TASK_BUTTON_MAX_WIDTH, (availLength - totalPadding) / entryCount);

    return RECT_INIT_DIM(firstAvailPos + (buttonWidth + theme->bigPadding) * index, leftSeparator.top, buttonWidth,
        RECT_HEIGHT(&leftSeparator));
}

static void taskbar_reposition_task_buttons(taskbar_t* taskbar, element_t* elem)
{
    uint64_t index = 0;
    taskbar_entry_t* entry;
    LIST_FOR_EACH(entry, &taskbar->entries, entry)
    {
        rect_t rect = taskbar_get_task_button_rect(taskbar, elem, index);
        element_move(entry->button, &rect);
        index++;
    }
}

static void taskbar_entry_add(taskbar_t* taskbar, element_t* elem, const surface_info_t* info, const char* name)
{
    taskbar_entry_t* entry = malloc(sizeof(taskbar_entry_t));
    if (entry == NULL)
    {
        return; // If this fails there isent much we can do so we just ignore it.
    }
    list_entry_init(&entry->entry);
    entry->info = *info;
    strcpy(entry->name, name);

    list_push(&taskbar->entries, &entry->entry);

    element_redraw(elem, true);

    rect_t rect = taskbar_get_task_button_rect(taskbar, elem, list_length(&taskbar->entries) - 1);
    entry->button = button_new(elem, info->id, &rect, entry->name, ELEMENT_TOGGLE);
    if (entry->button == NULL)
    {
        list_remove(&taskbar->entries, &entry->entry);
        free(entry);
        return; // Same here
    }

    taskbar_reposition_task_buttons(taskbar, elem);
}

static void taskbar_entry_remove(taskbar_t* taskbar, element_t* elem, surface_id_t surface)
{
    element_redraw(elem, true);

    taskbar_entry_t* entry;
    LIST_FOR_EACH(entry, &taskbar->entries, entry)
    {
        if (entry->info.id == surface)
        {
            element_free(entry->button);
            list_remove(&taskbar->entries, &entry->entry);
            free(entry);

            taskbar_reposition_task_buttons(taskbar, elem);
            return;
        }
    }
}

static uint64_t taskbar_update_clock(element_t* elem)
{
    time_t epoch = time(NULL);
    struct tm timeData;
    localtime_r(&epoch, &timeData);
    char buffer[MAX_PATH];
    sprintf(buffer, "%02d:%02d %d-%02d-%02d", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
        timeData.tm_mon + 1, timeData.tm_mday);
    element_t* clockLabel = element_find(elem, CLOCK_LABEL_ID);

    if (element_set_text(clockLabel, buffer) == ERR)
    {
        printf("taskbar: failed to update clock label\n");
        return ERR;
    }
    element_redraw(clockLabel, false);

    return 0;
}

static uint64_t taskbar_procedure(window_t* win, element_t* elem, const event_t* event)
{
    const theme_t* theme = element_get_theme(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect = element_get_content_rect(elem);

        rect_t startRect = taskbar_get_start_rect(elem);
        if (button_new(elem, START_ID, &startRect, "Start", ELEMENT_TOGGLE | ELEMENT_NO_OUTLINE) == NULL)
        {
            printf("taskbar: failed to create start button\n");
            return ERR;
        }

        rect_t clockRect = taskbar_get_clock_rect(elem);
        if (label_new(elem, CLOCK_LABEL_ID, &clockRect, "0", ELEMENT_NONE) == NULL)
        {
            printf("taskbar: failed to create clock label\n");
            return ERR;
        }

        window_set_timer(win, TIMER_REPEAT, CLOCKS_PER_SEC * 30);

        if (taskbar_update_clock(elem) == ERR)
        {
            printf("taskbar: failed to update clock\n");
            return ERR;
        }

        taskbar_t* taskbar = malloc(sizeof(taskbar_t));
        if (taskbar == NULL)
        {
            printf("taskbar: failed to allocate taskbar private data\n");
            errno = ENOMEM;
            return ERR;
        }
        taskbar->win = win;
        taskbar->disp = window_get_display(win);
        taskbar->startMenu = start_menu_new(taskbar->win, taskbar->disp);
        if (taskbar->startMenu == NULL)
        {
            free(taskbar);
            return ERR;
        }
        list_init(&taskbar->entries);

        element_set_private(elem, taskbar);
    }
    break;
    case LEVENT_DEINIT:
    {
        taskbar_t* taskbar = element_get_private(elem);
        if (taskbar == NULL)
        {
            break;
        }
        window_free(taskbar->startMenu);

        taskbar_entry_t* entry;
        taskbar_entry_t* temp;
        LIST_FOR_EACH_SAFE(entry, temp, &taskbar->entries, entry)
        {
            element_free(entry->button);
            list_remove(&taskbar->entries, &entry->entry);
            free(entry);
        }

        free(taskbar);
    }
    break;
    case EVENT_TIMER:
    {
        if (taskbar_update_clock(elem) == ERR)
        {
            return ERR;
        }
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        draw_rect(&draw, &rect, theme->deco.backgroundNormal);

        rect.bottom = rect.top + theme->frameSize;
        draw_rect(&draw, &rect, theme->deco.highlight);

        rect_t leftSeparator = taskbar_get_left_separator_rect(elem);
        rect_t rightSeparator = taskbar_get_right_separator_rect(elem);

        draw_separator(&draw, &leftSeparator, theme->deco.highlight, theme->deco.shadow, DIRECTION_VERTICAL);
        draw_separator(&draw, &rightSeparator, theme->deco.highlight, theme->deco.shadow, DIRECTION_VERTICAL);

        element_draw_end(elem, &draw);
    }
    break;
    case LEVENT_ACTION:
    {
        taskbar_t* taskbar = element_get_private(elem);

        if (event->lAction.source == START_ID)
        {
            if (event->lAction.type == ACTION_PRESS)
            {
                start_menu_open(taskbar->startMenu);
            }
            else if (event->lAction.type == ACTION_RELEASE)
            {
                start_menu_close(taskbar->startMenu);
            }
            break;
        }

        display_t* disp = window_get_display(win);

        if (event->lAction.type == ACTION_PRESS)
        {
            display_set_is_visible(disp, event->lAction.source, false);
        }
        else if (event->lAction.type == ACTION_RELEASE)
        {
            display_set_is_visible(disp, event->lAction.source, true);
        }
    }
    break;
    case UEVENT_START_MENU_CLOSE:
    {
        taskbar_t* taskbar = element_get_private(elem);

        levent_force_action_t event;
        event.action = ACTION_RELEASE;
        element_emit(elem, LEVENT_FORCE_ACTION, &event, sizeof(event));
    }
    break;
    case GEVENT_ATTACH:
    {
        if (event->globalAttach.info.type != SURFACE_WINDOW || strcmp(event->globalAttach.info.name, "StartMenu") == 0)
        {
            break;
        }

        taskbar_t* taskbar = element_get_private(elem);
        taskbar_entry_add(taskbar, elem, &event->globalAttach.info, event->globalAttach.info.name);
    }
    break;
    case GEVENT_DETACH:
    {
        taskbar_t* taskbar = element_get_private(elem);
        taskbar_entry_remove(taskbar, elem, event->globalDetach.info.id);
    }
    break;
    case GEVENT_REPORT:
    {
        taskbar_t* taskbar = element_get_private(elem);

        taskbar_entry_t* entry;
        LIST_FOR_EACH(entry, &taskbar->entries, entry)
        {
            if (event->globalReport.info.id != entry->info.id)
            {
                continue;
            }

            entry->info = event->globalReport.info;
            element_force_action(entry->button, (entry->info.flags & SURFACE_VISIBLE) ? ACTION_RELEASE : ACTION_PRESS);
            break;
        }
    }
    break;
    case GEVENT_KBD:
    {
        taskbar_t* taskbar = element_get_private(elem);

        if (event->globalKbd.type == KBD_RELEASE && event->globalKbd.code == KBD_LEFT_SUPER)
        {
            start_menu_state_t state = start_menu_get_state(taskbar->startMenu);
            if (state == START_MENU_OPEN || state == START_MENU_OPENING)
            {
                element_force_action(element_find(elem, START_ID), ACTION_RELEASE);
                start_menu_close(taskbar->startMenu);
            }
            else
            {
                element_force_action(element_find(elem, START_ID), ACTION_PRESS);
                start_menu_open(taskbar->startMenu);
            }
        }
    }
    break;
    }

    return 0;
}

window_t* taskbar_new(display_t* disp)
{
    fd_t klog = open("/dev/klog");
    if (klog == ERR)
    {
        printf("taskbar: failed to open klog\n");
        return NULL;
    }
    if (dup2(klog, STDOUT_FILENO) == ERR || dup2(klog, STDERR_FILENO) == ERR)
    {
        printf("taskbar: failed to redirect stdout/stderr to klog\n");
        close(klog);
        return NULL;
    }
    close(klog);

    rect_t rect = display_screen_rect(disp, 0);
    rect.top = rect.bottom - theme_global_get()->panelSize;

    if (display_subscribe(disp, GEVENT_ATTACH) == ERR || display_subscribe(disp, GEVENT_DETACH) == ERR ||
        display_subscribe(disp, GEVENT_REPORT) == ERR || display_subscribe(disp, GEVENT_KBD) == ERR)
    {
        printf("taskbar: failed to subscribe to global events\n");
        return NULL;
    }

    window_t* win = window_new(disp, "Taskbar", &rect, SURFACE_PANEL, WINDOW_NONE, taskbar_procedure, NULL);
    if (win == NULL)
    {
        printf("taskbar: failed to create taskbar window\n");
        return NULL;
    }

    if (window_set_visible(win, true) == ERR)
    {
        printf("taskbar: failed to show taskbar window\n");
        window_free(win);
        return NULL;
    }

    return win;
}
