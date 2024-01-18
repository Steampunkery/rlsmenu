#include "rlsmenu.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <time.h>

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define MENU_IDX_WIDTH 4

#define BOX_H L'\u2500'
#define BOX_V L'\u2502'
#define BOX_TL L'\u250C'
#define BOX_BL L'\u2514'
#define BOX_TR L'\u2510'
#define BOX_BR L'\u2518'

static void init_frame(rlsmenu_gui *gui, rlsmenu_frame *frame);
static void init_rlsmenu_list(rlsmenu_frame *);

static void rebuild_menu_str(rlsmenu_gui *gui);
static wchar_t *rebuild_rlsmenu_list(rlsmenu_frame *);

static int longest_item_name(wchar_t **item_names, int n_items);
static void draw_border(wchar_t *, int w, int h);

void (*menu_init_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = init_rlsmenu_list,
};

wchar_t *(*rebuild_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = rebuild_rlsmenu_list,
};

static wchar_t *idx_to_alpha = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

typedef struct node node;
struct node {
    node *next;
    void *data;
};

typedef struct rlsmenu_gui {
    node *frame_stack;
    node *return_stack;

    wchar_t *top_menu;
    bool should_rebuild_menu_str;
} rlsmenu_gui;

static void push(node **head, void *data) {
    node *n = malloc(sizeof(*n));
    n->data = data;
    n->next = *head;
    *head = n;
}

static node *pop(node **head) {
    if (!*head) return NULL;

    node *tmp = *head;
    *head = (*head)->next;
    tmp->next = NULL;

    return tmp;
}

void rlsmenu_gui_init(rlsmenu_gui *gui) {
    gui->frame_stack = NULL;
    gui->return_stack = NULL;
    gui->top_menu = NULL;
}

void rlsmenu_gui_push(rlsmenu_gui *gui, rlsmenu_frame *frame) {
    init_frame(gui, frame);

    push(&gui->frame_stack, frame);
}

static void init_frame(rlsmenu_gui *gui, rlsmenu_frame *frame) {
    frame->parent = gui;
    gui->should_rebuild_menu_str = true;

    menu_init_handler_for[frame->type](frame);
}

static void init_rlsmenu_list(rlsmenu_frame *frame) {
    rlsmenu_list *list = (rlsmenu_list *) frame;

    list->item_names = list->get_item_names(list->items, list->n_items, frame->state);

    int x_border = 0, y_border = 0;
    if (frame->flags & RLSMENU_BORDER)
        x_border = 4, y_border = 2;

    int title_len = frame->title ? wcslen(frame->title) : 0;
    frame->w = max(longest_item_name(list->item_names, list->n_items) + MENU_IDX_WIDTH, title_len) + x_border;
    frame->h = list->n_items + !!frame->title + y_border;
}

static int longest_item_name(wchar_t **item_names, int n_items) {
    int max = 0, len;
    for (int i = 0; i < n_items; i++)
        if ((len = wcslen(item_names[i])) > max)
            max = len;

    return max;
}

wchar_t *rlsmenu_get_menu_str(rlsmenu_gui *gui) {
    if (gui->should_rebuild_menu_str)
        rebuild_menu_str(gui);

    return gui->top_menu;
}

static void rebuild_menu_str(rlsmenu_gui *gui) {
    rlsmenu_frame *frame = gui->frame_stack->data;
    free(gui->top_menu);
    gui->top_menu = rebuild_handler_for[frame->type](frame);
    gui->should_rebuild_menu_str = false;
}

/*
 * This function employs a number of dirty hacks to avoid the weirdness of
 * swprintf and printing null chars. Consider finding a better way to do
 * this.
 */
static wchar_t *rebuild_rlsmenu_list(rlsmenu_frame *frame) {
    rlsmenu_list *list = (rlsmenu_list *) frame;
    wchar_t *str = calloc(1, sizeof(*str) * frame->w * frame->h);

    int x_off = 0, y_off = 0;
    if (frame->flags & RLSMENU_BORDER)
        x_off+=2, y_off++;

    if (frame->title) {
        swprintf(str + x_off + y_off*frame->w, frame->w, L"%ls", frame->title);
        y_off++;
    }

    for (int i = 0; i < list->n_items; i++) {
        swprintf(str+x_off+(y_off+i)*frame->w, frame->w, L"(%lc) %ls", idx_to_alpha[i], list->item_names[i]);
    }

    // This is a hack to overwrite the null chars swprintf will insert
    if (frame->flags & RLSMENU_BORDER)
        draw_border(str, frame->w, frame->h);

    // Do this to ensure null chars from calloc get printed
    for (int i = 0; i < frame->w * frame->h; i++)
        if (!str[i])
            str[i] = L' ';

    return str;
}

static void draw_border(wchar_t *str, int w, int h) {
    str[0] = BOX_TL;
    str[w-1] = BOX_TR;
    str[0+w*(h-1)] = BOX_BL;
    str[w*h-1] = BOX_BR;

    for (int i = 1; i < w-1; i++) {
        str[i] = BOX_H;
        str[i + w*(h-1)] = BOX_H;
    }

    for (int i = 1; i < h-1; i++) {
        str[i*w] = BOX_V;
        str[i*w+w-1] = BOX_V;
    }

}

wchar_t **f_item_names(void *items, int n_items, void *state) {
    (void) items;
    (void) n_items;
    (void) state;
    static wchar_t *names[] = {L"One", L"Two", L"Three"};
    return names;
}

int main() {
    (void) pop;
    setlocale(LC_ALL, "C.UTF-8");

    rlsmenu_list list = {
        .frame = {
            .type = RLSMENU_LIST,
            .flags = RLSMENU_BORDER,
            .title = L"Test",
            .state = NULL,
        },
        .cbs = { NULL, NULL },
        .items = NULL,
        .n_items = 3,

        .item_names = NULL,
        .get_item_names = f_item_names,
    };

    struct timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &begin);

    init_rlsmenu_list((rlsmenu_frame *) &list);
    wchar_t *win = rebuild_rlsmenu_list((rlsmenu_frame *) &list);

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    double time_spent = (end.tv_nsec - begin.tv_nsec) / 1000000000.0 +
        (end.tv_sec  - begin.tv_sec);

    wprintf(L"Time to compute menu: %lf\n", time_spent);

    for (int i = 0; i < list.frame.w * list.frame.h; i++) {
        if (i && i % list.frame.w == 0) wprintf(L"\n");
        wprintf(L"%lc", win[i]);
    }

    wprintf(L"\n");
    free(win);
}
