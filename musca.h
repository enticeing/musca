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

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

static int (*xerrorxlib)(Display *, XErrorEvent *);

enum {
WMProtocols,
WMDelete,
WMState,
WMClass,
WMName,
WMRole,
WMChangeState,

MuscaCommand,
MuscaReady,
MuscaResult,
MuscaType,
UTF8String,

NetSupported,
NetWMName,
NetClientList,
NetClientListStacking,
NetActiveWindow,
NetNumberOfDesktops,
NetDesktopNames,
NetCurrentDesktop,
NetDesktopGeometry,
NetCloseWindow,
NetSupportingWMCheck,

NetWMWindowType,
NetWMWindowTypeDesktop,
NetWMWindowTypeDock,
NetWMWindowTypeToolbar,
NetWMWindowTypeMenu,
NetWMWindowTypeUtility,
NetWMWindowTypeSplash,
NetWMWindowTypeDialog,
NetWMWindowTypeDropdownMenu,
NetWMWindowTypePopupMenu,
NetWMWindowTypeTooltip,
NetWMWindowTypeNotification,
NetWMWindowTypeCombo,
NetWMWindowTypeDND,
NetWMWindowTypeNormal,

NetWMState,
NetWMStateModal,
NetWMStateSticky,
NetWMStateMaximizedVert,
NetWMStateMaximizedHorz,
NetWMStateShaded,
NetWMStateSkipTaskbar,
NetWMStateSkipPager,
NetWMStateHidden,
NetWMStateFullscreen,
NetWMStateAbove,
NetWMStateBelow,
NetWMStateDemandsAttention,

NetWMPid,
NetWMDesktop,

AtomLast };

static Atom atoms[AtomLast];

char *atom_names[] = {
	"WM_PROTOCOLS",
	"WM_DELETE_WINDOW",
	"WM_STATE",
	"WM_CLASS",
	"WM_NAME",
	"WM_WINDOW_ROLE",
	"WM_CHANGE_STATE",

	"MUSCA_COMMAND",
	"MUSCA_READY",
	"MUSCA_RESULT",
	"MUSCA_TYPE",
	"UTF8_STRING",

	"_NET_SUPPORTED",
	"_NET_WM_NAME",
	"_NET_CLIENT_LIST",
	"_NET_CLIENT_LIST_STACKING",
	"_NET_ACTIVE_WINDOW",
	"_NET_NUMBER_OF_DESKTOPS",
	"_NET_DESKTOP_NAMES",
	"_NET_CURRENT_DESKTOP",
	"_NET_DESKTOP_GEOMETRY",
	"_NET_CLOSE_WINDOW",
	"_NET_SUPPORTING_WM_CHECK",

	"_NET_WM_WINDOW_TYPE",
	"_NET_WM_WINDOW_TYPE_DESKTOP",
	"_NET_WM_WINDOW_TYPE_DOCK",
	"_NET_WM_WINDOW_TYPE_TOOLBAR",
	"_NET_WM_WINDOW_TYPE_MENU",
	"_NET_WM_WINDOW_TYPE_UTILITY",
	"_NET_WM_WINDOW_TYPE_SPLASH",
	"_NET_WM_WINDOW_TYPE_DIALOG",
	"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	"_NET_WM_WINDOW_TYPE_POPUP_MENU",
	"_NET_WM_WINDOW_TYPE_TOOLTIP",
	"_NET_WM_WINDOW_TYPE_NOTIFICATION",
	"_NET_WM_WINDOW_TYPE_COMBO",
	"_NET_WM_WINDOW_TYPE_DND",
	"_NET_WM_WINDOW_TYPE_NORMAL",

	"_NET_WM_STATE",
	"_NET_WM_STATE_MODAL",
	"_NET_WM_STATE_STICKY",
	"_NET_WM_STATE_MAXIMIZED_VERT",
	"_NET_WM_STATE_MAXIMIZED_HORZ",
	"_NET_WM_STATE_SHADED",
	"_NET_WM_STATE_SKIP_TASKBAR",
	"_NET_WM_STATE_SKIP_PAGER",
	"_NET_WM_STATE_HIDDEN",
	"_NET_WM_STATE_FULLSCREEN",
	"_NET_WM_STATE_ABOVE",
	"_NET_WM_STATE_BELOW",
	"_NET_WM_STATE_DEMANDS_ATTENTION",

	"_NET_WM_PID",
	"_NET_WM_DESKTOP"
};

#define MUSCA_CLASS "musca"

struct _group;
struct _head;
struct _client;

typedef struct _frame {
	int id, x, y, w, h;
	struct _group *group;
	struct _client *cli;
	Window win;
	ubyte flags;
	ucell state;
	int sx, sy, sw, sh;
	struct _frame *next;
	struct _frame *prev;
} frame;

#define FF_DEDICATE 1
#define FF_CATCHALL (1<<1)
#define FF_HIDEBORDER (1<<2)

#define NEXT 1
#define PREV 0

#define FOR_RING(d,v,f,i) for ((v) = (f), (i) = 0; (f) && (v) && (!(i) || (v) != (f)); (v) = (d) ? (v)->next: (v)->prev, (i)++)
#define FOR_LIST(v,f,i) for ((v) = (f), (i) = 0; (f) && (v); (v) = (v)->next, (i)++)

enum { ANY, RECENT };

typedef struct _client {
	int id;
	Window win;
	struct _frame *frame;
	struct _group *group;
	char name[NOTE];
	char class[NOTE];
	char role[NOTE];
	int x, y, w, h;
	ubyte flags;
	ubyte kids;
	// use for resize/move in stack mode
	int mxr, myr, mb, mx, my, mw, mh;
	// last known floating position
	int fx, fy, fw, fh;
	// temporary borders during resize/move
	Window rl, rr, rt, rb;
	int unmaps;
	ucell state;
	ucell netwmstate;
	Bool input;
	struct _client *parent;
	struct _client *next;
	struct _client *prev;
} client;

#define CF_HIDDEN 1
#define CF_KILLED (1<<1)
#define CF_HINTS (1<<2)
#define CF_NORMAL (1<<3)
#define CF_SHRUNK (1<<4)

#define CF_INITIAL (CF_HIDDEN)

typedef struct _group {
	int id;
	frame *frames;
	client *clients;
	int l, r, t, b;
	char name[32];
	stack *states;
	ubyte flags;
	struct _head *head;
	struct _group *next;
	struct _group *prev;
	// client stack order
	stack *stacked;
} group;

#define GF_TILING 1
#define GF_STACKING (1<<1)

typedef struct _head {
	int id;
	Screen *screen;
	group *groups;
	char *display_string;
	stack *above;
	stack *below;
	stack *fullscreen;
	struct _head *next;
	struct _head *prev;
	// group stack order
	stack *stacked;
	Window ewmh;
} head;

Display *display;
head *heads;

struct frame_match {
	frame *frame;
	int side;
};

#define LEFT 1
#define RIGHT 2
#define TOP 3
#define BOTTOM 4
#define FRAMES_FEWEST 1
#define FRAMES_ALL 2
#define HORIZONTAL 1
#define VERTICAL 2

typedef struct _modmask {
	char *pattern;
	ucell mask;
	regex_t re;
} modmask;

typedef struct _keymap {
	char pattern[NOTE];
	char command[NOTE];
} keymap;

typedef struct _binding {
	char pattern[NOTE];
	char command[NOTE];
	ucell mod;
	KeyCode key;
} binding;

stack *bindings;

typedef struct _command {
	char *keys;
	char *pattern;
	char* (*func)(char*, regmatch_t*);
	ubyte flags;
	regex_t re;
} command;
hash *command_hash;
char *command_hints;

stack *unmanaged;
char *self;

enum { ms_border_focus, ms_border_unfocus, ms_border_dedicate_focus, ms_border_dedicate_unfocus,
ms_border_catchall_focus, ms_border_catchall_unfocus, ms_border_width, ms_frame_min_wh, ms_frame_resize,
ms_startup, ms_dmenu, ms_switch_window, ms_switch_group, ms_run_musca_command, ms_run_shell_command,
ms_notify, ms_stack_mouse_modifier, ms_focus_follow_mouse, ms_window_open_frame, ms_window_open_focus,
ms_command_buffer_size, ms_notify_buffer_size, ms_frame_display_hidden, ms_frame_split_focus,
ms_group_close_empty, ms_window_size_hints,
ms_last };

typedef struct _setting {
	char *name;
	ubyte type;
	union {
		char *s;
		ucell u;
		dcell d;
	};
	char *check;
	regex_t re;
} setting;
hash *setting_hash;

enum { mst_str, mst_ucell, mst_dcell };
ucell NumlockMask;

typedef struct {
	Window w;
	client *c;
	frame *f;
	ubyte manage;
	Status ok;
	XWindowAttributes attr;
	char name[NOTE];
	char class[NOTE];
	char role[NOTE];
	Atom type;
	ucell state;
	Bool input;
} winstate;

#define WINDOW_EVENT(ws) note("%s", (ws)->class)

ubyte mouse_grabbed;

typedef struct _alias {
	char *name;
	char *command;
} alias;
stack *aliases;

ubyte silent;

typedef struct _hook {
	regex_t re;
	char *pattern;
	char *command;
	struct _hook *next;
} hook;
hook *hooks;

enum { cmd_raise, cmd_use, cmd_kill, cmd_drop, cmd_add, cmd_shrink, cmd_last };

struct exec_marker {
	pid_t pid;
	group *group;
	frame *frame;
	time_t time;
};
#define EXEC_DELAY 10
#define EXEC_MARKERS 32
struct exec_marker exec_markers[EXEC_MARKERS];
ubyte exec_pointer;

char said[NOTE];
time_t spoke;

typedef struct {
	char class[NOTE];
	char group[NOTE];
} placement;
stack *placements;

bool sanity;
hash *arguments;
