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

#include "xwin.h"

void winlist_push(winlist *s, Window v)
{
	if (s->depth == s->limit)
	{
		s->limit += WINLIST;
		s->items = realloc(s->items, (s->limit+1) * sizeof(Window));
	}
	s->items[s->depth++] = v;
}
Window winlist_pop(winlist *s)
{
	return s->items[--(s->depth)];
}
Window winlist_top(winlist *s)
{
	return s->items[s->depth-1];
}
void winlist_del(winlist *s, ucell index)
{
	memmove(&(s->items[index]), &(s->items[index+1]), (s->depth - index) * sizeof(Window));
	s->depth--;
}
void winlist_discard(winlist *s, Window w)
{
	int i; for (i = 0; i < s->depth; i++) if (s->items[i] == w) winlist_del(s, i);
}
void *winlist_init(winlist *s)
{
	s->items = malloc(sizeof(Window)*(WINLIST+1));
	s->limit = WINLIST; s->depth = 0;
	return s;
}
void winlist_free(winlist *s)
{
	free(s->items);
}
// X tools
void XAtomSet(Display *d, ucell atom, Window w, Atom type, int format, void *data, int items)
{
	XChangeProperty(d, w, atoms[atom], type, format, PropModeReplace, (ubyte*)data, items);
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
	if (Success == XGetWindowProperty(d, w, atoms[atom], 0L, BLOCK*BLOCK, False, AnyPropertyType,
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
ucell XGetEWMHState(Display *d, Window win)
{
	ucell state = 0;
	ubyte *data; ucell len; int i, j;
	if (XAtomGet(d, NetWMState, win, &data, &len) && data && len)
	{
		Atom *states = (Atom*)data;
		for (i = 0; i < len; i++)
		{
			Atom s = states[i];
			for (j = NetWMState; j < NetWMStateDemandsAttention+1; j++)
				if (s == atoms[j]) state |= (1 << (j - NetWMState));
		}
	}
	free(data);
	return state;
}
void XSetEWMHState(Display *d, Window win, ucell state)
{
	int i, p = 0; Atom states[32];
	for (i = 0; i < 32; i++)
		if (state & (1 << i))
			states[p++] = atoms[NetWMState + i];
	XAtomSet(d, NetWMState, win, XA_ATOM, 32, &states, p);
}
Atom XGetEWMHType(Display *d, Window win)
{
	Atom type = None;
	ubyte *udata = NULL; ucell len;
	if (XAtomGet(d, NetWMWindowType, win, &udata, &len) && udata && len)
	{
		type = *(Atom*)udata;
		free(udata);
	}
	return type;
}
Bool XIsEWMHState(ucell state, ucell atom)
{
	return (state & (1 << (atom - NetWMState))) ? True:False;
}
int XWtf(Display *dpy, XErrorEvent *ee)
{
	if ((ee->error_code == BadWindow || ee->error_code == BadMatch)
		|| (ee->request_code == X_GrabButton        && ee->error_code == BadAccess  )
		|| (ee->request_code == X_GrabKey           && ee->error_code == BadAccess  ))
		return 0;
	fprintf(stderr, "fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}
ucell XGetColor(Display *d, const char *name)
{
	XColor color;
	Colormap map = DefaultColormap(d, DefaultScreen(d));
	return XAllocNamedColor(d, map, name, &color, &color) ? color.pixel: None;
}
Window XNewWindow(Display *d, Window parent, int x, int y, int w, int h, ucell bw, char *bc, char *bg, char *name, char *class)
{ JOT("0x%x 0x%x %d %d %d %d 0x%x (%s) (%s) (%s) (%s)", d, parent, x, y, w, h, bw, bc, bg, name, class);
	Window win = XCreateSimpleWindow(d, parent, x, y, w, h, bw,
		bc ? XGetColor(d, bc): None, bg ? XGetColor(d, bg): None);
	if (name && class)
	{
		XClassHint *hint = XAllocClassHint();
		hint->res_name = name;
		hint->res_class = class;
		XSetClassHint(d, win, hint);
		XFree(hint);
	}
	return win;
}
Window XGetFocus(Display *d)
{
	Window focus; int revert;
	XGetInputFocus(d, &focus, &revert);
	return (focus == None || focus == PointerRoot) ? DefaultRootWindow(d): focus;
}
void XFriendlyResize(Display *d, Window win, int x, int y, int w, int h, bool hint, bool center)
{
	XSizeHints hints;
	long userhints;
	int orig_w = w, orig_h = h;
	if (hint && XGetWMNormalHints(d, win, &hints, &userhints))
	{
		if (hints.flags & PMinSize)
		{
			w = MAX(w, hints.min_width); h = MAX(h, hints.min_height);
		}
		if (hints.flags & PMaxSize)
		{
			w = MIN(w, hints.max_width); h = MIN(h, hints.max_height);
		}
		if (hints.flags & PResizeInc && hints.flags & PBaseSize)
		{
			w -= hints.base_width; h -= hints.base_height;
			w -= w % hints.width_inc; h -= h % hints.height_inc;
			w += hints.base_width; h += hints.base_height;
		}
		if (hints.flags & PAspect)
		{
			double ratio = (double)w / h;
			double minr = (double)hints.min_aspect.x / hints.min_aspect.y;
			double maxr = (double)hints.max_aspect.x / hints.max_aspect.y;
				if (ratio < minr) h = (int)round(w / minr);
			else if (ratio > maxr) w = (int)round(h * maxr);
		}
	}
	if (center && (w < orig_w || h < orig_h))
	{
		x += (orig_w - w) / 2;
		y += (orig_h - h) / 2;
	}
	if (w > 0 && h > 0)
	{
		XWindowChanges wc;
		wc.x = x; wc.y = y; wc.width = w; wc.height = h;
		XConfigureWindow(d, win, CWX|CWY|CWWidth|CWHeight, &wc);
		XSync(d, False);
		// some windows prefer...
		XMoveResizeWindow(d, win, x, y, w, h);
	}
}
void XClose(Display *d, Window w)
{
	XEvent ke;
	ke.type = ClientMessage;
	ke.xclient.window = w;
	ke.xclient.message_type = atoms[WMProtocols];
	ke.xclient.format = 32;
	ke.xclient.data.l[0] = atoms[WMDelete];
	ke.xclient.data.l[1] = CurrentTime;
	XSendEvent(d, w, False, NoEventMask, &ke);
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
Window XIterateChildrenBack(Display *d, Window parent, XIterator cb, void *ptr)
{
	Window ret = None, d1, d2, *wins = NULL; ucell num, i = 0;
	if (XQueryTree(d, parent, &d1, &d2, &wins, &num))
	{
		for (i = num; i > 0 && cb(wins[i-1], ptr); i--);
		if (i > 0) ret = wins[i-1];
		if (wins) XFree(wins);
	}
	return ret;
}
void XRefresh(Display *d, Window w)
{
	XClearWindow(d, w);
}
void XCompressEvents(Display *d, XEvent *e)
{
	while (XCheckTypedWindowEvent(d, e->xany.window, e->type, e));
}
