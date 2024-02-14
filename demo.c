#include "rlsmenu.h"

#include <locale.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define ENTER 0xa
#define ESC   0x1b

wchar_t **f_item_names(void *items, int n_items, void *state) {
    (void) items;
    (void) n_items;
    (void) state;
    static wchar_t *names[] = {L"One", L"Two", L"Three"};
    return names;
}

int translate_input(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    else if (c >= 'A' && c <= 'Z') return c - 'A';

    switch (c) {
        case ENTER:
            return RLSMENU_SEL;
        case ESC:
            return RLSMENU_ESC;
        default:
            return -1;
    }
}

enum rlsmenu_cb_res on_complete(rlsmenu_frame *frame) {
    (void) frame;
    wprintf(L"\nPrinted from on_complete!");
    return RLSMENU_CB_DONE;
}

rlsmenu_list list_tmp = {
    .frame = {
        .type = RLSMENU_LIST,
        .flags = RLSMENU_BORDER,
        .title = L"Test",
        .state = NULL,
    },
    .cbs = &(rlsmenu_cbs) { NULL, on_complete },
    .items = NULL,
    .n_items = 3,

    .item_names = NULL,
    .get_item_names = f_item_names,
};

void draw_menu(rlsmenu_str menu_str) {
    for (int i = 0; i < menu_str.w * menu_str.h; i++) {
        if (i && i % menu_str.w == 0) wprintf(L"\n");
        wprintf(L"%lc", menu_str.str[i]);
    }
}

int main() {
    setlocale(LC_ALL, "C.UTF-8");

    struct termios old;
    struct termios new;
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new);

    rlsmenu_gui gui;
    rlsmenu_gui_init(&gui);

    rlsmenu_gui_push(&gui, (rlsmenu_frame *) &list_tmp);

    wprintf(L"\e[?25l\e[1;1H\e[2J");
    rlsmenu_str menu_str = rlsmenu_get_menu_str(&gui);
    draw_menu(menu_str);

    int c, in;
    enum rlsmenu_result res;
    while ((c = getchar()) != -1) {
        if ((in = translate_input((char) c)) == -1) continue;

        if ((res = rlsmenu_update(&gui, in)) == RLSMENU_DONE) break;

        menu_str = rlsmenu_get_menu_str(&gui);
        if (!menu_str.str) break;
        wprintf(L"\e[1;1H");
        draw_menu(menu_str);
    }

    wprintf(L"\n");
    wprintf(L"\e[?25h");

    rlsmenu_gui_deinit(&gui);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
}
