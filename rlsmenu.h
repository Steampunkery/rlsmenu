#pragma once
#include <wchar.h>
#include <stdbool.h>

#define RLSMENU_BORDER_SHIFT 0
#define RLSMENU_BORDER (1 << RLSMENU_BORDER_SHIFT)

enum rlsmenu_type { RLSMENU_LIST, RLSMENU_SLIST, RLSMENU_MSGBOX };
enum rlsmenu_result { RLSMENU_DONE, RLSMENU_CANCELED, RLSMENU_CONT };

// 0-51 are reserved for inputs a - Z
enum rlsmenu_input {
    RLSMENU_INVALID_KEY = -1,
    RLSMENU_ESC = 52,
    RLSMENU_PGUP,
    RLSMENU_PGDN,
    RLSMENU_UP,
    RLSMENU_DN,
    RLSMENU_SEL
};

/* All fields are managed by API functions. Should not be manipulated by
 * users.
 */
typedef struct rlsmenu_gui {
    struct node *frame_stack;
    struct node *return_stack;

    // cached string of the top menu frame
    wchar_t *top_menu;
    bool should_rebuild_menu_str;

    enum rlsmenu_result last_return_code;
} rlsmenu_gui;

// TODO: See if this enum is really necessary in the future. It's in the
// spec but might be redundant
enum rlsmenu_cb_res { RLSMENU_CB_FAILURE, RLSMENU_CB_SUCCESS, RLSMENU_CB_NEW_WIN };
typedef struct rlsmenu_frame rlsmenu_frame;
typedef void (*rlsmenu_cleanup_cb)(rlsmenu_frame *);
typedef struct rlsmenu_cbs {
    enum rlsmenu_cb_res (*on_select)(rlsmenu_frame *frame, void *selection);
    void (*on_complete)(rlsmenu_frame *frame);
    rlsmenu_cleanup_cb cleanup;
} rlsmenu_cbs;

typedef struct rlsmenu_frame {
    // Public fields
    enum rlsmenu_type type;
    int flags;
    wchar_t *title;
    void *state; // Private data pointer to pass to callbacks
    rlsmenu_cbs *cbs;

    // Private fields. Filled in by initializer
    rlsmenu_gui *parent;
    int w, h;
    bool from_child_frame;
} rlsmenu_frame;

// The shared fields of lists. All members are public.
typedef struct rlsmenu_list_shared {
    rlsmenu_frame frame;

    void *items;
    size_t item_size;
    int n_items;
    wchar_t const **item_names;
} rlsmenu_list_shared;

typedef struct rlsmenu_list {
    rlsmenu_list_shared s;
} rlsmenu_list;

typedef struct rlsmenu_slist {
    rlsmenu_list_shared s;
    int sel;
} rlsmenu_slist;

// All fields public
typedef struct rlsmenu_msgbox {
    rlsmenu_frame frame;
    wchar_t const **lines;
    int n_lines;
} rlsmenu_msgbox;

typedef struct rlsmenu_str {
    int w;
    int h;
    wchar_t *str;
    bool has_changed;
} rlsmenu_str;

// Provides input to a GUI object and updates the state correspondingly
enum rlsmenu_result rlsmenu_update(rlsmenu_gui *, enum rlsmenu_input);

// Initializes an rlsmenu_gui struct
void rlsmenu_gui_init(rlsmenu_gui *);

// Cleans up and frees the rlsmenu_gui struct
void rlsmenu_gui_deinit(rlsmenu_gui *gui);

// Pushes a new GUI frame. Copies frame so the template can be reused
void rlsmenu_gui_push(rlsmenu_gui *, rlsmenu_frame *);

// Lazily updates and returns the menu string of the top frame
rlsmenu_str rlsmenu_get_menu_str(rlsmenu_gui *);

// Pushes a data pointer onto the return stack
void rlsmenu_push_return(rlsmenu_gui *gui, void *data);

/*
 * Pops the top node of the return stack and returns the data pointer
 * contained within. Returns NULL on an empty stack.
 *
 * Note: It is impossible to distinguish between an empty return stack and
 * a NULL data pointer. TODO: Come up with a better scheme or implement
 * rlsmenu_is_return_stack_empty
 */
void *rlsmenu_pop_return(rlsmenu_gui *gui);
