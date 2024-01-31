#include "rlsmenu.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

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

static rlsmenu_frame *init_frame(rlsmenu_gui *gui, rlsmenu_frame *frame);
static rlsmenu_frame *init_rlsmenu_list(rlsmenu_frame *);

static void rebuild_menu_str(rlsmenu_gui *gui);
static wchar_t *rebuild_rlsmenu_list(rlsmenu_frame *);

static enum rlsmenu_result update_rlsmenu_list(rlsmenu_frame *frame, enum rlsmenu_input in);

static int longest_item_name(wchar_t **item_names, int n_items);
static void draw_border(wchar_t *, int w, int h);

rlsmenu_frame *(*menu_init_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = init_rlsmenu_list,
};

wchar_t *(*rebuild_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = rebuild_rlsmenu_list,
};

enum rlsmenu_result (*update_handler_for[])(rlsmenu_frame *, enum rlsmenu_input) = {
    [RLSMENU_LIST] = update_rlsmenu_list,
};

static wchar_t *idx_to_alpha = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

typedef struct node node;
struct node {
    node *next;
    void *data;
};

// Takes ownership of data
static void push(node **head, void *data) {
    node *n = malloc(sizeof(*n));
    n->data = data;
    n->next = *head;
    *head = n;
}

// Caller is in charge of returned memory
static node *pop(node **head) {
    if (!*head) return NULL;

    node *tmp = *head;
    *head = (*head)->next;
    tmp->next = NULL;

    return tmp;
}

enum rlsmenu_result rlsmenu_update(rlsmenu_gui *gui, enum rlsmenu_input in) {
    rlsmenu_frame *frame = gui->frame_stack->data;

    enum rlsmenu_result res = update_handler_for[frame->type](frame, in);
    if (res == RLSMENU_DONE) {
        // Client has to handle return stack
        // TODO: Make functions to interact with the return stack
        node *n = pop(&gui->frame_stack);
        free(n->data);
        free(n);
        gui->should_rebuild_menu_str = true;
    }

    return res;
}

static enum rlsmenu_result update_rlsmenu_list(rlsmenu_frame *frame, enum rlsmenu_input in) {
    (void) frame;
    switch (in) {
        case RLSMENU_ESC:
        case RLSMENU_SEL:
            return RLSMENU_DONE;
        default:
            return RLSMENU_CONT;
    }
}

void rlsmenu_gui_init(rlsmenu_gui *gui) {
    gui->frame_stack = NULL;
    gui->return_stack = NULL;
    gui->top_menu = NULL;
    gui->should_rebuild_menu_str = false;
}

void rlsmenu_gui_deinit(rlsmenu_gui *gui) {
    free(gui->top_menu);
}

// Init frame will copy the data so we don't change the user's template
void rlsmenu_gui_push(rlsmenu_gui *gui, rlsmenu_frame *frame) {
    frame = init_frame(gui, frame);

    push(&gui->frame_stack, frame);
}

// Returns the copied frame
static rlsmenu_frame *init_frame(rlsmenu_gui *gui, rlsmenu_frame *frame) {
    frame->parent = gui;
    gui->should_rebuild_menu_str = true;

    return menu_init_handler_for[frame->type](frame);
}

// Copies and initializes the frame
static rlsmenu_frame *init_rlsmenu_list(rlsmenu_frame *frame) {
    rlsmenu_list *list = malloc(sizeof(*list));
    *list = *(rlsmenu_list *) frame;
    frame = (rlsmenu_frame *) list;

    list->item_names = list->get_item_names(list->items, list->n_items, frame->state);

    int x_border = 0, y_border = 0;
    if (frame->flags & RLSMENU_BORDER)
        x_border = 4, y_border = 2;

    int title_len = frame->title ? wcslen(frame->title) : 0;
    frame->w = max(longest_item_name(list->item_names, list->n_items) + MENU_IDX_WIDTH, title_len) + x_border;
    frame->h = list->n_items + !!frame->title + y_border;

    return frame;
}

static int longest_item_name(wchar_t **item_names, int n_items) {
    int max = 0, len;
    for (int i = 0; i < n_items; i++)
        if ((len = wcslen(item_names[i])) > max)
            max = len;

    return max;
}

// Will return NULL if called before pushing a frame
rlsmenu_str rlsmenu_get_menu_str(rlsmenu_gui *gui) {
    if (gui->should_rebuild_menu_str)
        rebuild_menu_str(gui);

    rlsmenu_frame *frame = gui->frame_stack->data;
    return (rlsmenu_str) {
        .w = frame->w,
        .h = frame->h,
        .str = gui->top_menu
    };
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

