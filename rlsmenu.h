#pragma once
#include <wchar.h>

#define RLSMENU_BORDER_SHIFT 0
#define RLSMENU_BORDER (1 << RLSMENU_BORDER_SHIFT)

enum rlsmenu_type { RLSMENU_LIST };

typedef struct rlsmenu_gui rlsmenu_gui;

typedef struct rlsmenu_frame {
    enum rlsmenu_type type;
    int flags;
    wchar_t *title;
    void *state;

    rlsmenu_gui *parent;
    int w, h;
} rlsmenu_frame;

typedef struct rlsmenu_cbs {
    enum rlsmenu_cb_res (*on_select)(void *selection, rlsmenu_frame *frame);
    enum rlsmenu_cb_res (*on_complete)(rlsmenu_frame *frame);
} rlsmenu_cbs;

typedef struct rlsmenu_list {
    rlsmenu_frame frame;
    rlsmenu_cbs cbs;
    void *items;
    int n_items;

    wchar_t **item_names;
    wchar_t **(*get_item_names)(void *items, int n_items, void *state);
} rlsmenu_list;

