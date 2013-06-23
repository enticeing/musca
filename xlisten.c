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

#define DEBUG 0
#define VERSION "dev"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BLOCK 1024

typedef char byte;
typedef unsigned char ubyte;
typedef short int word;
typedef unsigned short int uword;
typedef int cell;
typedef unsigned int ucell;
typedef long long int lcell;
typedef unsigned long long int ulcell;
typedef double dcell;
typedef unsigned char bool;

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

static int (*xerrorxlib)(Display *, XErrorEvent *);
typedef ubyte (*XIterator)(Window, void*);
Display *display;
Window root;
Screen *screen;
char desc[BLOCK];

#define WINDOW_MASK (PropertyChangeMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask | VisibilityChangeMask | ExposureMask)
#define ROOT_MASK (SubstructureNotifyMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask | ColormapChangeMask)

int XWtf(Display *dpy, XErrorEvent *ee)
{
	if ((ee->error_code == BadWindow)
		|| (ee->request_code == X_ChangeWindowAttributes && ee->error_code == BadMatch))
		return 0;
	fprintf(stderr, "fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee);
}
Window XIterateChildren(Display *d, Window parent, XIterator cb, void *ptr)
{
	Window ret = None, d1, d2, *wins = NULL; ucell num, i = 0;
	if (XQueryTree(d, parent, &d1, &d2, &wins, &num))
	{
		for (i = 0; i < num && cb(wins[i], ptr); i++);
		if (i < num) ret = wins[i];
		if (wins) XFree(wins);
	}
	return ret;
}
void XAtomSet(Display *d, ucell atom, Window w, Atom type, int format, void *data, int items)
{
	XChangeProperty(d, w, atom, type, format, PropModeReplace, (ubyte*)data, items);
}
void XAtomSetString(Display *d, ucell atom, Window w, char *s)
{
	XAtomSet(d, atom, w, XA_STRING, 8, s, strlen(s)+1);
}
ubyte XAtomGet(Display *d, ucell atom, Window w, ubyte **data, ucell *items)
{
	*data = NULL; *items = 0;
	Atom actual_type_return;
	int actual_format_return;
	unsigned long nitems_return;
	unsigned long bytes_after_return;
	ubyte *prop_return = NULL;
	if (Success == XGetWindowProperty(d, w, atom, 0L, BLOCK*BLOCK, False, AnyPropertyType,
		&actual_type_return, &actual_format_return, &nitems_return, &bytes_after_return, &prop_return)
		&& nitems_return && prop_return)
	{
		int bytes = (nitems_return * (actual_format_return / 8)) + 1;
		ubyte *blk = malloc(bytes); memmove(blk, prop_return, bytes);
		*data = blk; *items = nitems_return;
		return 1;
	}
	if (prop_return) XFree(prop_return);
	return 0;
}
char XAtomGetString(Display *d, ucell prop, Window win, char **pad, ucell *plen)
{
	ubyte r = XAtomGet(d, prop, win, (ubyte**)pad, plen);
	if (r && *pad && *plen) (*pad)[*plen] = '\0';
	return r;
}
void event_CreateNotify(XEvent *e)
{
	XCreateWindowEvent *cw = &e->xcreatewindow;
	XSelectInput(display, cw->window, WINDOW_MASK);
}
void event_PropertyNotify(XEvent *e)
{
	ubyte *data = NULL; ucell len = 0; int i;
	Window *wins = NULL;
	XPropertyEvent *pe = &e->xproperty;
	char *name = XGetAtomName(display, pe->atom);
	ucell dlen = snprintf(desc, BLOCK, "%s", name);
	if (pe->window == root)
	{
		if (pe->atom == XInternAtom(display, "_NET_ACTIVE_WINDOW", False)
			&& XAtomGet(display, pe->atom, root, &data, &len) && data && len)
		{
			dlen += snprintf(desc+dlen, BLOCK-dlen, " 0x%08x", (ucell)*((Window*)data));
		} else
		if ((pe->atom == XInternAtom(display, "_NET_CLIENT_LIST", False) ||
				pe->atom == XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False))
			&& XAtomGet(display, pe->atom, root, &data, &len) && data && len)
		{
			wins = (Window*)data;
			for (i = 0; i < len; i++)
				dlen += snprintf(desc+dlen, BLOCK-dlen, " 0x%08x", (ucell)wins[i]);
		}
	}
	if (name) XFree(name);
	if (data) XFree(data);
}
void event_ClientMessage(XEvent *e)
{
	XClientMessageEvent *cm = &e->xclient;
	char *name = XGetAtomName(display, cm->message_type);
	snprintf(desc, BLOCK, "%s", name);
	if (name) XFree(name);
}
void (*event[LASTEvent])(XEvent*) = {
	[CreateNotify] = event_CreateNotify,
	[PropertyNotify] = event_PropertyNotify,
	[ClientMessage] = event_ClientMessage,
};
char *event_names[LASTEvent] = {
	[Expose]           = "Expose",
	[EnterNotify]      = "EnterNotify",
	[LeaveNotify]      = "LeaveNotify",
	[FocusIn]          = "FocusIn",
	[FocusOut]         = "FocusOut",
	[KeymapNotify]     = "KeymapNotify",
	[VisibilityNotify] = "VisibilityNotify",
	[CreateNotify]     = "CreateNotify",
	[DestroyNotify]    = "DestroyNotify",
	[UnmapNotify]      = "UnmapNotify",
	[MapNotify]        = "MapNotify",
	[ReparentNotify]   = "ReparentNotify",
	[ConfigureNotify]  = "ConfigureNotify",
	[GravityNotify]    = "GravityNotify",
	[CirculateNotify]  = "CirculateNotify",
	[PropertyNotify]   = "PropertyNotify",
	[SelectionNotify]  = "SelectionNotify",
	[ColormapNotify]   = "ColormapNotify",
	[MappingNotify]    = "MappingNotify",
	[ClientMessage]    = "ClientMessage",
};
bool setup(Window w, void *ptr)
{
	XSelectInput(display, w, WINDOW_MASK);
	return 1;
}
int main(int argc, char *argv[])
{
	display = XOpenDisplay(0);
	xerrorxlib = XSetErrorHandler(XWtf);
	screen = XScreenOfDisplay(display, DefaultScreen(display));
	root = screen->root;
	XSelectInput(display, root, ROOT_MASK);
	XIterateChildren(display, root, setup, NULL);
	XEvent ev;
	for (;;)
	{
		*desc = '\0';
		XNextEvent(display, &ev);
		if (event[ev.type]) event[ev.type](&ev);
		printf("%s 0x%08x %s\n", event_names[ev.type], (ucell)ev.xany.window, desc);
		fflush(stdout);
	}
}
