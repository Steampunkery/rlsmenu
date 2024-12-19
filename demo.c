#include "rlsmenu.h"

#include <locale.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define ENTER 0xa
#define ESC   0x1b
#define UP    0x41
#define DN    0x42
#define PGUP  0x35
#define PGDN  0x36

// Blocking
int getchar() {
    int n, c;
    while ((n = read(STDIN_FILENO, &c, 1)) == -1 && errno == EAGAIN)
        usleep(10000); // Don't peg the CPU for input (10ms)
    return n > 0 ? c : EOF;
}

// Non-blocking
int getchar_nb() {
    int n, c;
    n = read(STDIN_FILENO, &c, 1);

    if (n == 0)
        return EOF;

    if (n == -1) {
        if (errno == EAGAIN)
            return -2;
        return EOF;
    }

    return c;
}

int translate_input(char c) {
    if (c != 'q' && c >= 'a' && c <= 'z') return c - 'a';
    else if (c >= 'A' && c <= 'Z') return c - 'A';

    int ch;
    switch (c) {
        case ENTER:
            return RLSMENU_SEL;
        case 'q':
            return RLSMENU_ESC;
        case ESC:
            // Consume the [ if we're in an escape code. If we're not in an
            // escape code, this should time out (return -2). Note that on
            // older terminals it takes time to transmit each byte in the
            // escape sequence, and this doesn't wait at all. If you're using a
            // physical terminal, I guess you're SOL.
            if (getchar_nb() == -2) return RLSMENU_ESC;
            // Return -1 here. This should bubble up elsewhere.
            if ((ch = getchar()) == -1) return -1;
            switch (ch) {
                case UP:
                    return RLSMENU_UP;
                case DN:
                    return RLSMENU_DN;
                case PGUP:
                    return RLSMENU_PGUP;
                case PGDN:
                    return RLSMENU_PGDN;
                default:
                    return -1;
            }
        default:
            return -1;
    }
}

void on_complete(rlsmenu_frame *frame) {
    (void) frame;
    wprintf(L"Printed from on_complete!\n");
}

enum rlsmenu_cb_res on_select(rlsmenu_frame *frame, void *selection) {
    rlsmenu_push_return(frame->parent, selection);

    return RLSMENU_CB_SUCCESS;
}

char *items[] = { "a", "b", "c" };
wchar_t *item_names[] = { L"One", L"Two", L"Three" };

rlsmenu_slist list_tmp = {
    .s = {
        .frame = {
            .type = RLSMENU_SLIST,
            .flags = RLSMENU_BORDER,
            .title = L"Selection List Test",
            .state = NULL,
            .cbs = &(rlsmenu_cbs) { on_select, on_complete, NULL },
        },
        .items = items,
        .item_size = sizeof(char *),
        .n_items = 3,

        .item_names = item_names,
    },
};

rlsmenu_msgbox msgbox_tmp = {
    .frame = {
        .type = RLSMENU_MSGBOX,
        .flags = RLSMENU_BORDER,
        .title = L"Message Box Test",
        .state = NULL,
        .cbs = &(rlsmenu_cbs) { NULL, NULL, NULL },
    },

    .lines = (wchar_t *[]) {
        L"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed nisl nisi,",
        L"efficitur eu orci vel, rutrum porta ligula. Quisque eget rhoncus orci.",
        L"Vestibulum a elit ac est suscipit dictum. Sed mollis enim a turpis sodales",
        L"gravida. Nulla porttitor suscipit tortor, eu maximus nulla hendrerit et.",
        L"Sed tincidunt dictum ex et euismod. Vivamus tincidunt dui non malesuada",
        L"pellentesque."
    },
    .n_lines = 6,
};

void draw_menu(rlsmenu_str menu_str) {
    wprintf(L"\e[1;1H");
    for (int i = 0; i < menu_str.w * menu_str.h; i += menu_str.w)
        wprintf(L"%.*ls\n", menu_str.w, menu_str.str+i);
}

int run_menu(rlsmenu_gui *gui) {
    rlsmenu_str menu_str = rlsmenu_get_menu_str(gui);
    draw_menu(menu_str);

    int c, in;
    enum rlsmenu_result res;
    while ((c = getchar()) != EOF) {
        if ((in = translate_input((char) c)) == -1) continue;

        res = rlsmenu_update(gui, in);
        switch (res) {
            case RLSMENU_DONE:
            case RLSMENU_CANCELED:
                goto out;
            default:
                break;
        }

        menu_str = rlsmenu_get_menu_str(gui);
        if (!menu_str.str) break;
        if (menu_str.has_changed)
            draw_menu(menu_str);
    }

out:
    return c == EOF ? EOF : 0;
}

int main() {
    // Setup terminal for wide chars
    setlocale(LC_ALL, "C.UTF-8");

    // Save the old terminal configuration
    struct termios old;
    struct termios new;
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new);

    // Allow non-blocking input
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // Use the alternate buffer, hide the cursor, send cursor to top left,
    // clear screen, then draw menu
    wprintf(L"\e[?1049h\e[?25l\e[1;1H\e[2J");

    // Initialize the gui object
    rlsmenu_gui gui;
    rlsmenu_gui_init(&gui);

    // Push the first frame (selection list)
    rlsmenu_gui_push(&gui, (rlsmenu_frame *) &list_tmp);
    if (run_menu(&gui) == -1)
        goto out;

    // Pop the result and inspect it
    char **result = rlsmenu_pop_return(&gui);
    if (result)
        wprintf(L"Selection is: %s\n", *result);
    else
        wprintf(L"Cancelled\n");

    // Give time to see the result, then prepare for next menu
    sleep(2);
    wprintf(L"\e[1;1H\e[2J");

    // Push the next frame (message box)
    rlsmenu_gui_push(&gui, (rlsmenu_frame *) &msgbox_tmp);
    if (run_menu(&gui) == -1)
        goto out;

    // Could be other stuff here in the future

out:
    // Use main buffer, show cursor
    wprintf(L"\e[?1049l\e[?25h");
    // Release the gui object
    rlsmenu_gui_deinit(&gui);
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
}
