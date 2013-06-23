/*
  This file is part of Musca.

  Musca is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as publishead by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Musca is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Musca.  If not, see <http://www.gnu.org/licenses/>.

  Sean Pringle
  sean dot pringle at gmail dot com
  https://launchpad.net/musca
*/

#define DEBUG 1
#define VERSION "0.1.1"
#define APIS_CLASS "apis"

#include "tools.c"
#include "xwin.c"

Display *display;
Window root;
Screen *screen;

enum { NONE, LEFT, RIGHT, TOP, BOTTOM };

typedef struct {
	int x, y, w, h;
	void *ptr;
	Window l, r, t, b;
} box;

#define BOXES 32

typedef struct _grid {
	int w, h, count, active;
	box boxes[BOXES];
	ucell flags;
} grid;

#define GRID_VISIBLE 1

ucell NumlockMask;
Window drag_win = None; int drag_x, drag_y, drag_button;
XWindowAttributes drag_attr;
Window canvas;
#define MAX_DESKTOPS 10
ucell desktops;
ucell current;
winlist windows;

ucell pad_left = 0, pad_right = 0, pad_top = 0, pad_bottom = 0,
	screen_x = 0, screen_y = 0, screen_width = 0, screen_height = 0;

grid *grids[MAX_DESKTOPS];

#define WINDOW_KNOWN 1
#define WINDOW_TILED (1<<1)
#define WINDOW_BORDER (1<<2)
#define WINDOW_HINTS (1<<3)
#define WINDOW_CONFIG (1<<4)

#define WINDOW_STATE_RANGE 15
#define WINDOW_MODAL (1<<16)
#define WINDOW_STICKY (1<<17)
#define WINDOW_MAXVERT (1<<18)
#define WINDOW_MAXHORZ (1<<19)
#define WINDOW_SHADED (1<<20)
#define WINDOW_SKIPTASKBAR (1<<21)
#define WINDOW_SKIPPAGER (1<<22)
#define WINDOW_HIDDEN (1<<23)
#define WINDOW_FULLSCREEN (1<<24)
#define WINDOW_ABOVE (1<<25)
#define WINDOW_BELOW (1<<26)
#define WINDOW_ATTENTION  (1<<27)

#define WINDOW_DEFAULTS (WINDOW_BORDER|WINDOW_HINTS)

struct name_to_flag {
	char name[32];
	ucell flag;
};

struct name_to_flag names_to_flags[] = {
	{ "all", 0xFFFFFFFF },
	{ "none", 0 },
	{ "defaults", WINDOW_DEFAULTS },
	{ "hints", WINDOW_HINTS },
	{ "tile", WINDOW_TILED },
	{ "border", WINDOW_BORDER },
	{ "modal", WINDOW_MODAL },
	{ "sticky", WINDOW_STICKY },
	{ "maximized_vert", WINDOW_MAXHORZ },
	{ "maximized_horz", WINDOW_MAXVERT},
	{ "shaded", WINDOW_SHADED },
	{ "skip_taskbar", WINDOW_SKIPTASKBAR },
	{ "skip_pager", WINDOW_SKIPPAGER },
	{ "hidden", WINDOW_HIDDEN },
	{ "fullscreen", WINDOW_FULLSCREEN },
	{ "above", WINDOW_ABOVE },
	{ "below", WINDOW_BELOW },
	{ "demands_attention", WINDOW_ATTENTION },
};

hash *flagnames;

typedef struct _control {
	char *key;
	ucell mod;
	void (*cb)();
} control;

typedef struct _command {
	char name[16];
	ubyte code;
} command;

enum { OP_NOP,
OP_SPLIT, OP_REMOVE, OP_OVERLAY, OP_RULE, OP_SHOW, OP_FOCUS, OP_SIZE, OP_RESET, OP_CONFIG, OP_FLAGS, OP_HOOK,
OP_LAST };

typedef struct {
	ucell rflags;
	char class[NOTE];
	char name[NOTE];
	char role[NOTE];
	char type[NOTE];
	char tag[NOTE];
	byte desktop;
	ucell setflags, clrflags, flipflags;
	int x, y, w, h, f;
	int cols, rows;
} rule;

#define RULE_CLASS 1
#define RULE_NAME (1<<1)
#define RULE_ROLE (1<<2)
#define RULE_DESKTOP (1<<3)
#define RULE_FLAGS (1<<4)
#define RULE_X (1<<5)
#define RULE_Y (1<<6)
#define RULE_W (1<<7)
#define RULE_H (1<<8)
#define RULE_F (1<<9)
#define RULE_LEFT (1<<10)
#define RULE_RIGHT (1<<11)
#define RULE_TOP (1<<12)
#define RULE_BOTTOM (1<<13)
#define RULE_COVER (1<<14)
#define RULE_COLS (1<<17)
#define RULE_ROWS (1<<18)
#define RULE_XR (1<<19)
#define RULE_YR (1<<20)
#define RULE_WR (1<<21)
#define RULE_HR (1<<22)
#define RULE_COLSR (1<<23)
#define RULE_ROWSR (1<<24)
#define RULE_MASK (1<<25)
#define RULE_TYPE (1<<26)
#define RULE_RAISE (1<<27)
#define RULE_FOCUS (1<<28)
#define RULE_TAG (1<<29)

struct rule_iter {
	rule *rule;
	winlist siblings;
};

enum { HOOK_SWITCH_DESKTOP,
HOOK_LAST};

char *hook_names[HOOK_LAST] = {
	[HOOK_SWITCH_DESKTOP] = "switch_desktop",
};
hash *hook_commands;

typedef struct {
	Window win, trans, parent, *kids;
	ucell nkids;
	char class[NOTE];
	char name[NOTE];
	char role[NOTE];
	char tag[NOTE];
	ucell ewmh_type;
	char ewmh_type_name[NOTE];
	ucell ewmh_state;
	int state;
	int desktop;
	ucell flags;
	ucell left, right, top, bottom;
	XWindowAttributes attr;
	XWMHints *hints;
} profile;

hash *profiles;

#include "apis_proto.h"

void (*event[LASTEvent])(XEvent*) = {
	[KeyPress] = event_KeyPress,
	[KeyRelease] = event_KeyRelease,
	[ButtonPress] = event_ButtonPress,
	[ButtonRelease] = event_ButtonRelease,
	[MotionNotify] = event_MotionNotify,
	[EnterNotify] = event_EnterNotify,
	[LeaveNotify] = event_LeaveNotify,
	[FocusIn] = event_FocusIn,
	[FocusOut] = event_FocusOut,
	[KeymapNotify] = event_KeymapNotify,
	[Expose] = event_Expose,
	[GraphicsExpose] = event_GraphicsExpose,
	[NoExpose] = event_NoExpose,
	[VisibilityNotify] = event_VisibilityNotify,
	[CreateNotify] = event_CreateNotify,
	[DestroyNotify] = event_DestroyNotify,
	[UnmapNotify] = event_UnmapNotify,
	[MapNotify] = event_MapNotify,
	[MapRequest] = event_MapRequest,
	[ReparentNotify] = event_ReparentNotify,
	[ConfigureNotify] = event_ConfigureNotify,
	[ConfigureRequest] = event_ConfigureRequest,
	[GravityNotify] = event_GravityNotify,
	[ResizeRequest] = event_ResizeRequest,
	[CirculateNotify] = event_CirculateNotify,
	[CirculateRequest] = event_CirculateRequest,
	[PropertyNotify] = event_PropertyNotify,
	[SelectionClear] = event_SelectionClear,
	[SelectionRequest] = event_SelectionRequest,
	[SelectionNotify] = event_SelectionNotify,
	[ColormapNotify] = event_ColormapNotify,
	[ClientMessage] = event_ClientMessage,
	[MappingNotify] = event_MappingNotify,
#ifdef GenericEvent
	[GenericEvent] = event_GenericEvent,
#endif
};

bool (*opcode[OP_LAST])(char*, autostr*) = {
	[OP_NOP] = NULL,
	[OP_SPLIT] = op_split,
	[OP_REMOVE] = op_remove,
	[OP_OVERLAY] = op_overlay,
	[OP_RULE] = op_rule,
	[OP_SHOW] = op_show,
	[OP_FOCUS] = op_focus,
	[OP_SIZE] = op_size,
	[OP_RESET] = op_reset,
	[OP_CONFIG] = op_config,
	[OP_HOOK] = op_hook,
};

command commands[] = {
	{ "split", OP_SPLIT },
	{ "remove", OP_REMOVE },
	{ "overlay", OP_OVERLAY },
	{ "rule", OP_RULE },
	{ "show", OP_SHOW },
	{ "focus", OP_FOCUS },
	{ "size", OP_SIZE },
	{ "reset", OP_RESET },
	{ "config", OP_CONFIG },
	{ "hook", OP_HOOK },
};

hash *commandnames;

hash *session;
stack *rules;
Window lastactive = None;
ucell check;

#define DESKTOP_ALL -1
#define DESKTOP_NONE -2

