#include "rlsmenu.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define MENU_IDX_WIDTH 4

#define BOX_H L'\u2500'
#define BOX_V L'\u2502'
#define BOX_TL L'\u250C'
#define BOX_BL L'\u2514'
#define BOX_TR L'\u2510'
#define BOX_BR L'\u2518'

static rlsmenu_frame *init_frame(rlsmenu_gui *gui, rlsmenu_frame *frame);
static rlsmenu_frame *init_rlsmenu_list(rlsmenu_frame *);
static rlsmenu_frame *init_rlsmenu_slist(rlsmenu_frame *);

static void rebuild_menu_str(rlsmenu_gui *gui);
static wchar_t *rebuild_rlsmenu_list(rlsmenu_frame *);

static enum rlsmenu_result update_rlsmenu_list(rlsmenu_frame *frame, enum rlsmenu_input in);
static enum rlsmenu_result update_rlsmenu_slist(rlsmenu_frame *frame, enum rlsmenu_input in);

static rlsmenu_cleanup_cb cleanup_rlsmenu_list(rlsmenu_frame *frame);

static int longest_item_name(wchar_t **item_names, int n_items);
static void draw_border(wchar_t *, int w, int h);

rlsmenu_frame *(*menu_init_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = init_rlsmenu_list,
    [RLSMENU_SLIST] = init_rlsmenu_slist,
};

wchar_t *(*rebuild_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = rebuild_rlsmenu_list,
    [RLSMENU_SLIST] = rebuild_rlsmenu_list,
};

enum rlsmenu_result (*update_handler_for[])(rlsmenu_frame *, enum rlsmenu_input) = {
    [RLSMENU_LIST] = update_rlsmenu_list,
    [RLSMENU_SLIST] = update_rlsmenu_slist,
};

rlsmenu_cleanup_cb (*cleanup_handler_for[])(rlsmenu_frame *) = {
    [RLSMENU_LIST] = cleanup_rlsmenu_list,
    [RLSMENU_SLIST] = cleanup_rlsmenu_list,
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

static void clear(node *head, bool deep) {
    node *i, *tmp = i = head;
    while (i) {
        i = i->next;
        if (deep) {
            rlsmenu_frame *frame = tmp->data;
            rlsmenu_cleanup_cb f = cleanup_handler_for[frame->type](frame);
            if (f) f(frame);

            free(tmp->data);
        }

        free(tmp);
        tmp = i;
    }
}

static rlsmenu_cleanup_cb cleanup_rlsmenu_list(rlsmenu_frame *frame) {
    return ((rlsmenu_list_shared *) frame)->cbs->cleanup;
}

enum rlsmenu_result rlsmenu_update(rlsmenu_gui *gui, enum rlsmenu_input in) {
    rlsmenu_frame *frame = gui->frame_stack->data;
    if (in == RLSMENU_INVALID_KEY) return RLSMENU_CONT;

    enum rlsmenu_result res = update_handler_for[frame->type](frame, in);
    if (res == RLSMENU_DONE || res == RLSMENU_CANCELED) {
        // Client has to handle return stack
        node *n = pop(&gui->frame_stack);
        free(n->data);
        free(n);
        gui->should_rebuild_menu_str = true;
        gui->last_return_code = res;
    }

    return res;
}

static enum rlsmenu_result update_rlsmenu_list(rlsmenu_frame *frame, enum rlsmenu_input in) {
    rlsmenu_cbs *cbs = ((rlsmenu_list *) frame)->s.cbs;

    switch (in) {
        case RLSMENU_ESC:
        case RLSMENU_SEL:
            if (cbs && cbs->on_complete) cbs->on_complete(frame);
            if (cbs && cbs->cleanup) cbs->cleanup(frame);
            return RLSMENU_DONE;
        default:
            return RLSMENU_CONT;
    }
}

// Validity of the selection should be handled by caller
static enum rlsmenu_result process_selection(rlsmenu_frame *frame, rlsmenu_cbs *cbs, void *selection) {
    enum rlsmenu_cb_res res;
    if (cbs && cbs->on_select)
        res = cbs->on_select(frame, selection);
    else
        res = RLSMENU_CB_SUCCESS;

    switch (res) {
        case RLSMENU_CB_SUCCESS:
            if (cbs && cbs->on_complete) cbs->on_complete(frame);
            if (cbs && cbs->cleanup) cbs->cleanup(frame);

            return RLSMENU_DONE;
        case RLSMENU_CB_FAILURE:
            return RLSMENU_CONT;
        case RLSMENU_CB_NEW_WIN:
            frame->from_child_frame = true;
            return RLSMENU_CONT;
    }

    return RLSMENU_CANCELED;
}

static enum rlsmenu_result process_child_return(rlsmenu_frame *frame, rlsmenu_cbs *cbs) {
    frame->from_child_frame = false;
    switch (frame->parent->last_return_code) {
        case RLSMENU_DONE:
            if (cbs && cbs->on_complete)
                cbs->on_complete(frame);

            return RLSMENU_DONE;
        case RLSMENU_CANCELED:
            return RLSMENU_CONT;
        default:
            abort(); // Consider an assert here
    }
}

static enum rlsmenu_result update_rlsmenu_slist(rlsmenu_frame *frame, enum rlsmenu_input in) {
    rlsmenu_slist *slist = (rlsmenu_slist *) frame;

    if (frame->from_child_frame)
        return process_child_return(frame, slist->s.cbs);

    if (in >= 0 && (int) in < slist->s.n_items)
        return process_selection(frame, slist->s.cbs, slist->s.items + in * slist->s.item_size);

    switch (in) {
        case RLSMENU_ESC:
            return RLSMENU_CANCELED;
        case RLSMENU_SEL:
            if (slist->sel >= 0 && slist->sel < slist->s.n_items)
                return process_selection(frame, slist->s.cbs,
                        slist->s.items + slist->sel * slist->s.item_size);
            return RLSMENU_CONT;
        case RLSMENU_UP:
            slist->sel = max(0, slist->sel-1);
            frame->parent->should_rebuild_menu_str = true;
            return RLSMENU_CONT;
        case RLSMENU_DN:
            slist->sel = min(slist->s.n_items-1, slist->sel+1);
            frame->parent->should_rebuild_menu_str = true;
            return RLSMENU_CONT;
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

// Note: This will leak any allocated memory in the return stack
void rlsmenu_gui_deinit(rlsmenu_gui *gui) {
    clear(gui->frame_stack, true);
    clear(gui->return_stack, false);
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
    frame->from_child_frame = false;

    return menu_init_handler_for[frame->type](frame);
}

static void init_rlsmenu_list_shared(rlsmenu_list_shared *s) {
    rlsmenu_frame *frame = (rlsmenu_frame *) s;

    int x_border = 0, y_border = 0;
    if (frame->flags & RLSMENU_BORDER)
        x_border = 4, y_border = 2;

    int title_len = frame->title ? wcslen(frame->title) : 0;
    frame->w = max(longest_item_name(s->item_names, s->n_items) + MENU_IDX_WIDTH, title_len) + x_border;
    frame->h = s->n_items + !!frame->title + y_border;
}

static rlsmenu_frame *init_rlsmenu_list(rlsmenu_frame *frame) {
    rlsmenu_list *list = malloc(sizeof(*list));
    *list = *(rlsmenu_list *) frame;

    init_rlsmenu_list_shared(&list->s);
    return (rlsmenu_frame *) list;
}

static rlsmenu_frame *init_rlsmenu_slist(rlsmenu_frame *frame) {
    rlsmenu_slist *slist = malloc(sizeof(*slist));
    *slist = *(rlsmenu_slist *) frame;

    init_rlsmenu_list_shared(&slist->s);
    slist->sel = -1;
    return (rlsmenu_frame *) slist;
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
    if (!gui->frame_stack)
        return (rlsmenu_str) { .str = NULL };

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
    wchar_t *str = calloc(1, sizeof(*str) * (frame->w * frame->h + 1));

    int x_off = 0, y_off = 0;
    if (frame->flags & RLSMENU_BORDER)
        x_off+=2, y_off++;

    if (frame->title) {
        swprintf(str + x_off + y_off*frame->w, frame->w, L"%ls", frame->title);
        y_off++;
    }

    for (int i = 0; i < list->s.n_items; i++) {
        if (frame->type == RLSMENU_SLIST && ((rlsmenu_slist *) frame)->sel == i)
            swprintf(str+x_off+(y_off+i)*frame->w, frame->w, L"(*) %ls", list->s.item_names[i]);
        else
            swprintf(str+x_off+(y_off+i)*frame->w, frame->w, L"(%lc) %ls", idx_to_alpha[i], list->s.item_names[i]);
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

void rlsmenu_push_return(rlsmenu_gui *gui, void *data) {
    push(&gui->return_stack, data);
}

void *rlsmenu_pop_return(rlsmenu_gui *gui) {
    node *tmp = pop(&gui->return_stack);

    if (!tmp) return NULL;
    void *data = tmp->data;
    free(tmp);

    return data;
}
