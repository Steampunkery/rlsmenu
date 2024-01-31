#pragma once
#include <wchar.h>
#include <stdbool.h>

#define RLSMENU_BORDER_SHIFT 0
#define RLSMENU_BORDER (1 << RLSMENU_BORDER_SHIFT)

enum rlsmenu_type { RLSMENU_LIST };
enum rlsmenu_result { RLSMENU_CONT, RLSMENU_DONE };

// 0-51 are reserved for inputs a - Z
enum rlsmenu_input {
    RLSMENU_ESC = 52,
    RLSMENU_PGUP,
    RLSMENU_PGDN,
    RLSMENU_UP,
    RLSMENU_DN,
    RLSMENU_SEL
};

typedef struct rlsmenu_gui {
    struct node *frame_stack;
    struct node *return_stack;

    // cached string of the top menu frame
    wchar_t *top_menu;
    bool should_rebuild_menu_str;
} rlsmenu_gui;

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

typedef struct rlsmenu_str {
    int w;
    int h;
    wchar_t *str;
} rlsmenu_str;

enum rlsmenu_result rlsmenu_update(rlsmenu_gui *, enum rlsmenu_input);
void rlsmenu_gui_init(rlsmenu_gui *);
void rlsmenu_gui_deinit(rlsmenu_gui *gui);
void rlsmenu_gui_push(rlsmenu_gui *, rlsmenu_frame *);
rlsmenu_str rlsmenu_get_menu_str(rlsmenu_gui *);
