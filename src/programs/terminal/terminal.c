#include "terminal.h"
#include "command.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/win.h>

// This is probobly one of the messiest parts of this project.

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH 750
#define TERMINAL_HEIGHT 500

static win_t* terminal;
static terminal_state_t state;

static pipefd_t stdin;
static pipefd_t stdout;

static point_t cursorPos;
static bool cursorVisible;

static char input[MAX_PATH];

#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * (font)->width) + winTheme.edgeWidth + winTheme.padding, \
        .y = ((cursorPos)->y * (font)->height) + winTheme.edgeWidth + winTheme.padding, \
    };

static void input_push(char chr)
{
    uint64_t strLen = strlen(input);
    if (strLen >= MAX_PATH - 2)
    {
        return;
    }
    input[strLen] = chr;
    input[strLen + 1] = '\0';
}

static char input_pop(void)
{
    uint64_t strLen = strlen(input);
    if (strLen != 0)
    {
        char result = input[strLen - 1];
        input[strLen - 1] = '\0';
        return result;
    }

    return '\0';
}

static void input_clear(void)
{
    input[0] = '\0';
}

static void terminal_cursor_draw(void)
{
    const gfx_psf_t* font = win_font(terminal);
    point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);
    if (cursorVisible)
    {
        gfx_char(&gfx, font, &point, font->height, ' ', winTheme.bright, winTheme.bright);
    }
    else
    {
        gfx_char(&gfx, font, &point, font->height, ' ', winTheme.dark, winTheme.dark);
    }
    win_draw_end(terminal, &gfx);
}

static void terminal_cursor_clear(void)
{
    bool temp = cursorVisible;
    cursorVisible = false;
    terminal_cursor_draw();
    cursorVisible = temp;
}

static void terminal_print_prompt(void)
{
    char cwd[MAX_PATH];
    realpath(cwd, ".");

    terminal_print("\n");
    terminal_print(cwd);
    terminal_print("\n> ");
}

static void terminal_handle_input(char chr)
{
    switch (chr)
    {
    case '\n':
    {
        terminal_put('\n');

        switch (state)
        {
        case TERMINAL_COMMAND:
        {
            command_parse(input);
            if (state == TERMINAL_COMMAND)
            {
                terminal_print_prompt();
            }
        }
        break;
        case TERMINAL_SPAWN:
        {
            write(stdin.write, input, strlen(input));
        }
        break;
        }

        input_clear();
    }
    break;
    case '\b':
    {
        if (input_pop() != '\0')
        {
            terminal_put('\b');
        }
    }
    break;
    default:
    {
        input_push(chr);
        terminal_put(chr);
    }
    break;
    }
}

static void terminal_state_set(terminal_state_t newState)
{
    state = newState;
    input_clear();
}

uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        terminal_state_set(TERMINAL_COMMAND);
        win_timer_set(window, BLINK_INTERVAL);

        chdir("home:/usr");
    }
    break;
    case LMSG_REDRAW:
    {
        terminal_clear();

        terminal_print("Welcome to the Terminal (Very WIP)\n");
        terminal_print("Type help for a list of commands\n");

        terminal_print_prompt();
    }
    break;
    case LMSG_TIMER:
    {
        win_timer_set(window, BLINK_INTERVAL);

        cursorVisible = !cursorVisible;
        terminal_cursor_draw();
    }
    break;
    case MSG_KBD:
    {
        msg_kbd_t* data = (msg_kbd_t*)msg->data;
        if (data->type != KBD_PRESS)
        {
            break;
        }

        char chr = kbd_ascii(data->code, data->mods);
        if (chr == '\0')
        {
            break;
        }

        terminal_handle_input(chr);

        cursorVisible = true;
        terminal_cursor_draw();
        win_timer_set(window, BLINK_INTERVAL);
    }
    break;
    }

    return 0;
}

void terminal_init(void)
{
    rect_t rect = RECT_INIT_DIM(500, 200, TERMINAL_WIDTH, TERMINAL_HEIGHT);
    win_expand_to_window(&rect, WIN_DECO);

    terminal = win_new("Terminal", &rect, DWM_WINDOW, WIN_DECO, procedure);
    if (terminal == NULL)
    {
        exit(errno);
    }
}

void terminal_deinit(void)
{
    win_free(terminal);
}

static void terminal_read_stdout(void)
{
    pollfd_t fd = {.fd = stdout.read, .requested = POLL_READ};

    do
    {
        char chr;
        if (read(stdout.read, &chr, 1) == 0)
        {
            close(stdout.read);
            close(stdin.write);
            terminal_print_prompt();
            terminal_state_set(TERMINAL_COMMAND);
            break;
        }

        terminal_put(chr);
        poll(&fd, 1, 0);
    } while (fd.occurred & POLL_READ);
}

void terminal_loop(void)
{
    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        switch (state)
        {
        case TERMINAL_COMMAND:
        {
            win_receive(terminal, &msg, NEVER);
            win_dispatch(terminal, &msg);
        }
        break;
        case TERMINAL_SPAWN:
        {
            pollfd_t fds[] = {{.fd = win_fd(terminal), .requested = POLL_READ}, {.fd = stdout.read, .requested = POLL_READ}};
            poll(fds, 2, BLINK_INTERVAL);

            if (win_receive(terminal, &msg, 0))
            {
                win_dispatch(terminal, &msg);
            }
            if (fds[1].occurred & POLL_READ)
            {
                terminal_read_stdout();
            }
        }
        break;
        }
    }
}

void terminal_clear(void)
{
    cursorVisible = false;
    cursorPos = (point_t){.x = 0, .y = 0};

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    rect_t rect = RECT_INIT_GFX(&gfx);
    gfx_edge(&gfx, &rect, winTheme.edgeWidth, winTheme.shadow, winTheme.highlight);
    RECT_SHRINK(&rect, winTheme.edgeWidth);
    gfx_rect(&gfx, &rect, winTheme.dark);

    win_draw_end(terminal, &gfx);
}

uint64_t terminal_spawn(const char** argv)
{
    pipe(&stdin);
    pipe(&stdout);

    spawn_fd_t fds[] = {{STDIN_FILENO, stdin.read}, {STDOUT_FILENO, stdout.write}, SPAWN_FD_END};
    pid_t pid = spawn(argv, fds);
    if (pid == ERR)
    {
        close(stdin.read);
        close(stdin.write);
        close(stdout.read);
        close(stdout.write);
        return ERR;
    }

    close(stdin.read);
    close(stdout.write);
    terminal_state_set(TERMINAL_SPAWN);
    return 0;
}

static void terminal_scroll(void)
{
    cursorPos.y -= 2;

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    rect_t rect = RECT_INIT_GFX(&gfx);
    RECT_SHRINK(&rect, winTheme.edgeWidth);

    gfx_scroll(&gfx, &rect, win_font(terminal)->height * 2, winTheme.dark);

    win_draw_end(terminal, &gfx);
}

static void terminal_put_backend(gfx_t* gfx, char chr)
{
    const gfx_psf_t* font = win_font(terminal);

    switch (chr)
    {
    case '\n':
    {
        terminal_cursor_clear();
        cursorPos.x = 0;
        cursorPos.y++;

        if ((cursorPos.y + 1) * font->height > gfx->height - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_scroll();
        }
    }
    break;
    case '\b':
    {
        if (cursorPos.x != 0)
        {
            terminal_cursor_clear();
            cursorPos.x--;
            terminal_cursor_clear();
        }
    }
    break;
    case '\0':
    {
    }
    break;
    default:
    {
        point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);
        gfx_char(gfx, font, &point, font->height, chr, winTheme.bright, winTheme.dark);
        cursorPos.x++;

        if ((cursorPos.x + 2) * font->width > gfx->width - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_put_backend(gfx, '\n');
        }
    }
    break;
    }
}

void terminal_put(char chr)
{
    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    terminal_put_backend(&gfx, chr);

    win_draw_end(terminal, &gfx);
}

void terminal_print(const char* str)
{
    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    while (*str != '\0')
    {
        terminal_put_backend(&gfx, *str);
        str++;
    }

    win_draw_end(terminal, &gfx);
}

void terminal_error(const char* str)
{
    terminal_print("error: ");
    if (str != NULL)
    {
        terminal_print(str);
    }
    else
    {
        terminal_print(strerror(errno));
    }
    terminal_put('\n');
}
