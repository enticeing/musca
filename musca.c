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

#define _GNU_SOURCE
#define DEBUG 0
#define VERSION "0.9.24"

#include "tools.c"
#include "musca.h"
#include "musca_proto.h"
#include "config.h"

void say(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	char *pad = allocate(settings[ms_notify_buffer_size].u);
	vsnprintf(pad, settings[ms_notify_buffer_size].u, fmt, ap);
	va_end(ap);
	if (!(strcmp(said, pad) == 0 && spoke == time(0)))
	{
		snprintf(said, NOTE, "%s", pad); spoke = time(0);
		// silent means no 'notify'
		if (silent)
			printf("%s\n", pad);
		else
		if (fork() == 0)
		{
			FILE *p = popen(settings[ms_notify].s, "w");
			if (p)
			{
				fwrite(pad, 1, strlen(pad), p);
				pclose(p);
			}
			exit(EXIT_SUCCESS);
		}
	}
	free(pad);
}
void um(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	char *pad = allocate(settings[ms_notify_buffer_size].u);
	vsnprintf(pad, settings[ms_notify_buffer_size].u, fmt, ap);
	va_end(ap);
	// silent means no 'notify'
	if (silent)
		fprintf(stderr, "%s\n", pad);
	else
	if (fork() == 0)
	{
		FILE *p = popen(settings[ms_notify].s, "w");
		if (p)
		{
			fwrite(pad, 1, strlen(pad), p);
			pclose(p);
		}
		exit(EXIT_SUCCESS);
	}
	free(pad);
}
// x11 color
ucell get_color(head *h, const char *name)
{
	XColor color;
	Colormap map = DefaultColormap(display, h->id);
	return XAllocNamedColor(display, map, name, &color, &color) ? color.pixel: None;
}
int error_callback(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow)
		{ if (DEBUG) crap("BadWindow"); sanity = 1; return 0; }
	if (ee->request_code == X_SetInputFocus     && ee->error_code == BadMatch   )
		{ if (DEBUG) crap("X_SetInputFocus BadMatch"); return 0; }
	if (ee->request_code == X_ConfigureWindow   && ee->error_code == BadMatch   )
		{ if (DEBUG) crap("X_ConfigureWindow BadMatch"); return 0; }
	if (ee->request_code == X_GrabButton        && ee->error_code == BadAccess  )
		{ if (DEBUG) crap("X_GrabButton BadAccess"); return 0; }
	if (ee->request_code == X_GrabKey           && ee->error_code == BadAccess  )
		{ if (DEBUG) crap("X_GrabKey BadAccess"); return 0; }
	if (ee->request_code == X_CopyArea          && ee->error_code == BadDrawable)
		{ if (DEBUG) crap("X_CopyArea BadDrawable"); return 0; }
	fprintf(stderr, "fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}
// BINDINGS
ucell modifier_names_to_mask(char *names)
{
	int i; ucell mod = 0;
	modmask *m;
	FOR_ARRAY (m, modmasks, modmask, i)
		if (regexec(&m->re, names, 0, NULL, 0) == 0)
			mod |= m->mask;
	return mod;
}
binding* find_binding(ucell mod, KeyCode key)
{
	int i; binding *b = NULL;
	FOR_STACK (b, bindings, binding*, i)
		if (b->mod == mod && b->key == key)
			return b;
	return NULL;
}
ubyte process_binding(char *pattern, ucell *m, KeyCode *k)
{
	ucell mod = modifier_names_to_mask(pattern);
	char *l = pattern, *r = pattern;
	while ((r = strchr(l, '+'))) l = r+1;
	KeySym sym = XStringToKeysym(l);
	KeyCode code = XKeysymToKeycode(display, sym);
	if ((l > pattern && !mod) || sym == NoSymbol || !code)
		return 0;
	*m = mod; *k = code;
	return 1;
}
ubyte create_binding(char *pattern, char *command)
{
	int i; binding *b = NULL, *match = NULL;
	FOR_STACK (match, bindings, binding*, i)
	{
		if (strcmp(match->pattern, pattern) == 0)
		{
			b = match;
			break;
		}
	}
	ucell mod; KeyCode code;
	if (!process_binding(pattern, &mod, &code))
		return 0;
	if (!b) b = allocate(sizeof(binding));
	strcpy(b->pattern, pattern);
	strcpy(b->command, command);
	b->mod = mod; b->key = code;
	stack_push(bindings, b);
	return 1;
}
ubyte remove_binding(char *pattern)
{
	int i; binding *match = NULL;
	FOR_STACK (match, bindings, binding*, i)
	{
		if (strcmp(match->pattern, pattern) == 0)
		{
			stack_del(bindings, i);
			free(match);
			return 1;
		}
	}
	return 0;
}
void refresh_bindings()
{
	int i; binding *b = NULL;
	FOR_STACK (b, bindings, binding*, i)
		process_binding(b->pattern, &b->mod, &b->key);
}
// ALIASES
int find_alias(char *name)
{
	char *end = name; strscanthese(&end, " \t");
	alias *a; int i;
	FOR_STACK (a, aliases, alias*, i)
		if (strncmp(a->name, name, end-name) == 0
			&& strlen(a->name) == end-name)
			return i;
	return -1;
}
ubyte try_alias(char *name)
{
	int i = find_alias(name);
	if (i >= 0)
	{
		alias *a = aliases->items[i];
		char *tmp = allocate(strlen(a->command)+strlen(name)+1);
		sprintf(tmp, "%s %s", a->command, name+strlen(a->name));
		free(musca_command(tmp));
		free(tmp); return 1;
	}
	return 0;
}
ubyte create_alias(char *name, char *command)
{
	alias *a = NULL;
	int i = find_alias(name);
	if (i >= 0)
	{
		a = aliases->items[i];
		free(a->name); free(a->command);
		a->name = name; a->command = command;
		return 2;
	}
	a = allocate(sizeof(alias));
	a->name = name;
	a->command = command;
	stack_push(aliases, a);
	return 1;
}
ubyte try_hook(char *cmd)
{
	hook *h; int i;
	FOR_LIST (h, hooks, i)
	{
		if (regexec(&h->re, cmd, 0, NULL, 0) == 0)
		{
			free(musca_command(h->command));
			return 1;
		}
	}
	return 0;
}
// GENERAL WINDOW STUFF

void atom_set(ucell atom, Window w, Atom type, int format, void *data, int items)
{
	XChangeProperty(display, w, atoms[atom], type, format, PropModeReplace, (ubyte*)data, items);
}
void atom_set_string(ucell atom, Window w, char *s)
{
	atom_set(atom, w, XA_STRING, 8, s, strlen(s)+1);
}
ubyte atom_get_string(ucell prop, Window win, char **pad, ucell *plen)
{
	XTextProperty tp; int n; char** cl; *pad = NULL; *plen = 0;
	if (XGetTextProperty(display, win, &tp, atoms[prop]) != 0)
	{
		if (tp.encoding == XA_STRING)
		{
			*plen = strlen((char*)tp.value);
			*pad = allocate(*plen+1);
			strcpy(*pad, (char*)tp.value);
		}
		else
		if (Success <= Xutf8TextPropertyToTextList(display, &tp, &cl, &n) && cl && *cl && n > 0)
		{
			int l, i; *plen = 0;
			for (i = 0; i < n; i++)
				*plen += strlen(cl[i])+1;
			*pad = allocate(*plen+1);
			for (i = 0, l = 0; i < n; i++)
				l+= snprintf(*pad+l, *plen-l, "%s\n", cl[i]);
			strrtrim(*pad);
			XFreeStringList(cl);
		}
		XFree(tp.value);
	}
	return *pad ? 1:0;
}
ubyte atom_get(ucell atom, Window w, ubyte **data, ucell *items)
{
	*data = NULL; *items = 0;
	Atom actual_type_return;
	int actual_format_return;
	unsigned long nitems_return;
	unsigned long bytes_after_return;
	unsigned char *prop_return = NULL;
	if (Success == XGetWindowProperty(display, w, atoms[atom], 0L, BLOCK, False, AnyPropertyType,
		&actual_type_return, &actual_format_return, &nitems_return, &bytes_after_return, &prop_return)
		&& nitems_return && prop_return)
	{
		int bytes = (nitems_return * (actual_format_return / 8));
		ubyte *blk = allocate(bytes);
		memmove(blk, prop_return, bytes);
		*data = blk;
		*items = nitems_return;
		return 1;
	}
	if (prop_return) XFree(prop_return);
	return 0;
}
void window_name(Window win, char *pad)
{
	strcpy(pad, "unknown");
	char *data; ucell len;
	if (atom_get_string(WMName, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
	if (atom_get_string(NetWMName, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
}
void window_class(Window win, char *pad)
{
	strcpy(pad, "unknown");
	char *data; ucell len;
	if (atom_get_string(WMClass, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
	char *class = strchr(pad, '\n');
	if (class)
	{
		class++;
		memmove(pad, class, strlen(class)+1);
	}
}
void window_role(Window win, char *pad)
{
	strcpy(pad, "unknown");
	char *data; ucell len;
	if (atom_get_string(WMRole, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
}
Atom window_type(Window win)
{
	Atom type = None;
	ubyte *data; ucell len;
	if (atom_get(NetWMWindowType, win, &data, &len) && data && len)
		type = *(Atom*)data;
	free(data);
	return type;
}
ubyte is_netwmstate(ucell state, ucell atom)
{
	return (state & (1 << (atom - NetWMState))) ? 1:0;
}
// grab _NET_WM_STATE from a window and create a bitmap of NetWMState* settings the window uses.
ucell window_state(Window win)
{
	ucell state = 0;
	ubyte *data; ucell len; int i, j;
	if (atom_get(NetWMState, win, &data, &len) && data && len)
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
ubyte is_unmanaged_class(char *name)
{
	if (name && *name)
	{
		char *item; int i;
		FOR_STACK (item, unmanaged, char*, i)
			if (strcmp(item, name) == 0) return 1;
	}
	return 0;
}
ubyte is_unmanaged_window(Window win)
{
	char name[NOTE];
	window_class(win, name);
	if (strcmp(name, MUSCA_CLASS) == 0) return 1;
	return is_unmanaged_class(name);
}
ubyte window_on_screen(head *hd, Window win, XWindowAttributes *attr, int *x, int *y, int *w, int *h)
{
	XWindowAttributes _attr;
	if (!attr)
	{
		attr = &_attr;
		if (!XGetWindowAttributes(display, win, attr))
			return 1;
	}
	int lx = attr->x, ly = attr->y, lw = attr->width, lh = attr->height;
	if (lx + lw > hd->screen->width)  lx -= (lx + lw - hd->screen->width);
	if (ly + lh > hd->screen->height) ly -= (ly + lh - hd->screen->height);
	ubyte on_scr = (lx == attr->x && ly == attr->y) ? 1: 0;
	*x = lx; *y = ly; *w = lw; *h = lh;
	return on_scr;
}
Window window_create(Window parent, int x, int y, int w, int h, ucell bw, char *bc, char *bg, char *name, char *class)
{
	Window win = XCreateSimpleWindow(display, parent, x, y, w, h, bw,
		bc ? get_color(heads, bc): None, bg ? get_color(heads, bg): None);
	if (name)
	{
		XClassHint *hint = XAllocClassHint(); hint->res_name = name; hint->res_class = class;
		XSetClassHint(display, win, hint);
		XFree(hint);
	}
	return win;
}
void window_stack(head *h, Window *list, int n)
{
	Window w; int i, wc = 0;
	Window *wins = allocate(sizeof(Window) * (h->above->depth + h->fullscreen->depth + 1));
	FOR_STACK (w, h->fullscreen, Window, i) wins[wc++] = w;
	FOR_STACK (w, h->above, Window, i) wins[wc++] = w;
	wins[wc++] = list[0];
	XRaiseWindow(display, wins[0]);
	if (wc > 1) XRestackWindows(display, wins, wc);
	if (n > 1) XRestackWindows(display, list, n);
	free(wins);
}
ubyte sanity_window(Window *tree, ucell n, Window w)
{
	int i;
	for (i = 0; i < n; i++)
		if (tree[i] == w) return 1;
	return 0;
}
int sanity_stack(stack *s, Window *tree, ucell num)
{
	int found = 0;
	if (s->depth)
	{
		int *indexes = allocate(sizeof(int) * s->depth);
		int count = 0, i; Window *w;
		FOR_STACK (w, s, Window*, i)
		{
			note("%d", i);
			if (!sanity_window(tree, num, *w))
				indexes[count++] = i;
		}
		found = count;
		while (count > 0) stack_del(s, indexes[--count]);
		free(indexes);
	}
	return found;
}
void sanity_head(head *h)
{
	Window d1, d2, *tree = NULL; ucell num;
	if (XQueryTree(display, h->screen->root, &d1, &d2, &tree, &num))
	{
		int found = 0;
		stack *destroy = stack_create();
		group *g; client *c; int i, j;
		FOR_RING (NEXT, g, h->groups, i)
		{
			FOR_RING (NEXT, c, g->clients, j)
			{
				if (!sanity_window(tree, num, c->win))
				{
					note("%s client %u %s", g->name, (ucell)c, c->name);
					stack_push(destroy, c);
				}
			}
		}
		FOR_STACK (c, destroy, client*, i) client_remove(c);
		stack_free(destroy);
		found = sanity_stack(h->above, tree, num);
		note("above %d", found);
		found = sanity_stack(h->below, tree, num);
		note("below %d", found);
		found = sanity_stack(h->fullscreen, tree, num);
		note("fullscreen %d", found);
	}
	if (tree) XFree(tree);
}
void sanity_heads()
{
	head *h; int i;
	FOR_RING (NEXT, h, heads, i) sanity_head(h);
}
void window_discard_references(Window w)
{
	head *h; group *g; int i, j;
	FOR_RING (NEXT, h, heads, i)
	{
		stack_discard(h->above, (void*)w);
		stack_discard(h->below, (void*)w);
		stack_discard(h->fullscreen, (void*)w);
		FOR_RING (NEXT, g, h->groups, j)
			stack_discard(g->stacked, (void*)w);
	}
}
// SEARCHES

head* head_by_screen(Screen *s)
{
	head *f; int i;
	FOR_RING (NEXT, f, heads, i)
		if (f->screen == s) return f;
	return NULL;
}
head* head_by_root(Window win)
{
	head *h; int i;
	FOR_RING (NEXT, h, heads, i)
		if (h->screen->root == win) return h;
	return NULL;
}
client* client_by_window(Window w)
{
	head *h; group *g; client *c; int i, j, k;
	FOR_RING (NEXT, h, heads, i)
		FOR_RING (NEXT, g, h->groups, j)
			FOR_RING (NEXT, c, g->clients, k)
				if (c->win == w) return c;
	return NULL;
}
client* client_by_resize_window(Window w)
{
	head *h; group *g; client *c; int i, j, k;
	FOR_RING (NEXT, h, heads, i)
		FOR_RING (NEXT, g, h->groups, j)
			FOR_RING (NEXT, c, g->clients, k)
				if (c->rl == w) return c;
	return NULL;
}
client* client_hidden(frame *f, ubyte direction, ubyte local, ubyte prefer)
{
	client *c, *m = NULL; int i; group *t = f->group;
	if (prefer == RECENT)
	{
		FOR_STACK (c, t->stacked, client*, i)
		{
			if (c->flags & CF_SHRUNK) continue;
			if (local && c->frame && c->frame == f && f->cli != c) m = c;
			if (!local && (!c->frame || (c->frame->cli != c && !c->kids))) m = c;
		}
		if (m) return m;
	}
	FOR_RING (direction, c, t->clients, i)
	{
		if (c->flags & CF_SHRUNK) continue;
		if (local && c->frame && c->frame == f && f->cli != c) return c;
		if (!local && (!c->frame || (c->frame->cli != c && !c->kids))) return c;
	}
	return m;
}
frame* is_frame_background(Window win)
{
	int i, j, k;
	head *h; group *g; frame *f;
	FOR_RING (NEXT, h, heads, i)
		FOR_RING (NEXT, g, h->groups, j)
			FOR_RING (NEXT, f, g->frames, k)
				if (f->win == win) return f;
	return NULL;
}
ubyte is_valid_client(client *c)
{
	int i, j, k;
	head *h; group *g; client *d;
	FOR_RING (NEXT, h, heads, i)
		FOR_RING (NEXT, g, h->groups, j)
			FOR_RING (NEXT, d, g->clients, k)
				if (d == c) return 1;
	return 0;
}
ubyte is_valid_frame(frame *f)
{
	int i, j, k;
	head *h; group *g; frame *d;
	FOR_RING (NEXT, h, heads, i)
		FOR_RING (NEXT, g, h->groups, j)
			FOR_RING (NEXT, d, g->frames, k)
				if (d == f) return 1;
	return 0;
}
ubyte is_valid_group(group *c)
{
	int i, j;
	head *h; group *g;
	FOR_RING (NEXT, h, heads, i)
		FOR_RING (NEXT, g, h->groups, j)
			if (g == c) return 1;
	return 0;
}

group* group_first(head *h)
{
	group *g, *l = h->groups; int i;
	FOR_RING (NEXT, g, h->groups, i)
		if (g->id < l->id) l = g;
	return l;
}
group* group_last(head *h)
{
	group *g, *l = h->groups; int i;
	FOR_RING (NEXT, g, h->groups, i)
		if (g->id > l->id) l = g;
	return l;
}
frame* frame_first(group *g)
{
	frame *f, *l = g->frames; int i;
	FOR_RING (NEXT, f, g->frames, i)
		if (f->id < l->id) l = f;
	return l;
}
frame* frame_last(group *g)
{
	frame *f, *l = g->frames; int i;
	FOR_RING (NEXT, f, g->frames, i)
		if (f->id > l->id) l = f;
	return l;
}
client* client_first(group *g)
{
	client *c, *l = g->clients; int i;
	FOR_RING (NEXT, c, g->clients, i)
		if (c->id < l->id) l = c;
	return l;
}
client* client_last(group *g)
{
	client *c, *l = g->clients; int i;
	FOR_RING (NEXT, c, g->clients, i)
		if (c->id > l->id) l = c;
	return l;
}
client* client_by_id(group *g, int id)
{
	client *c; int i;
	FOR_RING (NEXT, c, g->clients, i)
		if (c->id == id) return c;
	return NULL;
}
client* client_by_name(group *g, char *name)
{
	client *c; int i;
	FOR_RING (NEXT, c, g->clients, i)
		if (strcmp(c->name, name) == 0) return c;
	return NULL;
}
client* client_from_string(group *g, char *s, client *def)
{
	if (!s) return def;
	client *c = isdigit(*s) ? client_by_id(g, strtol(s, NULL, 10)): client_by_name(g, s);
	return c ? c: def;
}

// CLIENTS
ubyte ewmh_client_visible(client *c)
{
	return is_netwmstate(c->netwmstate, NetWMStateSkipTaskbar)
		|| is_netwmstate(c->netwmstate, NetWMStateSkipPager) ? 0: 1;
}
void ewmh_clients()
{
	group *g = heads->groups, *o;
	client *c, *f; int i, j, wc = 0, ws = 0;
	FOR_RING (NEXT, o, g->head->groups, i) FOR_RING (NEXT, c, o->clients, j) wc++;
	Window *wins_all = allocate(sizeof(Window*)*wc);
	Window *wins_stack = allocate(sizeof(Window*)*wc);
	Window active = g->clients && ewmh_client_visible(g->clients) ? g->clients->win: None;
	wc = 0; FOR_RING (NEXT, o, g->head->groups, i)
	{
		f = client_first(o);
		FOR_RING (PREV, c, f, j)
		{
			if (ewmh_client_visible(c))
			{
				atom_set(NetWMDesktop, c->win, XA_CARDINAL, 32, &o->id, 1);
				wins_all[wc++] = c->win;
			}
		}
	}
	ws = 0; FOR_STACK (o, g->head->stacked, group*, i)
	{
		FOR_STACK (c, o->stacked, client*, j)
		{
			if (ewmh_client_visible(c))
			{
				atom_set(NetWMDesktop, c->win, XA_CARDINAL, 32, &o->id, 1);
				wins_stack[ws++] = c->win;
			}
		}
	}
	Window r = g->head->screen->root;
	atom_set(NetClientList,         r, XA_WINDOW, 32, wins_all,   wc);
	atom_set(NetClientListStacking, r, XA_WINDOW, 32, wins_stack, ws);
	atom_set(NetActiveWindow,       r, XA_WINDOW, 32, &active, 1);
	free(wins_all); free(wins_stack);
	XFlush(display);
}
void ewmh_groups()
{
	autostr s; str_create(&s);
	group *first = heads->groups;
	group *g; ucell i;
	FOR_RING (NEXT, g, heads->groups, i)
		if (g->id < first->id) first = g;
	FOR_RING (NEXT, g, first, i)
	{
		str_print(&s, strlen(g->name)+1, "%s", g->name);
		s.len++;
	}
	int xy[2];
	xy[0] = g->head->screen->width - g->l - g->r;
	xy[1] = g->head->screen->height - g->t - g->b;
	Window r = g->head->screen->root;
	atom_set(NetNumberOfDesktops, r, XA_CARDINAL, 32, &i, 1);
	atom_set(NetDesktopNames,     r, atoms[UTF8String], 8, s.pad, s.len);
	atom_set(NetCurrentDesktop,   r, XA_CARDINAL, 32, &heads->groups->id, 1);
	atom_set(NetDesktopGeometry,  r, XA_CARDINAL, 32, xy, 2);
	str_free(&s);
}
void client_configure(client *c, XConfigureRequestEvent *cr)
{
	XWindowAttributes attr;
	Window trans = None;
	XSizeHints hints;
	long userhints;
	int x, y, w, h, pw, ph, b;
	frame *f = c->frame;
	group *g = c->group;
	Window win = c->win;
	char debug[BLOCK]; int dlen = 0;
	ucell bw = MAX(settings[ms_border_width].u, 0);
	int fbw = f->flags & FF_HIDEBORDER ? 0: bw;

	if (g->flags & GF_STACKING)
	{
		x = g->l; y = g->t;
		pw = g->head->screen->width - g->l - g->r;
		ph = g->head->screen->height - g->t - g->b;
	} else
	{
		x = f->x+fbw; y = f->y+fbw;
		pw = f->w-fbw-fbw; ph = f->h-fbw-fbw;
	}
	w = pw; h = ph;
	window_name(c->win, c->name);
	window_class(c->win, c->class);
	window_role(c->win, c->role);
	dlen += sprintf(debug+dlen, "%s ", c->name);

	if (!XGetTransientForHint(display, win, &trans) && trans == None && g->flags & GF_TILING)
	{
		// normal windows go full frame
		dlen += sprintf(debug+dlen, "normal ");
		w = pw; h = ph; b = 0;
		c->flags |= CF_NORMAL;
	} else
	{
		b = bw;
		// stacking and transient windows stay their preferred size
		if (XGetWindowAttributes(display, win, &attr))
		{
			x = attr.x; y = attr.y; w = attr.width; h = attr.height;
		}
		if (cr)
		{
			if (cr->value_mask & CWX) x = cr->x;
			if (cr->value_mask & CWY) y = cr->y;
			if (cr->value_mask & CWWidth)  w = cr->width;
			if (cr->value_mask & CWHeight) h = cr->height;
		}
		dlen += sprintf(debug+dlen, "floating %d %d %d %d ", x, y, w, h);
		c->flags &= ~CF_NORMAL;
	}
	if (c->flags & CF_HINTS && XGetWMNormalHints(display, win, &hints, &userhints))
	{
		dlen += sprintf(debug+dlen, "hints ");
		if (hints.flags & PMinSize)
		{
			dlen += sprintf(debug+dlen, "PMinSize %d %d ", hints.min_width, hints.min_height);
			w = MAX(w, hints.min_width); h = MAX(h, hints.min_height);
		}
		if (hints.flags & PMaxSize)
		{
			dlen += sprintf(debug+dlen, "PMaxSize %d %d ", hints.max_width, hints.max_height);
			w = MIN(w, hints.max_width); h = MIN(h, hints.max_height);
		}
		if (hints.flags & PResizeInc && hints.flags & PBaseSize)
		{
			w -= hints.base_width; h -= hints.base_height;
			w -= w % hints.width_inc; h -= h % hints.height_inc;
			w += hints.base_width; h += hints.base_height;
			dlen += sprintf(debug+dlen, "PResizeInc %d %d %d %d ", hints.width_inc, hints.height_inc, w, h);
		}
		if (hints.flags & PAspect)
		{
			dcell ratio = (dcell)w / h;
			dcell minr = (dcell)hints.min_aspect.x / hints.min_aspect.y;
			dcell maxr = (dcell)hints.max_aspect.x / hints.max_aspect.y;
			dlen += sprintf(debug+dlen, "PAspect %2.2f %2.2f %2.2f ", ratio, minr, maxr);
				if (ratio < minr) h = (int)(w / minr);
			else if (ratio > maxr) w = (int)(h * maxr);
		}
		w = MIN(w, pw); h = MIN(h, ph);
		if (trans == None)
		{
			w = MAX(settings[ms_frame_min_wh].u-bw-bw, w);
			h = MAX(settings[ms_frame_min_wh].u-bw-bw, h);
		}
	}
	// center fixed size window whe tiling
	if (g->flags & GF_TILING && (w < pw || h < ph))
	{
		x = f->x+fbw + ((pw - w) / 2);
		y = f->y+fbw + ((ph - h) / 2);
	}
	dlen += sprintf(debug+dlen, "Co-ords %d %d %d %d ", x, y, w, h);
	XWindowChanges wc; wc.x = x; wc.y = y; wc.width = w; wc.height = h; wc.border_width = b;
	XConfigureWindow(display, win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	c->x = x; c->y = y; c->w = w; c->h = h;
	note("%s", debug);
}
void client_and_kids(client *c, stack *clients, client *except)
{
	if (c != except)
	{
		int i; client *k;
		FOR_RING (NEXT, k, c, i)
		{
			if (k != except && k->parent == c && !stack_find(clients, k))
				client_and_kids(k, clients, except);
		}
		stack_push(clients, c);
	}
}
void client_display(client *c, frame *target)
{
	ubyte redraw = ((target && target != c->frame) ||
		(c->frame && (c->frame->cli != c || c->frame->state != c->state))) ? 1:0;
	note("%s", redraw ? "redraw": "skip");
	if (target) c->frame = target;
	if (c->frame) c->frame->cli = c;
	c->state = c->frame->state;
	if (redraw)
	{
		client_configure(c, NULL);
		stack *family = stack_create();
		client *ancestor = c, *relative; int i;
		while (ancestor->parent && is_valid_client(ancestor->parent))
			ancestor = ancestor->parent;
		client_and_kids(c, family, NULL);
		client_and_kids(ancestor, family, c);
		Window *wins = allocate(sizeof(Window) * (family->depth + 1));
		FOR_STACK (relative, family, client*, i)
		{
			client_show(relative);
			if (relative->frame != c->frame)
			{
				relative->frame = c->frame;
				relative->state = c->state - 1;
			}
			if (relative->state != c->state)
			{
				client_configure(relative, NULL);
				relative->state = c->state;
			}
			wins[i] = relative->win;
		}
		if (c->frame && c->group->flags & GF_TILING)
			wins[i] = c->frame->win;
		window_stack(c->group->head, wins, family->depth+1);
		if (c->flags & CF_NORMAL)
		{
			XSetWindowBorderWidth(display, c->win, 1);
			XSetWindowBorderWidth(display, c->win, 0);
		}
		free(wins); stack_free(family);
	}
}
void client_refresh(client *c)
{
	c->state = 0;
	client_display(c, NULL);
}
void client_border(client *c, char *colour)
{
	ucell xcolour = get_color(c->group->head, colour);
	client *k; int i;
	FOR_RING (NEXT, k, c->group->clients, i)
		if (k->parent == c)
			XSetWindowBorder(display, k->win, xcolour);
	XSetWindowBorder(display, c->win, xcolour);
}
void client_unfocus(client *c)
{
	client_border(c, settings[ms_border_unfocus].s);
}
void client_focus(client *c, frame *target)
{
	if (c->group->clients && c->group->clients != c)
		client_unfocus(c->group->clients);
	c->group->clients = c;
	client_display(c, target);
	client_border(c, settings[ms_border_focus].s);
	XSetInputFocus(display, c->input ? c->win: c->frame->win, RevertToPointerRoot, CurrentTime);
	ewmh_clients();
	stack_discard(c->group->stacked, c);
	stack_push(c->group->stacked, c);
}
void client_raise(client *c)
{
	c->flags &= ~CF_SHRUNK;
	frame_focus_client(c->frame && c->frame->cli == c ?
		c->frame: frame_available(c->group->frames), c);
}
void client_show(client *c)
{
	c->flags &= ~CF_HIDDEN;
	XMapWindow(display, c->win);
	c->unmaps = 0;
}
void client_hide(client *c)
{
	c->flags |= CF_HIDDEN;
	XUnmapWindow(display, c->win);
	c->unmaps++; c->state = 0;
}
void client_shrink(client *c)
{
	c->flags |= CF_SHRUNK;
	XUnmapWindow(display, c->win);
	c->unmaps++; c->state = 0;
}
void client_remove(client *c)
{
	frame *f = c->frame;
	client *p = c->parent;
	client_destroy(c);
	if (!f->cli && f->group == f->group->head->groups)
	{
		ubyte focus = heads->groups->frames == f ? 1: 0;
		if (p && is_valid_client(p) && (!p->frame || p->frame->cli != p))
		{
			if (focus) client_focus(p, f);
			else client_display(p, f);
		} else
		{
			if (focus) frame_auto_focus_hidden(f, NEXT);
			else frame_auto_display_hidden(f, NEXT);
		}
	}
	ewmh_clients();
}
void client_kill(client *c)
{
	if (c->flags & CF_KILLED)
	{
		// we already sent a close event and apparently got ignored, so get nasty
		XKillClient(display, c->win);
		client_remove(c);
	} else
	{
		// be polite the by dfault
		XEvent ke;
		ke.type = ClientMessage;
		ke.xclient.window = c->win;
		ke.xclient.message_type = atoms[WMProtocols];
		ke.xclient.format = 32;
		ke.xclient.data.l[0] = atoms[WMDelete];
		ke.xclient.data.l[1] = CurrentTime;
		XSendEvent(display, c->win, False, NoEventMask, &ke);
		c->flags |= CF_KILLED;
	}
}
void client_push(group *t, client *c)
{
	c->group = t;
	client *last = client_last(t);
	c->id = last ? last->id+1 : 0;
	if (t->clients)
	{
		client *tmp = last->next;
		last->next = c; tmp->prev = c;
		c->prev = last; c->next = tmp;
	} else
		t->clients = c->next = c->prev = c;
	if (!c->frame) c->frame = t->frames;
	c->state = 0;
}
void client_pop(client *c)
{
	group *t = c->group;
	if (t->clients == t->clients->next)
		t->clients = NULL;
	else
	{
		c->next->prev = c->prev;
		c->prev->next = c->next;
		if (t->clients == c)
			t->clients = c->next;
	}
	if (c->frame && c->frame->cli == c)
		c->frame->cli = NULL;
	c->frame = NULL;

	client *n; int i;
	FOR_RING (NEXT, n, t->clients, i)
		if (n->id > c->id) n->id--;
	stack_discard(c->group->stacked, c);
}
client* client_create(group *t, frame *f, Window win)
{
	client *c = allocate(sizeof(client));
	c->frame = f; c->win = win; c->flags = CF_INITIAL;
	if (settings[ms_window_size_hints].u) c->flags |= CF_HINTS;
	c->group = NULL; c->next = NULL; c->prev = NULL;
	c->kids = 0; c->parent = NULL; c->netwmstate = 0;
	c->name[0] = '\0'; c->class[0] = '\0'; c->role[0] = '\0';
	c->x = 0; c->y = 0; c->w = 0; c->h = 0;
	c->fx = 0; c->fy = 0; c->fw = 0; c->fh = 0;
	c->rl = 0; c->rr = 0; c->rt = 0; c->rb = 0;
	c->unmaps = 0; c->state = f->state - 1; c->input = True;
	if (t) client_push(t, c);
	return c;
}
void client_destroy(client *c)
{
	client_pop(c);
	if (c->parent && is_valid_client(c->parent))
		c->parent->kids--;
	stack_discard(c->group->stacked, c);
	free(c);
}
void client_regroup(group *g, client *c)
{
	if (c->kids)
	{
		stack *kids = stack_create();
		client *k; int i;
		FOR_RING (NEXT, k, c->group->clients, i)
			if (k->parent == c) stack_push(kids, k);
		FOR_STACK (k, kids, client*, i)
			client_regroup(g, k);
		stack_free(kids);
	}
	client_pop(c); client_hide(c); client_push(g, c);
}
// FRAMES
void frame_push(group *t, frame *f)
{
	f->group = t;
	frame *last = frame_last(t);
	f->id = last ? last->id+1 : 0;
	if (t->frames)
	{
		frame *tmp = last->next;
		last->next = f; tmp->prev = f;
		f->prev = last; f->next = tmp;
	} else
		t->frames = f->next = f->prev = f;
}
void frame_pop(frame *f)
{
	group *t = f->group;
	if (t->frames == t->frames->next)
		t->frames = NULL;
	else
	{
		f->next->prev = f->prev;
		f->prev->next = f->next;
		if (t->frames == f)
			t->frames = f->next;
	}
	client *c; frame *n; int i;
	FOR_RING (NEXT, n, t->frames, i)
		if (n->id > f->id) n->id--;
	FOR_RING (NEXT, c, t->clients, i)
		if (c->frame && c->frame == f)
			c->frame = t->frames;
}
ucell frame_border_focus(frame *f)
{
	char *color = NULL;
	if (f->flags & FF_HIDEBORDER) return None;
	if (f->group->flags & GF_TILING)
	{
		color = settings[ms_border_focus].s;
		if (f->flags & FF_DEDICATE) color = settings[ms_border_dedicate_focus].s;
		if (f->flags & FF_CATCHALL) color = settings[ms_border_catchall_focus].s;
	}
	return color ? get_color(f->group->head, color): None;
}
ucell frame_border_unfocus(frame *f)
{
	char *color = NULL;
	if (f->flags & FF_HIDEBORDER) return None;
	if (f->group->flags & GF_TILING)
	{
		color = settings[ms_border_unfocus].s;
		if (f->flags & FF_DEDICATE) color = settings[ms_border_dedicate_unfocus].s;
		if (f->flags & FF_CATCHALL) color = settings[ms_border_catchall_unfocus].s;
	}
	return color ? get_color(f->group->head, color): None;
}
frame* frame_create(group *t, int x, int y, int w, int h)
{
	frame *f = allocate(sizeof(frame));
	f->x = x; f->y = y; f->w = w; f->h = h; f->flags = 0;
	f->sx = 0; f->sy = 0; f->sw = 0; f->sh = 0;
	f->group = NULL; f->cli = NULL; f->next = NULL; f->prev = NULL;
	f->state = 1;
	if (t) frame_push(t, f);
	// frames have a background window, just for borders so far
	ucell bw = MAX(settings[ms_border_width].u, 0);
	f->win = window_create(t->head->screen->root, x, y, w-bw-bw, h-bw-bw, bw,
		settings[ms_border_unfocus].s, NULL, MUSCA_CLASS, MUSCA_CLASS);
	// pseudo-transparent to the root window
	XSetWindowBackgroundPixmap(display, f->win, ParentRelative);
	XSelectInput(display, f->win, PropertyChangeMask | EnterWindowMask | LeaveWindowMask);
	return f;
}
void frame_destroy(frame *f)
{
	frame_pop(f);
	XDestroyWindow(display, f->win);
	free(f);
}
frame* frame_available(frame *f)
{
	frame *a; int i;
	// is there a catchall frame
	FOR_RING (NEXT, a, f, i)
		if (a->flags & FF_CATCHALL) return a;
	// if user prefers empty frame, find one
	if (strcasecmp(settings[ms_window_open_frame].s, "empty") == 0)
		FOR_RING (NEXT, a, f, i)
			if (!(a->flags & FF_DEDICATE) && !a->cli) return a;
	// if current frame is dedicated, find another
	if (f->flags & FF_DEDICATE)
		FOR_RING (NEXT, a, f, i)
			if (!(a->flags & FF_DEDICATE)) return a;
	// at this stage just take the current frame regardless
	return f;
}
void frame_display_hidden(frame *f, ubyte direction, ubyte local, ubyte prefer)
{
	client *c = client_hidden(f, direction, local, prefer);
	if (c) client_display(c, f);
}
void frame_focus_hidden(frame *f, ubyte direction, ubyte local, ubyte prefer)
{
	client *c = client_hidden(f, direction, local, prefer);
	if (c) client_focus(c, f);
	if (!f->cli)
	{
		XSetInputFocus(display, f->win, RevertToPointerRoot, CurrentTime);
		window_stack(f->group->head, &f->win, 1);
	}
}
void frame_auto_display_hidden(frame *f, ubyte direction)
{
	if (f->group->flags & GF_STACKING)
	{
		frame_display_hidden(f, direction, 0, RECENT);
		return;
	}
	switch (settings[ms_frame_display_hidden].u)
	{
		case 1:
			frame_display_hidden(f, direction, 0, RECENT);
			break;
		case 2:
			frame_display_hidden(f, direction, 1, RECENT);
			break;
	}
}
void frame_auto_focus_hidden(frame *f, ubyte direction)
{
	if (f->group->flags & GF_STACKING)
	{
		frame_focus_hidden(f, direction, 0, RECENT);
		return;
	}
	switch (settings[ms_frame_display_hidden].u)
	{
		case 1:
			frame_focus_hidden(f, direction, 0, RECENT);
			break;
		case 2:
			frame_focus_hidden(f, direction, 1, RECENT);
			break;
	}
}
void frames_display_hidden(group *t)
{
	frame *f; int i;
	FOR_RING (NEXT, f, t->frames, i)
		if (!f->cli || (f->cli && f->cli->frame != f))
			frame_auto_display_hidden(f, NEXT);
}
// any changes to frames setting do not take effect until this is called
// bundle multiple changes before a single call to prevent flicker
void frame_update(frame *f)
{
	ucell bw = MAX(settings[ms_border_width].u, 0);
	XSetWindowBackgroundPixmap(display, f->win, ParentRelative);
	if (f->x != f->sx || f->y != f->sy || f->w != f->sw || f->h != f->sh)
	{
		f->sx = f->x; f->sy = f->y; f->sw = f->w; f->sh = f->h;
		f->state = (f->state + 1) % 1000000;
	}
	if (f->flags & FF_HIDEBORDER)
	{
		XSetWindowBorderWidth(display, f->win, 0);
		XMoveResizeWindow(display, f->win, f->x, f->y, f->w, f->h);
	} else
	{
		XSetWindowBorderWidth(display, f->win, bw);
		XMoveResizeWindow(display, f->win, f->x, f->y, f->w-bw-bw, f->h-bw-bw);
	}
	if (f->group->flags & GF_TILING)
	{
		XMapWindow(display, f->win);
		if (f->cli && f->cli->frame == f)
			client_display(f->cli, f);
		else	frame_auto_display_hidden(f, NEXT);
	}
}
void frame_unfocus(frame *f)
{
	if (f->cli) client_unfocus(f->cli);
	XSetWindowBorder(display, f->win, frame_border_unfocus(f));
}
void frame_focus(frame *f)
{
	if (f->group->head != heads)   head_focus(f->group->head);
	if (f->group != heads->groups) group_focus(f->group);
	head *h = f->group->head;
	if (f != h->groups->frames)
		frame_unfocus(h->groups->frames);
	h->groups->frames = f;
	XSetWindowBorder(display, f->win, frame_border_focus(f));
	if (f->group->flags & GF_STACKING)
	{
		// display everything
		client *c; int i;
		FOR_RING (NEXT, c, f->group->clients, i)
			if (i && c->flags & CF_HIDDEN) client_display(c, f);
		if (c) client_focus(c, f);
	} else
	{
		// display only one
		if (f->cli && f->cli->frame == f)
			client_focus(f->cli, NULL);
		else	frame_auto_focus_hidden(f, NEXT);
	}
}
void frame_target_client(frame *f, client *c)
{
	if (c->frame && c->frame->cli == c)
		c->frame->cli = NULL;
	f->cli = c; c->frame = f;
	c->state = f->state - 1;
}
void frame_focus_client(frame *f, client *c)
{
	if (c)
	{
		client_unfocus(f->group->clients);
		frame_target_client(f, c);
		f->group->clients = c;
	}
	frame_focus(f);
}
void frame_refocus_client(frame *f, client *c)
{
	client_unfocus(f->group->clients);
	f->cli = c; c->frame = f;
	f->group->clients = c;
	frame_focus(f);
}
void frame_display_client(frame *f, client *c)
{
	if (c)
	{
		frame_target_client(f, c);
		client_display(c, f);
	} else
		frame_auto_display_hidden(f, NEXT);
}
void frame_hide(frame *f)
{
	XUnmapWindow(display, f->win);
	f->state = 1;
}
frame* frame_hsplit(dcell ratio)
{
	frame *f = heads->groups->frames;
	group *t = f->group;
	int lw = ceil(f->w * ratio);
	if (ratio >= 1 || lw < settings[ms_frame_min_wh].u || f->w - lw < settings[ms_frame_min_wh].u)
	{
		um("unable to split: %f %d %d", ratio, lw, f->w - lw);
		return NULL;
	}
	group_track(heads->groups);
	int rw = f->w - lw, rx = f->x + lw;
	f->w -= rw; frame_update(f);
	frame *n = frame_create(t, rx, f->y, rw, f->h);
	frame_update(n);
	return n;
}
frame* frame_vsplit(dcell ratio)
{
	frame *f = heads->groups->frames;
	group *t = f->group;
	int th = ceil(f->h * ratio);
	if (ratio >= 1 || th < settings[ms_frame_min_wh].u || f->h - th < settings[ms_frame_min_wh].u)
	{
		um("unable to split: %f %d %d", ratio, th, f->h - th);
		return NULL;
	}
	group_track(heads->groups);
	int bh = f->h - th, by = f->y + th;
	f->h -= bh; frame_update(f);
	frame *n = frame_create(t, f->x, by, f->w, bh);
	frame_update(n);
	return n;
}
void frame_split(ubyte direction, dcell ratio)
{
	frame *new = (direction == VERTICAL) ? frame_vsplit(ratio): frame_hsplit(ratio);
	if (new && strcasecmp(settings[ms_frame_split_focus].s, "new") == 0) frame_focus(new);
}
// return 0/false if the frame is not adjacent to the block defined by x, y, w, h.
// if a frame side does touch a block side, and the length of the frame's side is
// contained *entirely within* the block side, return the side number.
// also see frame_relative()
ubyte frame_borders(frame *s, int x, int y, int w, int h)
{
	ubyte in_y = (s->y >= y && s->y + s->h <= y + h) ? 1: 0;
	ubyte in_x = (s->x >= x && s->x + s->w <= x + w) ? 1: 0;
	if (s->x == x && in_y) return LEFT;
	if (s->x == x + w && in_y) return LEFT;
	if (s->x + s->w == x && in_y) return RIGHT;
	if (s->x + s->w == x + w && in_y) return RIGHT;
	if (s->y == y && in_x) return TOP;
	if (s->y == y + h && in_x) return TOP;
	if (s->y + s->h == y && in_x) return BOTTOM;
	if (s->y + s->h == y + h && in_x) return BOTTOM;
	return 0;
}
// locate all frames bordering the block defined by x, y, w, h.  'bordering'
// follows the frame_borders() rules.
struct frame_match* frames_bordering(group *t, int x, int y, int w, int h)
{
	// null terminator
	frame *f; int i = 0, fc;
	FOR_RING (NEXT, f, t->frames, fc); fc++;
	struct frame_match *matches = allocate(sizeof(struct frame_match)*fc);
	memset(matches, 0, sizeof(struct frame_match)*fc);
	FOR_RING (NEXT, f, t->frames, fc)
	{
		ubyte side = frame_borders(f, x, y, w, h);
		if (side)
		{
			matches[i].frame = f;
			matches[i].side = side;
			i++;
		}
	}
	return matches;
}
ubyte frame_in_set(frame **set, frame *f)
{
	int i = 0; if (!set) return 0;
	while (set[i] && set[i] != f) i++;
	return set[i] ? 1: 0;
}
// resize as many frames as necessary to fill the block defined by x, y, w, h, ignoring
// any frames in **exceptions.
void frames_fill_gap_except(group *t, frame **exceptions, int x, int y, int w, int h, ubyte mode)
{
	struct frame_match *matches = frames_bordering(t, x, y, w, h);
	int sides[5], i; memset(&sides, 0, sizeof(int)*5);
	for (i = 0; matches[i].frame; i++)
		if (!frame_in_set(exceptions, matches[i].frame))
			sides[matches[i].side]++;
	if (sides[LEFT] && sides[RIGHT])
		sides[sides[LEFT] < sides[RIGHT] ? RIGHT: LEFT] = 0;
	if (sides[TOP] && sides[BOTTOM])
		sides[sides[TOP] < sides[BOTTOM] ? BOTTOM: TOP] = 0;
	if (mode == FRAMES_FEWEST)
	{
		if (sides[TOP] + sides[BOTTOM] < sides[LEFT] + sides[RIGHT])
			{ sides[TOP] = 0; sides[BOTTOM] = 0; }
		else { sides[LEFT] = 0; sides[RIGHT] = 0; }
	}
	for (i = 0; matches[i].frame; i++)
	{
		frame *f = matches[i].frame;
		ubyte side = matches[i].side;
		if (frame_in_set(exceptions, f)) continue;
		if (!sides[side]) continue;
		     if (side == LEFT) { f->x -= w; f->w += w; }
		else if (side == RIGHT)  f->w += w;
		else if (side == TOP)  { f->y -= h; f->h += h; }
		else if (side == BOTTOM) f->h += h;
		frame_update(f);
	}
	free(matches);
}
void frames_fill_gap(group *t, int x, int y, int w, int h, ubyte mode)
{
	frames_fill_gap_except(t, NULL, x, y, w, h, mode);
}
ubyte nearest_side(int x, int y, int a, int b, int c, int d)
{
	int left = x - a, right = c - x, top = y - b, bottom = d - y;
	if (right < left && right < top && right < bottom) return RIGHT;
	if (top < left && top < bottom) return TOP;
	if (bottom < left) return BOTTOM;
	return LEFT;
}
// return false/0 if the frame does not cover the block dfine dby x, y, w, h.
// if the frame and block do overlap anywhere, return the side of the frame
// closest to a block side.
ubyte frame_covers(frame *s, int x, int y, int width, int height)
{
	ubyte side = 0;
	int a = x, b = y, c = x + width, d = y + height;
	int e = s->x, f = s->y, g = s->x + s->w, h = s->y + s->h;
	note("%d %d %d %d %d %d %d %d", a, b, c, d, e, f, g, h);
	     if (e > a && f > b && e < c && f < d) // ef
		side = nearest_side(e, f, a, b, c, d);
	else if (g > a && f > b && g < c && f < d) // gf
		side = nearest_side(g, f, a, b, c, d);
	else if (e > a && h > b && e < c && h < d) // eh
		side = nearest_side(e, h, a, b, c, d);
	else if (g > a && h > b && g < c && h < d) // gh
		side = nearest_side(g, h, a, b, c, d);
	else if (a > e && b > f && a < g && b < h) // ab
		side = nearest_side(a, b, e, f, g, h);
	else if (c > e && b > f && c < g && b < h) // cb
		side = nearest_side(c, b, e, f, g, h);
	else if (a > e && d > f && a < g && d < h) // ad
		side = nearest_side(a, d, e, f, g, h);
	else if (c > e && d > f && c < g && d < h) // cd
		side = nearest_side(c, d, e, f, g, h);
	return side;
}
// resize any frames obscuring the block defined by x, y, width, height, ignoring
// any frames in **exceptions.
void frames_make_gap_except(group *t, frame **exceptions, int x, int y, int width, int height)
{
	frame *s; int i;
	FOR_RING (NEXT, s, t->frames, i)
	{
		ubyte side = 0;
		if (!frame_in_set(exceptions, s))
		{
			side = frame_covers(s, x, y, width, height);
			int inc = 0;
			     if (side == LEFT) { inc = x + width - s->x; s->x += inc; s->w -= inc; }
			else if (side == RIGHT)  s->w -= (s->x + s->w) - x;
			else if (side == TOP)  { inc = y + height - s->y; s->y += inc; s->h -= inc; }
			else if (side == BOTTOM) s->h -= (s->y + s->h) - y;
			else {
				side = frame_borders(s, x, y, width, height);
				     if (side == LEFT) { s->x += width; s->w -= width; }
				else if (side == RIGHT)  s->w -= width;
				else if (side == TOP)  { s->y += height; s->h -= height; }
				else if (side == BOTTOM) s->h -= height;
			}
		}
		if (side) frame_update(s);
	}
}
void frames_make_gap(group *t, int x, int y, int w, int h)
{
	frames_make_gap_except(t, NULL, x, y, w, h);
}
// look for a frame that has a side on the same 'axis' as the supplied frame, *and* has
// 'side' adjacent to the supplied frame.
frame* frame_sibling(frame *f, ubyte axis, ubyte side)
{
	frame *s; int i;
	// dont return a sibling if a frame opposite the 'axis' has an opposing side in line with 'side'
	FOR_RING (NEXT, s, f->group->frames, i)
	{
		if (s != f && (
			(side == LEFT && s->x + s->w == f->x &&
				((axis == TOP && s->y + s->h == f->y) || (axis == BOTTOM && s->y == f->y + f->h))) ||
			(side == RIGHT && s->x == f->x + f->w &&
				((axis == TOP && s->y + s->h == f->y) || (axis == BOTTOM && s->y == f->y + f->h))) ||
			(side == TOP && s->y + s->h == f->y &&
				((axis == LEFT && s->x + s->w == f->x) || (axis == RIGHT && s->x == f->x + f->w))) ||
			(side == BOTTOM && s->y == f->y + f->h &&
				((axis == LEFT && s->x + s->w == f->x) || (axis == RIGHT && s->x == f->x + f->w)))
			)) return NULL;
	}
	// return a sibling if a frame is on the same side of the 'axis' has an oppsoing side abutting 'side'
	FOR_RING (NEXT, s, f->group->frames, i)
	{
		if (s != f && (
			(axis == LEFT   && f->x == s->x) ||
			(axis == RIGHT  && f->x + f->w == s->x + s->w) ||
			(axis == TOP    && f->y == s->y) ||
			(axis == BOTTOM && f->y + f->h == s->y + s->h)
			) && (
			(side == LEFT   && f->x == s->x + s->w) ||
			(side == RIGHT  && f->x + f->w == s->x) ||
			(side == TOP    && f->y == s->y + s->h) ||
			(side == BOTTOM && f->y + f->h == s->y)
			)) return s;
	}
	return NULL;
}
// find a frame's siblings on the same axis.  these will form a single block bordering the axis
frame** frame_siblings(frame *f, ubyte axis)
{
	frame *s, **matches = allocate(sizeof(frame*)*NOTE);
	memset(matches, 0, sizeof(frame*)*NOTE);
	// first check for an opposite frame that exactly matches. we don't
	// want to move a group unless we have to
	int i = 0; matches[i++] = f;
	ubyte side1 = 0, side2 = 0;
	if (axis == LEFT || axis == RIGHT)
		{ side1 = TOP; side2 = BOTTOM; }
	else { side1 = LEFT; side2 = RIGHT; }
	s = f->group->frames;
	do {	s = frame_sibling(s, axis, side1);
		if (s) matches[i++] = s;
	} while (s);
	s = f;
	do {	s = frame_sibling(s, axis, side2);
		if (s) matches[i++] = s;
	} while (s);
	return matches;
}
// check whether a group of frames is resizable in the direction of 'axis'
ubyte frame_siblings_growable(frame **siblings, ubyte axis, ucell size)
{
	int i, x = 0, y = 0, w = 0, h = 0;
	frame *f = siblings[0];
	if (axis == LEFT || axis == RIGHT)
	{
		if (axis == LEFT) { x = f->x - size; y = f->y; }
		else { x = f->x + f->w; y = f->y; }
		w = size; h = 0;
		for (i = 0; siblings[i]; i++)
			y = MIN(y, siblings[i]->y), h += siblings[i]->h;
	} else
	if (axis == TOP || axis == BOTTOM)
	{
		if (axis == TOP) { x = f->x; y = f->y - size; }
		else { x = f->x; y = f->y + f->h; }
		w = 0; h = size;
		for (i = 0; siblings[i]; i++)
			x = MIN(x, siblings[i]->x), w += siblings[i]->w;
	}
	FOR_RING (NEXT, f, f->group->frames, i)
	{
		if (!frame_in_set(siblings, f) && frame_covers(f, x-1, y-1, w+2, h+2))
		{
			if ((axis == LEFT || axis == RIGHT ) && f->w <= settings[ms_frame_min_wh].u) return 0;
			if ((axis == TOP  || axis == BOTTOM) && f->h <= settings[ms_frame_min_wh].u) return 0;
		}
	}
	return 1;
}
ubyte frame_siblings_shrinkable(frame **siblings, ubyte axis, ucell size)
{
	int i;
	for (i = 0; siblings[i]; i++)
	{
		frame *f = siblings[i];
		if ((axis == LEFT || axis == RIGHT ) && f->w <= settings[ms_frame_min_wh].u) return 0;
		if ((axis == TOP  || axis == BOTTOM) && f->h <= settings[ms_frame_min_wh].u) return 0;
	}
	return 1;
}
void frame_remove()
{
	frame *f = heads->groups->frames;
	group *t = f->group;
	if (t->frames->next == f) return;
	group_track(t);
	frame *set[2]; set[0] = f; set[1] = NULL;
	frames_fill_gap_except(f->group, set, f->x, f->y, f->w, f->h, FRAMES_FEWEST);
	frame_destroy(f);
	frame_focus(t->frames);
	frames_display_hidden(t);
}
ubyte frame_shrink(ubyte direction, ubyte adapt, ucell size)
{
	group *t = heads->groups;
	frame *f = t->frames;
	frame **group = NULL;
	int i = 0, x, y, w, h;
	if (direction == HORIZONTAL)
	{
		if (f->x + f->w >= t->head->screen->width - t->r)
		{
			if (adapt) return frame_grow(direction, 0, size);
			else {
				if (f->w <= settings[ms_frame_min_wh].u || t->frames->next == f || f->x <= t->l) return 0;
				group = frame_siblings(f, LEFT);
				if (frame_siblings_shrinkable(group, LEFT, size))
				{
					x = f->x; w = size; y = f->y; h = 0;
					for (i = 0; group[i]; i++)
						y = MIN(y, group[i]->y), h += group[i]->h;
					frames_fill_gap_except(f->group, group, x, y, w, h, FRAMES_FEWEST);
					for (i = 0; group[i]; i++)
					{
						group[i]->w -= size, group[i]->x += size;
						frame_update(group[i]);
					}
				}
			}
		} else
		{
			if (f->w <= settings[ms_frame_min_wh].u || t->frames->next == f) return 0;
			group = frame_siblings(f, RIGHT);
			if (frame_siblings_shrinkable(group, RIGHT, size))
			{
				x = f->x + f->w - size; w = size; y = f->y; h = 0;
				for (i = 0; group[i]; i++)
					y = MIN(y, group[i]->y), h += group[i]->h;
				frames_fill_gap_except(f->group, group, x, y, w, h, FRAMES_FEWEST);
				for (i = 0; group[i]; i++)
				{
					group[i]->w -= size;
					frame_update(group[i]);
				}
			}
		}
	} else
	if (direction == VERTICAL)
	{
		if (f->y + f->h >= t->head->screen->height - t->b)
		{
			if (adapt) return frame_grow(direction, 0, size);
			else {
				if (f->h <= settings[ms_frame_min_wh].u || t->frames->next == f || f->y <= t->t) return 0;
				group = frame_siblings(f, TOP);
				if (frame_siblings_shrinkable(group, TOP, size))
				{
					x = f->x; w = 0; y = f->y; h = size;
					for (i = 0; group[i]; i++)
						x = MIN(x, group[i]->x), w += group[i]->w;
					frames_fill_gap_except(f->group, group, x, y, w, h, FRAMES_FEWEST);
					for (i = 0; group[i]; i++)
					{
						group[i]->h -= size, group[i]->y += size;
						frame_update(group[i]);
					}
				}
			}
		} else
		{
			if (f->h <= settings[ms_frame_min_wh].u || t->frames->next == f) return 0;
			group = frame_siblings(f, BOTTOM);
			if (frame_siblings_shrinkable(group, BOTTOM, size))
			{
				y = f->y + f->h - size; h = size; x = f->x; w = 0;
				for (i = 0; group[i]; i++)
					x = MIN(x, group[i]->x), w += group[i]->w;
				frames_fill_gap_except(f->group, group, x, y, w, h, FRAMES_FEWEST);
				for (i = 0; group[i]; i++)
				{
					group[i]->h -= size;
					frame_update(group[i]);
				}
			}
		}
	}
	if (group) free(group);
	frame_focus(f);
	return 1;
}
ubyte frame_grow(ubyte direction, ubyte adapt, ucell size)
{
	group *t = heads->groups;
	frame *f = t->frames;
	frame **group = NULL;
	int i = 0, x, y, w, h;
	ubyte changes = 0;
	if (direction == HORIZONTAL)
	{
		if (f->x + f->w >= t->head->screen->width - t->r)
		{
			if (adapt) return frame_shrink(direction, 0, size);
			else {
				if (t->frames->next == f || f->x <= t->l) return 0;
				group = frame_siblings(f, LEFT);
				if (frame_siblings_growable(group, LEFT, size))
				{
					changes = 1;
					x = f->x - size; w = size; y = f->y; h = 0;
					for (i = 0; group[i]; i++)
						y = MIN(y, group[i]->y), h += group[i]->h;
					frames_make_gap_except(f->group, group, x, y, w, h);
					for (i = 0; group[i]; i++)
					{
						group[i]->w += size, group[i]->x -= size;
						frame_update(group[i]);
					}
				}
			}
		} else
		{
			if (t->frames->next == f) return 0;
			group = frame_siblings(f, RIGHT);
			if (frame_siblings_growable(group, RIGHT, size))
			{
				changes = 1;
				x = f->x + f->w; w = size; y = f->y; h = 0;
				for (i = 0; group[i]; i++)
					y = MIN(y, group[i]->y), h += group[i]->h;
				frames_make_gap_except(f->group, group, x, y, w, h);
				for (i = 0; group[i]; i++)
				{
					group[i]->w += size;
					frame_update(group[i]);
				}
			}
		}
	} else
	if (direction == VERTICAL)
	{
		if (f->y + f->h >= t->head->screen->height - t->b)
		{
			if (adapt) return frame_shrink(direction, 0, size);
			else {
				if (t->frames->next == f || f->y <= t->t) return 0;
				group = frame_siblings(f, TOP);
				if (frame_siblings_growable(group, TOP, size))
				{
					changes = 1;
					y = f->y - size; h = size; x = f->x; w = 0;
					for (i = 0; group[i]; i++)
						x = MIN(x, group[i]->x), w += group[i]->w;
					frames_make_gap_except(f->group, group, x, y, w, h);
					for (i = 0; group[i]; i++)
					{
						group[i]->h += size, group[i]->y -= size;
						frame_update(group[i]);
					}
				}
			}
		} else
		{
			if (t->frames->next == f) return 0;
			group = frame_siblings(f, BOTTOM);
			if (frame_siblings_growable(group, BOTTOM, size))
			{
				changes = 1;
				y = f->y + f->h; h = size; x = f->x; w = 0;
				for (i = 0; group[i]; i++)
					x = MIN(x, group[i]->x), w += group[i]->w;
				frames_make_gap_except(f->group, group, x, y, w, h);
				for (i = 0; group[i]; i++)
				{
					group[i]->h += size;
					frame_update(group[i]);
				}
			}
		}
	}
	if (group) free(group);
	frame_focus(f);
	return 1;
}
void frame_single(frame *f)
{
	group *g = f->group;
	while (f->next != f) frame_destroy(f->next);
	f->x = g->l; f->y = g->t;
	f->w = g->head->screen->width  - g->r - g->l;
	f->h = g->head->screen->height - g->b - g->t;
	// don't do a frame update here in case frame is not in an active group
}
char frames_overlap_y(frame *a, frame *b)
{
	return ((a->y <= b->y && (a->y + a->h) > b->y) || (b->y <= a->y && (b->y + b->h) > a->y))
		? 1: 0;
}
char frames_overlap_x(frame *a, frame *b)
{
	return ((a->x <= b->x && (a->x + a->w) > b->x) || (b->x <= a->x && (b->x + b->w) > a->x))
		? 1 : 0;
}
// find a frame that has some adjacent border to the supplied frame, in the direction of side
// this bails out with the first find.  could be expanded to return the frame with the longest
// stretch of adjacent border, which might be more intuitive in the UI navigation.
frame* frame_relative(frame *f, ubyte side)
{
	frame *s; int i;
	FOR_RING (NEXT, s, f, i)
	{
		// skip self
		if (!i) continue;
		if (	(side == LEFT   && f->x == (s->x + s->w) && frames_overlap_y(f, s)) ||
			(side == RIGHT  && s->x == (f->x + f->w) && frames_overlap_y(f, s)) ||
			(side == TOP    && f->y == (s->y + s->h) && frames_overlap_x(f, s)) ||
			(side == BOTTOM && s->y == (f->y + f->h) && frames_overlap_x(f, s))
			) return s;
	}
	return NULL;
}
void frame_left()
{
	frame *s = frame_relative(heads->groups->frames, LEFT);
	if (s) frame_focus(s);
}
void frame_right()
{
	frame *s = frame_relative(heads->groups->frames, RIGHT);
	if (s) frame_focus(s);
}
void frame_up()
{
	frame *s = frame_relative(heads->groups->frames, TOP);
	if (s) frame_focus(s);
}
void frame_down()
{
	frame *s = frame_relative(heads->groups->frames, BOTTOM);
	if (s) frame_focus(s);
}
// exchange the content of the current frame for another
void frame_swap(ubyte direction)
{
	group_track(heads->groups);
	group *t = heads->groups;
	frame *f = t->frames;
	frame *s = frame_relative(f, direction);
	if (s)
	{
		client *a = f->cli, *b = s->cli;
		f->cli = NULL; s->cli = NULL;
		if (a) client_display(a, s);
		if (b) client_display(b, f);
		frame_focus(s);
	}
}
void frame_slide(ubyte direction)
{
	group_track(heads->groups);
	group *t = heads->groups;
	frame *f = t->frames;
	frame *s = frame_relative(f, direction);
	if (s)
	{
		frame_focus_client(s, f->cli);
		frame_auto_display_hidden(f, NEXT);
	}
}
void frame_dedicate()
{
	frame *f = heads->groups->frames;
	if (f->flags & FF_CATCHALL) return;
	f->flags ^= FF_DEDICATE;
	frame_focus(f);
}
void frame_catchall()
{
	int i;
	frame *a, *f = heads->groups->frames;
	f->flags ^= FF_CATCHALL;
	// catchall means undedicated
	f->flags &= ~FF_DEDICATE;
	if (f->flags & FF_CATCHALL)
	{
		FOR_RING (NEXT, a, f, i)
		{
			if (!i) continue;
			// there can only be one
			if (a->flags & FF_CATCHALL)
			{
				a->flags &= ~FF_CATCHALL;
				frame_unfocus(a);
			}
		}
	}
	frame_focus(f);
}

// GROUPS

void group_push(head *h, group *t)
{
	t->head = h;
	group *last = group_last(h);
	t->id = last ? last->id+1 : 0;
	if (h->groups)
	{
		group *tmp = last->next;
		last->next = t; tmp->prev = t;
		t->prev = last; t->next = tmp;
	} else
		h->groups = t->next = t->prev = t;
}
void group_pop(group *t)
{
	head *h = t->head;
	if (h->groups == h->groups->next)
		h->groups = NULL;
	else {
		t->next->prev = t->prev;
		t->prev->next = t->next;
		if (h->groups == t)
			h->groups = t->next;
	}
}
group* group_create(head *head, char *name, int x, int y, int w, int h)
{
	group *t = allocate(sizeof(group));
	t->frames = NULL; t->clients = NULL; t->flags = GF_TILING;
	t->head = NULL; t->next = NULL; t->prev = NULL;
	t->l = 0; t->r = 0; t->t = 0; t->b = 0; t->id = 0;
	strcpy(t->name, name); t->states = stack_create();
	t->stacked = stack_create();
	if (head) group_push(head, t);
	frame_create(t, x, y, w, h);
	return t;
}
void group_destroy(group *t)
{
	head *h = t->head; group *g; int i;
	FOR_RING (NEXT, g, h->groups, i)
		if (g->id > t->id) g->id--;
	while (t->clients)
		client_regroup(t->next, t->clients);
	while (t->frames)
		frame_destroy(t->frames);
	while (t->states->depth)
		free(stack_pop(t->states));
	group_pop(t);
	stack_free(t->states);
	stack_free(t->stacked);
	stack_discard(h->stacked, t);
	free(t);
}
void group_unfocus(group *t)
{
	if (heads->groups == t) frame_unfocus(t->frames);
}
void group_hide(group *t)
{
	frame *f; client *c; int i;
	FOR_RING (NEXT, f, t->frames, i)
		frame_hide(f);
	FOR_RING (NEXT, c, t->clients, i)
		client_hide(c);
	if (settings[ms_group_close_empty].u && !t->clients && t->next != t)
		group_destroy(t);
}
void group_focus(group *t)
{
	frame *f; int i;
	t->head->groups = t;
	FOR_RING (NEXT, f, t->frames, i)
		frame_update(f);
	frame_focus(t->frames);
	ewmh_clients();
	ewmh_groups();
	stack_discard(t->head->stacked, t);
	stack_push(t->head->stacked, t);
}
void group_raise(group *g)
{
	if (g != g->head->groups)
	{
		group *old = g->head->groups;
		group_unfocus(old);
		group_focus(g);
		group_hide(old);
	}
}
void group_next()
{
	if (heads->groups != heads->groups->next)
		group_raise(heads->groups->next);
}
void group_prev()
{
	if (heads->groups != heads->groups->prev)
		group_raise(heads->groups->prev);
}
void group_other()
{
	if (heads->stacked->depth > 1)
		group_raise(stack_get(heads->stacked, 1));
}
// resize frames to match changes in the screen border padding
void group_resize(group *ta, int l, int r, int t, int b)
{
	frame *f = ta->frames; int i;
	int sw = ta->head->screen->width;
	int sh = ta->head->screen->height;
	     if (l < ta->l) { frames_fill_gap(ta, l, ta->t, ta->l - l, sh - ta->b, FRAMES_ALL); ta->l = l; }
	else if (l > ta->l) { frames_make_gap(ta, ta->l, ta->t, l - ta->l, sh - ta->b); ta->l = l; }
	     if (r < ta->r) { frames_fill_gap(ta, sw - ta->r, ta->t, ta->r - r, sh - ta->b, FRAMES_ALL); ta->r = r; }
	else if (r > ta->r) { frames_make_gap(ta, sw - r, ta->t, r - ta->r, sh - ta->b); ta->r = r; }
	     if (t < ta->t) { frames_fill_gap(ta, ta->l, t, sw - ta->l - ta->r, ta->t - t, FRAMES_ALL); ta->t = t; }
	else if (t > ta->t) { frames_make_gap(ta, ta->l, ta->t, sw - ta->l - ta->r, t - ta->t); ta->t = t; }
	     if (b < ta->b) { frames_fill_gap(ta, ta->l, sh - ta->b, sw - ta->l - ta->r, ta->b - b, FRAMES_ALL); ta->b = b; }
	else if (b > ta->b) { frames_make_gap(ta, ta->l, sh - b, sw - ta->l - ta->r, b - ta->b); ta->b = b; }
	FOR_RING (NEXT, f, ta->frames, i)
		frame_update(f);
}
group* group_by_name(head *h, char *name)
{
	group *t; int i;
	FOR_RING (NEXT, t, h->groups, i)
		if (strcmp(t->name, name) == 0) return t;
	return NULL;
}
group* group_by_id(head *h, ucell id)
{
	group *t; int i;
	FOR_RING (NEXT, t, h->groups, i)
		if (t->id == id) return t;
	return NULL;
}
group* group_from_string(head *h, char *s, group *def)
{
	if (!s) return def;
	group *g = isdigit(*s) ? group_by_id(h, strtol(s, NULL, 10)): group_by_name(h, s);
	return g ? g: def;
}
group* group_auto_create(head *h, char *name)
{
	group *g = group_from_string(h, name, NULL);
	if (!g && !isdigit(*name))
		g = group_create(heads, name, 0, 0, heads->screen->width, heads->screen->height);
	return g;
}
char* group_dump(group *g)
{
	frame *f; int i;
	autostr s; str_create(&s);
	str_print(&s, NOTE, "group\t%d\t%d\t%d\t%d\t%s\n", g->l, g->r, g->t, g->b, g->name);
	FOR_RING (NEXT, f, g->frames, i)
		str_print(&s, NOTE, "frame\t%d\t%d\t%d\t%d\t%u\t%s\t%s\n", f->x, f->y, f->w, f->h, f->flags,
			f->cli ? f->cli->class: NULL, f->cli ? f->cli->role: NULL);
	client *c;
	FOR_RING (NEXT, c, g->clients, i)
		str_print(&s, NOTE, "window\t%d\t%s\t%s\t%s\n", i, c->class, c->name, c->role);
	return s.pad;
}
void group_load(group *g, char *dump)
{
	frame *f; client *c; int i;
	char *line = dump, *l = line, *r = l, *class, *role;
	int x, y, w, h; ubyte flags;
	while (g->frames) frame_destroy(g->frames);
	while (*line)
	{
		if (strncmp(line, "group", 5) == 0)
		{
			l = line + 5; strskip(&l, isspace);
			x = strtol(l, &l, 10); strskip(&l, isspace);
			y = strtol(l, &l, 10); strskip(&l, isspace);
			w = strtol(l, &l, 10); strskip(&l, isspace);
			h = strtol(l, &l, 10); strskip(&l, isspace);
			g->l = x; g->r = y; g->t = w; g->b = h;
			r = strchr(l, '\n');
			memmove(g->name, l, (r-l));
			g->name[r-l] = '\0';
		} else
		if (strncmp(line, "frame", 5) == 0)
		{
			l = line + 5; strskip(&l, isspace);
			x = strtol(l, &l, 10); strskip(&l, isspace);
			y = strtol(l, &l, 10); strskip(&l, isspace);
			w = strtol(l, &l, 10); strskip(&l, isspace);
			h = strtol(l, &l, 10); strskip(&l, isspace);
			flags = strtol(l, &l, 10); strskip(&l, isspace);
			class = strnextthese(&l, "\t"); strskip(&l, isspace);
			role = strnextthese(&l, "\t\n");
			f = frame_create(g, x, y, w, h); f->flags = flags;
			FOR_RING (NEXT, c, g->clients, i)
			{
				if (!c->frame && strcmp(c->class, class) == 0 && strcmp(c->role, role) == 0)
				{
					f->cli = c; c->frame = f;
					break;
				}
			}
			free(class); free(role);
		}
		strscanthese(&line, "\n");
		strskip(&line, isspace);
	}
	FOR_RING (NEXT, c, g->clients, i) c->state = 0;
	FOR_RING (NEXT, f, g->frames, i) frame_update(f);
	frame_focus(g->frames);
}
void group_track(group *g)
{
	if (g->states->depth == STACK)
		free(stack_shift(g->states));
	stack_push(g->states, group_dump(g));
}
void group_undo(group *g)
{
	if (g->states->depth > 0)
	{
		char *layout = stack_pop(g->states);
		group_load(g, layout); free(layout);
	}
	else	um("nothing to undo for %s", g->name);
}
void group_stack()
{
	group *g = heads->groups;
	if (g->flags & GF_STACKING)
	{
		g->flags &= ~GF_STACKING;
		g->flags |= GF_TILING;
		group_undo(g);
	} else
	{
		group_track(g);
		while (g->frames) frame_destroy(g->frames);
		g->l = 0; g->r = 0; g->t = 0; g->b = 0;
		frame_create(g, 0, 0, g->head->screen->width, g->head->screen->height);
		g->flags &= ~GF_TILING; g->flags |= GF_STACKING;
		frame_update(g->frames);
		client *c; int i;
		FOR_RING (NEXT, c, g->clients, i)
		{
			if (c->fw > 0 && c->fh > 0)
			{
				c->x = c->fx; c->y = c->fy; c->w = c->fw; c->h = c->fh;
			} else
			{
				// compensate for now-invisible frame border
				c->x--; c->y--;
			}
			XMoveResizeWindow(display, c->win, c->x, c->y, c->w, c->h);
			c->fx = c->x; c->fy = c->y; c->fw = c->w; c->fh = c->h;
			client_display(c, g->frames);
		}
		frame_focus(g->frames);
	}
}
// HEADS
void head_focus(head *h)
{
	group_unfocus(heads->groups);
	heads = h;
	if (heads->display_string)
		putenv(heads->display_string);
	group_focus(heads->groups);
}
void head_next()
{
	head_focus(heads->next);
}
// EVENTS
void launch(char *cmd)
{
	// exec_markers[] is a circular buffer
	exec_pointer = (exec_pointer + 1) % EXEC_MARKERS;
	struct exec_marker *m = &exec_markers[exec_pointer];
	m->pid = exec_cmd(cmd);
	m->group = heads->groups;
	m->frame = m->group->frames;
	m->time = time(0);
}
void menu(char *cmd, char *after)
{
	char *tmp = allocate(strlen(cmd)+strlen(after)+BLOCK);
	sprintf(tmp, "%s | %s | %s", cmd, settings[ms_dmenu].s, after);
	printf("%s\n", tmp);
	launch(tmp); free(tmp);
}
void menu_wrapper(char *cmd, char *after)
{
	autostr s; str_create(&s);
	str_print(&s, strlen(cmd)+NOTE, "echo \"%s\"", cmd);
	menu(s.pad, after);
	str_free(&s);
}
void shutdown()
{
	// we just bail out for now.  any clients that want to stay alive, and are
	// not our descendants, can do so.
	// this assumes that the X session will not also stop when we die :-)
	exit(EXIT_SUCCESS);
}
dcell parse_size(char *cmd, regmatch_t *subs, ucell index, ucell limit)
{
	dcell size = 0, an = 0, bn = 0;
	char *a = regsubstr(cmd, subs, index);
	char *b = regsubstr(cmd, subs, index+1);
	an = strtod(a, NULL);
	if (b) bn = strtod(b, NULL);
	if (strchr(a, '%')) size = (an / 100) * limit; // percent
	else if (!b) size = an; // pixels
	else	size = (an / bn) * limit; // fraction
	free(a); free(b);
	return MIN(MAX(size, settings[ms_frame_min_wh].u), limit);
}
char* com_frame_split(char *cmd, regmatch_t *subs)
{
	group *g = heads->groups; frame *f = g->frames;
	int sw = heads->screen->width  - g->l - g->r,
	    sh = heads->screen->height - g->t - g->b;
	char *mode = regsubstr(cmd, subs, 1);
	ucell fs = f->h, ss = sh; ubyte dir = VERTICAL;
	if (*mode == 'h') { fs = f->w; ss = sw; dir = HORIZONTAL; }
	dcell size = parse_size(cmd, subs, 2, fs);
	frame_split(dir, size / fs);
	free(mode);
	return NULL;
}
char* com_frame_size(char *cmd, regmatch_t *subs)
{
	group *g = heads->groups; frame *f = g->frames;
	int sw = heads->screen->width  - g->l - g->r,
	    sh = heads->screen->height - g->t - g->b;
	char *mode = regsubstr(cmd, subs, 1);
	ucell fs = f->h, ss = sh; ubyte dir = VERTICAL;
	if (*mode == 'w') { fs = f->w; ss = sw; dir = HORIZONTAL; }
	dcell size = parse_size(cmd, subs, 2, ss);
	// auto-create another frame if we are full w/h
	if (fs == ss)
		frame_split(dir, size / ss);
	else {
		group_track(heads->groups);
		if (fs > size)
			frame_shrink(dir, 0, fs - size);
		else	frame_grow(dir, 0, size - fs);
	}
	free(mode);
	return NULL;
}
char* com_frame_resize(char *cmd, regmatch_t *subs)
{
	char *op = regsubstr(cmd, subs, 1);
	     if (*op == 'u') frame_shrink(VERTICAL, 1, settings[ms_frame_resize].u);
	else if (*op == 'd') frame_grow(VERTICAL, 1, settings[ms_frame_resize].u);
	else if (*op == 'l') frame_shrink(HORIZONTAL, 1, settings[ms_frame_resize].u);
	else if (*op == 'r') frame_grow(HORIZONTAL, 1, settings[ms_frame_resize].u);
	free(op);
	return NULL;
}
char* com_group_pad(char *cmd, regmatch_t *subs)
{
	char *l = regsubstr(cmd, subs, 1);
	char *r = regsubstr(cmd, subs, 2);
	char *t = regsubstr(cmd, subs, 3);
	char *b = regsubstr(cmd, subs, 4);
	int ln = strtol(l, NULL, 10);
	int rn = strtol(r, NULL, 10);
	int tn = strtol(t, NULL, 10);
	int bn = strtol(b, NULL, 10);
	group_resize(heads->groups, ln, rn, tn, bn);
	free(l); free(r); free(t); free(b);
	return NULL;
}
char* com_group_add(char *cmd, regmatch_t *subs)
{
	char *name = regsubstr(cmd, subs, 1);
	group *old = heads->groups;
	group_unfocus(old);
	group *new = group_create(heads, name, 0, 0, heads->screen->width, heads->screen->height);
	group_focus(new);
	group_hide(old);
	free(name);
	return NULL;
}
char* com_group_drop(char *cmd, regmatch_t *subs)
{
	group *active = heads->groups;
	char *name = regsubstr(cmd, subs, 1);
	group *t = group_from_string(heads, name, NULL);
	if (t && heads->groups != heads->groups->next)
	{
		group_destroy(t);
		if (t == active) group_focus(heads->groups);
	}
	free(name);
	return NULL;
}
char* com_group_name(char *cmd, regmatch_t *subs)
{
	char *name = regsubstr(cmd, subs, 1);
	group *t = heads->groups;
	snprintf(t->name, 32, "%s", name);
	free(name);
	return NULL;
}
char* com_window_to_group(char *cmd, regmatch_t *subs)
{
	char *name = regsubstr(cmd, subs, 1);
	client *c = heads->groups->frames->cli;
	while (c && c->parent && is_valid_client(c->parent))
		c = c->parent;
	if (c)
	{
		group *g = group_auto_create(heads, name);
		if (g)
		{
			client_regroup(g, c);
			frame_auto_focus_hidden(heads->groups->frames, NEXT);
		}
	}
	free(name);
	return NULL;
}
char* com_window_raise(char *cmd, regmatch_t *subs)
{
	char *slot = regsubstr(cmd, subs, 1);
	client *c = client_from_string(heads->groups, slot, NULL);
	if (c) client_raise(c);
	free(slot);
	return NULL;
}
char* com_window_shrink(char *cmd, regmatch_t *subs)
{
	char *slot = regsubstr(cmd, subs, 1);
	client *c = client_from_string(heads->groups, slot, heads->groups->frames->cli);
	if (c)
	{
		client_shrink(c);
		if (c->frame) frame_auto_focus_hidden(c->frame, NEXT);
	}
	free(slot);
	return NULL;
}
char* com_refresh(char *cmd, regmatch_t *subs)
{
	client *c = heads->groups->clients;
	if (c) client_refresh(c);
	return NULL;
}
char* com_border(char *cmd, regmatch_t *subs)
{
	frame *f = heads->groups->frames;
	ubyte flag = parse_flag(cmd, subs, 1, !(f->flags & FF_HIDEBORDER));
	if (flag) f->flags &= ~FF_HIDEBORDER;
	else f->flags |= FF_HIDEBORDER;
	f->state++;
	frame_update(f);
	frame_focus(f);
	return NULL;
}
char* com_frame_swap(char *cmd, regmatch_t *subs)
{
	group_track(heads->groups);
	char *op = regsubstr(cmd, subs, 1);
	     if (*op == 'u') frame_swap(TOP);
	else if (*op == 'd') frame_swap(BOTTOM);
	else if (*op == 'l') frame_swap(LEFT);
	else if (*op == 'r') frame_swap(RIGHT);
	free(op);
	return NULL;
}
char* com_frame_slide(char *cmd, regmatch_t *subs)
{
	group_track(heads->groups);
	char *op = regsubstr(cmd, subs, 1);
	     if (*op == 'u') frame_slide(TOP);
	else if (*op == 'd') frame_slide(BOTTOM);
	else if (*op == 'l') frame_slide(LEFT);
	else if (*op == 'r') frame_slide(RIGHT);
	free(op);
	return NULL;
}
char* com_group_dump(char *cmd, regmatch_t *subs)
{
	char *file = regsubstr(cmd, subs, 1);
	group *g = heads->groups;
	if (g)
	{
		char *dump = group_dump(g);
		try (oops) {
			blurt(file, dump);
		} catch (oops);
		if (oops.code)
			um("could not write %s", file);
		if (dump) free(dump);
	}
	free(file);
	return NULL;
}
char* com_group_load(char *cmd, regmatch_t *subs)
{
	char *file = regsubstr(cmd, subs, 1);
	group *g = heads->groups;
	if (g)
	{
		char *dump = NULL;
		try (oops) {
			dump = slurp(file, NULL);
			group_track(g);
			group_load(g, dump);
		} catch (oops);
		if (oops.code)
			um("could not read %s", file);
		if (dump) free(dump);
	}
	free(file);
	return NULL;
}
char* com_frame_remove(char *cmd, regmatch_t *subs)
{
	frame_remove();
	return NULL;
}
char* com_frame_kill(char *cmd, regmatch_t *subs)
{
	char *name = regsubstr(cmd, subs, 1);
	client *c = client_from_string(heads->groups, name, heads->groups->frames->cli);
	if (c) client_kill(c);
	free(name);
	return NULL;
}
char* com_frame_cycle(char *cmd, regmatch_t *subs)
{
	frame *f = heads->groups->frames;
	char *local = NULL, *dir = regsubstr(cmd, subs, 1);
	if (dir && *dir == 'l')
	{
		local = dir;
		dir = regsubstr(cmd, subs, 2);
	}
	if (!dir || *dir == 'n')
		frame_focus_hidden(f, NEXT, local ? 1:0, ANY);
	else frame_focus_hidden(f, PREV, local ? 1:0, ANY);
	free(local); free(dir);
	return NULL;
}
char* com_frame_only(char *cmd, regmatch_t *subs)
{
	group_track(heads->groups);
	frame *f = heads->groups->frames;
	frame_single(f);
	frame_update(f);
	frame_focus(f);
	return NULL;
}
char* com_group_undo(char *cmd, regmatch_t *subs)
{
	group_undo(heads->groups);
	return NULL;
}
ubyte parse_flag(char *cmd, regmatch_t *subs, ucell index, ubyte current)
{
	char *mode = regsubstr(cmd, subs, index);
	ubyte flag = 0;
	if (strcasecmp(mode, "on") == 0) flag = 1;
	if (strcasecmp(mode, "flip") == 0)
		flag = current ? 0: 1;
	free(mode);
	return flag;
}
char* com_frame_dedicate(char *cmd, regmatch_t *subs)
{
	frame *f = heads->groups->frames;
	ubyte on = parse_flag(cmd, subs, 1, f->flags & FF_DEDICATE);
	if ((on && !(f->flags & FF_DEDICATE)) ||
		(!on && f->flags & FF_DEDICATE))
		frame_dedicate();
	return NULL;
}
char* com_frame_catchall(char *cmd, regmatch_t *subs)
{
	frame *f = heads->groups->frames;
	ubyte on = parse_flag(cmd, subs, 1, f->flags & FF_CATCHALL);
	if ((on && !(f->flags & FF_CATCHALL)) ||
		(!on && f->flags & FF_CATCHALL))
		frame_catchall();
	return NULL;
}
char* com_frame_focus(char *cmd, regmatch_t *subs)
{
	char *dir = regsubstr(cmd, subs, 1);
		if (*dir == 'u') frame_up();
	else	if (*dir == 'd') frame_down();
	else	if (*dir == 'l') frame_left();
	else	if (*dir == 'r') frame_right();
	free(dir);
	return NULL;
}
char* com_group_use(char *cmd, regmatch_t *subs)
{
	group *g = NULL;
	char *name = regsubstr(cmd, subs, 1);
	if (strcasecmp(name, "(next)") == 0) group_next();
	else if (strcasecmp(name, "(prev)") == 0) group_prev();
	else if (strcasecmp(name, "(other)") == 0) group_other();
	else if ((g = group_from_string(heads, name, NULL)) != NULL) group_raise(g);
	else com_group_add(cmd, subs);
	free(name);
	return NULL;
}
char* com_exec(char *cmd, regmatch_t *subs)
{
	char *exec = regsubstr(cmd, subs, 1);
	if (strlen(exec)) launch(exec);
	free(exec);
	return NULL;
}
char* com_manage(char *cmd, regmatch_t *subs)
{
	ubyte on = parse_flag(cmd, subs, 1, 0);
	char *class, *name = regsubstr(cmd, subs, 2); int i;
	FOR_STACK (class, unmanaged, char*, i)
	{
		if (strcmp(class, name) != 0) continue;
		free(class); stack_del(unmanaged, i);
	}
	if (!on && !is_unmanaged_class(name))
	{
		stack_push(unmanaged, name);
		say("%s is unmanaged", name);
		name = NULL;
	}
	else	say("%s is managed", name);
	free(name);
	return NULL;
}
char* com_screen_switch(char *cmd, regmatch_t *subs)
{
	char *num = regsubstr(cmd, subs, 1);
	if (strcmp(num, "(next)") == 0) head_next();
	else
	{
		ucell n = strtol(num, NULL, 10);
		head *h; int i; ubyte found = 0;
		FOR_RING (NEXT, h, heads, i)
		{
			if (h->id == n)
			{
				head_focus(h);
				found = 1;
				break;
			}
		}
		if (!found)
			um("invalid screen id %d", n);
	}
	free(num);
	return NULL;
}
char* com_group_stack(char *cmd, regmatch_t *subs)
{
	group *g = heads->groups;
	ubyte on = parse_flag(cmd, subs, 1, g->flags & GF_STACKING);
	if ((on && !(g->flags & GF_STACKING)) ||
		(!on && g->flags & GF_STACKING))
		group_stack();
	return NULL;
}
char* com_set(char *cmd, regmatch_t *subs)
{
	char *name = regsubstr(cmd, subs, 1);
	char *value = regsubstr(cmd, subs, 2);
	setting *s = hash_get(setting_hash, name);
	if (s && regmatch(s->check, value, 0, NULL, REG_EXTENDED|REG_ICASE) == 0)
	{
		switch (s->type)
		{
			case mst_str:
				free(s->s);
				s->s = value;
				say("set %s to %s", name, value);
				break;
			case mst_ucell:
				s->u = strtol(value, NULL, 10);
				say("set %s to %u", name, s->u);
				free(value);
				break;
			case mst_dcell:
				s->d = strtod(value, NULL);
				say("set %s to %f", name, s->d);
				free(value);
				break;
		}
		free(name);
		return NULL;
	}
	um("invalid setting '%s' to: %s", name, value);
	free(name); free(value);
	return NULL;
}
char* com_bind(char *cmd, regmatch_t *subs)
{
	ubyte flag = parse_flag(cmd, subs, 1, 0);
	char *pattern = regsubstr(cmd, subs, 2);
	if (flag)
	{
		char *command = regsubstr(cmd, subs, 3);
		if (command && *command)
		{
			strtrim(command);
			if (create_binding(pattern, command))
				say("bound %s to %s", pattern, command);
			else	um("could not bind %s", pattern);
		}
		free(command);
	} else
	{
		if (pattern && strcasecmp(pattern, "all") == 0)
		{
			while (bindings->depth)
				remove_binding(((binding*)bindings->items[0])->pattern);
			say("unbound all keys");
		} else
		{
			if (remove_binding(pattern))
				say("unbound %s", pattern);
			else	um("could not unbind %s", pattern);
		}
	}
	grab_stuff();
	free(pattern);
	return NULL;
}
char* com_switch(char *cmd, regmatch_t *subs)
{
	char *mode = regsubstr(cmd, subs, 1);
	if (strcasecmp(mode, "window") == 0) window_switch();
	else if (strcasecmp(mode, "group") == 0) group_switch();
	free(mode);
	return NULL;
}
char* com_command(char *cmd, regmatch_t *subs)
{
	int i; alias *a; autostr s; str_create(&s);
	str_print(&s, strlen(command_hints)+NOTE, "%s \n", command_hints);
	FOR_STACK (a, aliases, alias*, i) str_print(&s, NOTE, "%s \n", a->name);
	menu_wrapper(s.pad, settings[ms_run_musca_command].s);
	str_free(&s);
	return NULL;
}
char* com_shell(char *cmd, regmatch_t *subs)
{
        char *tmp = allocate(strlen(cmd)+strlen(settings[ms_run_shell_command].s)+BLOCK);
	sprintf(tmp, "dmenu_run | %s", settings[ms_run_shell_command].s);
	printf("%s\n", tmp);
	launch(tmp); free(tmp);
	return NULL;

	/* menu("dmenu_run", settings[ms_run_shell_command].s); */
	/* return NULL; */
}
char* com_alias(char *cmd, regmatch_t *subs)
{
	char *name    = regsubstr(cmd, subs, 1);
	char *command = regsubstr(cmd, subs, 2);
	ubyte r = create_alias(name, command);
	say("%s alias %s as %s", r == 1 ? "created": "replaced", name, command);
	// do not free name/command, as create_alias uses the pointers.
	return NULL;
}
char* com_say(char *cmd, regmatch_t *subs)
{
	char *msg = regsubstr(cmd, subs, 1);
	say("%s", msg); free(msg);
	return NULL;
}
char* com_run(char *cmd, regmatch_t *subs)
{
	char *file = regsubstr(cmd, subs, 1);
	if (!run_file(file)) um("could not run %s", file);
	free(file);
	return NULL;
}
char* show_unmanaged()
{
	int i; char *item;
	autostr s; str_create(&s);
	FOR_STACK (item, unmanaged, char*, i)
		str_print(&s, NOTE, "%s\n", item);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_bindings()
{
	autostr s; str_create(&s);
	int i; binding *b = NULL;
	FOR_STACK (b, bindings, binding*, i)
		str_print(&s, strlen(b->pattern) + strlen(b->command) + NOTE,
			"bind on %s %s\n", b->pattern, b->command);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_settings()
{
	autostr s; str_create(&s);
	int i; setting *setting = NULL;
	for (i = 0; i < ms_last; i++)
	{
		setting = &settings[i];
		str_print(&s, strlen(setting->name) + NOTE, "set %s ", setting->name);
		if (setting->type == mst_str)
			str_print(&s, strlen(setting->s), "%s", setting->s);
		else
		if (setting->type == mst_ucell)
			str_print(&s, NOTE, "%u", setting->u);
		else
		if (setting->type == mst_dcell)
			str_print(&s, NOTE, "%f", setting->d);
		str_push(&s, '\n');
	}
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_hooks()
{
	autostr s; str_create(&s);
	hook *h; int i;
	FOR_LIST (h, hooks, i)
		str_print(&s, strlen(h->pattern) + strlen(h->command) + NOTE,
			"hook on %s %s\n", h->pattern, h->command);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_groups()
{
	autostr s; str_create(&s);
	group *g, *f = group_first(heads); int i;
	FOR_RING(NEXT, g, f, i)
		str_print(&s, strlen(g->name) + 10, "%d %s %s\n",
			g->id, g == heads->groups ? "*": "-", g->name);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_frames()
{
	autostr s; str_create(&s);
	frame *f, *a = frame_first(heads->groups); int i;
	FOR_RING (NEXT, f, a, i)
		str_print(&s, NOTE, "%d %s %d %d %d %d %u\n",
			f->id, f == heads->groups->frames ? "*": "-", f->x, f->y, f->w, f->h, f->flags);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_windows()
{
	autostr s; str_create(&s);
	client *c, *f = client_first(heads->groups); int i;
	FOR_RING(NEXT, c, f, i)
		str_print(&s, strlen(c->name) + 10, "%d %s %s\n",
			c->id, c == heads->groups->clients ? "*": "-", c->name);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* show_aliases()
{
	autostr s; str_create(&s);
	alias *a; int i;
	FOR_STACK (a, aliases, alias*, i)
		str_print(&s, strlen(a->name)+strlen(a->command)+10, "alias %s %s\n",
			a->name, a->command);
	s.len = strrtrim(s.pad);
	return s.pad;
}
char* com_show(char *cmd, regmatch_t *subs)
{
	char *result = NULL;
	char *arg = regsubstr(cmd, subs, 1);
	if (strcasecmp(arg, "unmanaged") == 0)
		result = show_unmanaged();
	else if (strcasecmp(arg, "bindings") == 0)
		result = show_bindings();
	else if (strcasecmp(arg, "settings") == 0)
		result = show_settings();
	else if (strcasecmp(arg, "hooks") == 0)
		result = show_hooks();
	else if (strcasecmp(arg, "groups") == 0)
		result = show_groups();
	else if (strcasecmp(arg, "frames") == 0)
		result = show_frames();
	else if (strcasecmp(arg, "windows") == 0)
		result = show_windows();
	else if (strcasecmp(arg, "aliases") == 0)
		result = show_aliases();
	if (!strlen(result)) say("%s empty", arg);
	free(arg);
	return result;
}
char* com_hook(char *cmd, regmatch_t *subs)
{
	ubyte flag = parse_flag(cmd, subs, 1, 0);
	char *pattern = regsubstr(cmd, subs, 2);
	char *command = regsubstr(cmd, subs, 3);
	hook *h = hooks, *lh = h;
	while (h)
	{
		if (strcmp(pattern, h->pattern) == 0)
			break;
		lh = h; h = h->next;
	}
	if (flag)
	{
		if (h)
		{
			free(h->command);
			h->command = command;
			free(h->pattern);
		} else
		{
			h = allocate(sizeof(hook));
			h->pattern = pattern;
			h->command = command;
			if (regcomp(&h->re, pattern, REG_EXTENDED|REG_ICASE) != 0)
			{
				um("error compiling regex %s", pattern);
				regfree(&h->re); free(h); free(command); free(pattern);
			} else
			{
				h->next = hooks;
				hooks = h;
			}
		}
	} else
	{
		if (h)
		{
			free(h->command);
			free(h->pattern);
			if (h == hooks) hooks = h->next;
			else	lh->next = h->next;
			regfree(&h->re);
			free(h);
		}
		free(command);
		free(pattern);
	}
	return NULL;
}
char* com_client(char *cmd, regmatch_t *subs)
{
	client *c = heads->groups->clients;
	if (c)
	{
		ucell mask = 0;
		char *name = regsubstr(cmd, subs, 1);
		if (strcasecmp(name, "hints") == 0) mask = CF_HINTS;
		if (mask)
		{
			ubyte flag = parse_flag(cmd, subs, 2, c->flags & mask);
			if (flag) c->flags |= mask; else c->flags &= ~mask;
			frame_focus_client(c->frame, c);
			say("%s %s for %s", name, c->flags & mask ? "on": "off", c->name);
		}
		free(name);
	}
	return NULL;
}
char* com_place(char *cmd, regmatch_t *subs)
{
	placement *p; int i;
	char *class = regsubstr(cmd, subs, 1);
	ubyte flag = parse_flag(cmd, subs, 2, 0);
	char *group = regsubstr(cmd, subs, 3);
	FOR_STACK (p, placements, placement*, i)
		if (strcmp(p->class, class) == 0)
			break;
	if (flag && group && *group)
	{
		if (i == placements->depth)
		{
			p = allocate(sizeof(placement));
			stack_push(placements, p);
		}
		snprintf(p->class, NOTE, "%s", class);
		snprintf(p->group, NOTE, "%s", group);
		say("created placement rule for %s on %s", class, group);
	} else
	{
		if (i < placements->depth)
		{
			stack_discard(placements, p);
			free(p);
		}
		say("removed placement rule for %s", class);
	}
	free(class); free(group);
	return NULL;
}
char* com_debug(char *cmd, regmatch_t *subs)
{
	char *action = regsubstr(cmd, subs, 1);
	if (strcasecmp(action, "sanity") == 0)
		sanity_head(heads);
	return NULL;
}
char* com_quit(char *cmd, regmatch_t *subs)
{
	shutdown();
	return NULL;
}
ubyte run_file(char *file)
{
	// should check file exists, but an empty stdin won't hurt musca -i
	char tmp[NOTE];
	snprintf(tmp, NOTE, "cat %s | $MUSCA -i", file);
	exec_cmd(tmp);
	return 1;
}
char* musca_command(char *cmd_orig)
{
	int i;
	char *result = NULL, *pattern = NULL, *cmd_copy = strdup(cmd_orig), *cmd = cmd_copy;
	group *g = heads->groups; regmatch_t subs[10]; ubyte old_silent = silent;
	int matches = 0, len = strtrim(cmd);
	if (len)
	{
		if (strncasecmp("silent ", cmd, 7) == 0)
		{
			cmd += 7;
			silent = 1;
		}
		strskip(&cmd, isspace); char *first = cmd;
		char *name = strnext(&first, isspace);
		command *c = hash_get(command_hash, name);
		if (c)
		{
			matches++;
			stack *patterns = strsplitthese(c->pattern, "\n");
			FOR_STACK (pattern, patterns, char*, i)
			{
				if (regmatch(pattern, cmd, 10, subs, REG_EXTENDED|REG_ICASE) == 0)
				{
					if (c->flags & g->flags) result = (c->func)(cmd, subs);
					else um("command invalid for %s mode: %s", g->flags & GF_TILING ? "tiling": "stacking", cmd);
					break;
				}
			}
			FOR_STACK (pattern, patterns, char*, i)
				free(pattern);
			stack_free(patterns);
		}
		free(name);
	}
	if (!matches && strlen(cmd) && !try_alias(cmd))
		um("could not execute: %s", cmd);
	else
		try_hook(cmd);
	ewmh_clients();
	ewmh_groups();
	if (result && *result) say("%s", result);
	silent = old_silent;
	free(cmd_copy);
	return result;
}
char* musca_commands(char *content)
{
	int i; char *line; autostr s; str_create(&s);
	stack *lines = strsplitthese(content, "\n");
	FOR_STACK (line, lines, char*, i)
	{
		if (strtrim(line) && *line != '#')
		{
			char *result = musca_command(line);
			if (result)
			{
				str_print(&s, strlen(result)+1, "%s\n", result);
				free(result);
			}
		}
		free(line);
	}
	stack_free(lines);
	s.len = strrtrim(s.pad);
	return s.pad;
}
void window_switch()
{
	char *list = show_windows();
	menu_wrapper(list, settings[ms_switch_window].s);
	free(list);
}
void group_switch()
{
	char *list = show_groups();
	menu_wrapper(list, settings[ms_switch_group].s);
	free(list);
}
client* manage(Window win, XWindowAttributes *attr)
{
	client *c, *p;
	head *h = head_by_screen(attr->screen);
	Window trans = None;
	// transients use parent's frame even if it is dedicated
	if (XGetTransientForHint(display, win, &trans) && (p = client_by_window(trans)))
	{
		c = client_create(p->group, p->frame ? p->frame : p->group->frames, win);
		c->parent = p; p->kids++;
	} else
	{
		group *cg = h->groups;
		frame *cf = NULL;
		ubyte *data = NULL; ucell len;
		placement *p; int i;
		char class[NOTE]; window_class(win, class);
		FOR_STACK (p, placements, placement*, i)
			if (strcmp(p->class, class) == 0) break;
		if (i < placements->depth
			&& (cg = group_auto_create(h, p->group)) != NULL);
		else
		// if we launched this app, and if it uses _NET_WM_PID, check to see
		// if it was launched on a different group
		if (atom_get(NetWMPid, win, &data, &len) && data && len)
		{
			pid_t pid = *((pid_t*)data);
			time_t limit = time(0) - EXEC_DELAY;
			int i; struct exec_marker *em;
			FOR_ARRAY (em, exec_markers, struct exec_marker, i)
			{
				if (em->pid == pid)
				{
					if (is_valid_group(em->group)) cg = em->group;
					if (is_valid_frame(em->frame) && em->frame->group == cg) cf = em->frame;
					em->pid = 0;
					break;
				}
			}
			// expire old exec_markers entries
			FOR_ARRAY (em, exec_markers, struct exec_marker, i)
				if (em->time < limit) em->pid = 0;
		}
		if (data) free(data);
		if (!cf) cf = cg->frames;
		c = client_create(cg, frame_available(cf), win);
	}
	XSelectInput(display, win, PropertyChangeMask | EnterWindowMask | LeaveWindowMask);
	atom_set(MuscaType, win, XA_STRING, 8, "client", 7);
	if (c->frame->group == c->frame->group->head->groups)
	{
		if (c->frame == heads->groups->frames || settings[ms_window_open_focus].u)
			frame_focus_client(c->frame, c);
		else	client_display(c, c->frame);
		frames_display_hidden(c->group);
	} else
		// client is appearing on a hidden group.  attach to frame, but do nothing else
		// as it will all happen in a later group_focus()
		frame_target_client(c->frame, c);
	ewmh_clients();
	return c;
}
winstate* quiz_window(Window win)
{
	winstate *ws = allocate(sizeof(winstate));
	ws->w = win; ws->c = NULL; ws->f = NULL; ws->manage = 0;
	ws->name[0] = '\0'; ws->class[0] = '\0';
	ws->type = None; ws->state = 0;
	if ((ws->ok = XGetWindowAttributes(display, win, &ws->attr)))
	{
		XWMHints *hints = XGetWMHints(display, win);
		ws->input = hints && hints->flags & InputHint ? hints->input: True;
		XFree(hints);
		ws->c = client_by_window(win);
		if (!ws->c) ws->f = is_frame_background(win);
		if (ws->c)
		{
			strcpy(ws->name,  ws->c->name);
			strcpy(ws->class, ws->c->class);
			strcpy(ws->role,  ws->c->role);
			ws->c->netwmstate = ws->state;
			ws->c->input = ws->input;
		} else
		{
			window_name(ws->w,  ws->name);
			window_class(ws->w, ws->class);
			window_role(ws->w, ws->role);
		}
		ws->type  = window_type(win);
		ws->state = window_state(win);
		ws->manage = (
			(ws->type != None
				&& ws->type != atoms[NetWMWindowTypeNormal]
				&& ws->type != atoms[NetWMWindowTypeUtility]
				&& ws->type != atoms[NetWMWindowTypeToolbar]
				&& ws->type != atoms[NetWMWindowTypeMenu]
				&& ws->type != atoms[NetWMWindowTypeDialog])
			|| is_netwmstate(ws->state, NetWMStateFullscreen)
			|| is_netwmstate(ws->state, NetWMStateHidden)
			|| is_unmanaged_window(win)) ? 0:1;
		note("%s %s %s %s %s %s %x %x", ws->class, ws->name, ws->manage ? "manage": "ignore",
			ws->input ? "focus": "nofocus", ws->c ? "client": "unknown",
			ws->attr.override_redirect ? "override" : "normal", ws->state, ws->type);
	}
	return ws;
}
void createnotify(XEvent *ev)
{
	winstate *ws = quiz_window(ev->xcreatewindow.window); WINDOW_EVENT(ws);
	if (ws->ok)
	{
		if (!ws->attr.override_redirect)
			XSelectInput(display, ws->w, PropertyChangeMask | EnterWindowMask | LeaveWindowMask);
		if (strcmp(MUSCA_CLASS, ws->class) == 0)
			atom_set_string(MuscaReady, ws->w, "ok");
	}
	free(ws);
}
void configurerequest(XEvent *ev)
{
	client *p;
	Window trans = None;
	XConfigureRequestEvent *cr = &ev->xconfigurerequest;
	winstate *ws = quiz_window(cr->window); WINDOW_EVENT(ws);
	if (ws->ok && !ws->attr.override_redirect)
	{
		if (ws->c)
		{
			if ((XGetTransientForHint(display, ws->w, &trans) && (p = client_by_window(trans)))
				|| (ws->c->group != ws->c->group->head->groups))
				client_configure(ws->c, cr);
			else	client_refresh(ws->c);
		} else
		{
			XWindowChanges wc;
			if (is_netwmstate(ws->state, NetWMStateFullscreen))
			{
				wc.x = 0; wc.y = 0;
				wc.width = heads->screen->width;
				wc.height = heads->screen->height;
				wc.border_width = 0;
				wc.sibling = Above;
			} else
			{
				wc.x = cr->x; wc.y = cr->y;
				wc.width = MAX(1, cr->width);
				wc.height = MAX(1, cr->height);
				wc.border_width = cr->border_width;
				wc.sibling = cr->above;
			}
			wc.stack_mode = cr->detail;
			XConfigureWindow(display, cr->window, cr->value_mask, &wc);
		}
	}
	free(ws);
}
void configurenotify(XEvent *ev)
{
	XConfigureEvent *cn = &ev->xconfigure;
	winstate *ws = quiz_window(cn->window); WINDOW_EVENT(ws);
	if (ws->ok && !ws->attr.override_redirect)
	{
		if (!ws->c && !ws->f)
		{
			head *h = head_by_root(ws->w);
			if (h && (h->screen->width != cn->width || h->screen->height != cn->height))
			{
				note("screen %d %d %d", h->id, cn->width, cn->height);
				h->screen->width = cn->width;
				h->screen->height = cn->height;
				group *g; int j;
				FOR_RING (NEXT, g, h->groups, j)
					frame_single(g->frames);
				if (h == heads)
				{
					frame_update(h->groups->frames);
					frame_focus(h->groups->frames);
				}
			}
		}
	}
	free(ws);
}
client* handle_map(winstate *ws)
{
	client *c = NULL;
	if (ws->ok && !ws->f && !ws->c)
	{
		head *hd = head_by_root(ws->attr.root);
		if (!ws->attr.override_redirect && !ws->c && ws->manage)
			c = manage(ws->w, &ws->attr);
		else
		{
			int x, y, w, h;
			if (!window_on_screen(hd, ws->w, &ws->attr, &x, &y, &w, &h))
				XMoveResizeWindow(display, ws->w, x, y, w, h);
		}
	}
	return c;
}
void maprequest(XEvent *ev)
{
	winstate *ws = quiz_window(ev->xmaprequest.window); WINDOW_EVENT(ws);
	client *c = handle_map(ws);
	if (c)
	{
		free(ws);
		ws = quiz_window(c->win);
	}
	if (ws->ok)
	{
		if (ws->c && ws->c->group != ws->c->group->head->groups)
			say("map attempt on group %s", ws->c->group->name);
		else XMapWindow(display, ws->w);
	}
	free(ws);
}
void mapnotify(XEvent *ev)
{
	winstate *ws = quiz_window(ev->xmap.window); WINDOW_EVENT(ws);
	if (ws->ok)
	{
		handle_map(ws);
		head *h = head_by_root(ws->attr.root);
		if (is_netwmstate(ws->state, NetWMStateFullscreen))
			stack_push(h->fullscreen, (void*)ws);
		else
		if (ws->attr.override_redirect || is_netwmstate(ws->state, NetWMStateAbove))
			stack_push(h->above, (void*)ws->w);
	}
	free(ws);
}
void unmapnotify(XEvent *ev)
{
	winstate *ws = quiz_window(ev->xunmap.window); WINDOW_EVENT(ws);
	if (ws->ok && !ws->attr.override_redirect)
	{
		// did we *not* tell it to unmap during group switch?  if so, this is a voluntary
		// unmap (perhaps minimizing to a sys tray or similar) so wish it bon voyage and forget.
		if (ws->c)
		{
			ws->c->unmaps--;
			if (ws->c->unmaps < 0)
				client_remove(ws->c);
		}
	}
	window_discard_references(ws->w);
	free(ws);
}
void mappingnotify(XEvent *ev)
{
	XMappingEvent *me = &ev->xmapping;
	if (me->request == MappingKeyboard || me->request == MappingModifier)
	{
		XRefreshKeyboardMapping(me);
		grab_stuff();
	}
}
void destroynotify(XEvent *ev)
{
	Window win = ev->xdestroywindow.window;
	client *c = client_by_window(win);
	if (c) client_remove(c);
	window_discard_references(win);
}
void enternotify(XEvent *ev)
{
	while (XCheckTypedEvent(display, EnterNotify, ev));
	winstate *ws = NULL;
	XCrossingEvent *ce = &ev->xcrossing;
	Window win = ce->window;
	if (!mouse_grabbed && settings[ms_focus_follow_mouse].u && ce->mode == NotifyNormal
		&& !head_by_root(win) && (ws = quiz_window(win)) && ws->ok)
	{
		WINDOW_EVENT(ws);
		if (ws->c) frame_refocus_client(ws->c->frame, ws->c);
		else if (ws->f) frame_focus(ws->f);
	}
	if (ws) free(ws);
}
void keypress(XEvent *ev)
{
	XKeyEvent *key = &ev->xkey;
	binding *kb = find_binding(key->state & ~(NumlockMask | LockMask), key->keycode);
	if (kb) free(musca_command(kb->command));
	else note("unknown XKeyEvent %u %d", key->state, key->keycode);
}
void buttonpress(XEvent *ev)
{
	// *any* mouse button press, including rolling the wheel, arrives here and will cause
	// a frame and client focus change
	while (XCheckTypedEvent(display, ButtonPress, ev));
	XButtonEvent *button = &ev->xbutton; frame *f;
	if (button->subwindow != None)
	{
		client *c = NULL;
		if (!(f = is_frame_background(button->subwindow)))
		{
			c = client_by_window(button->subwindow);
			if (c) f = c->frame;
		}
		if (f)
		{
			frame_focus(f);
			// start drag for resize or move in stack mode
			if (c && c->group->flags & GF_STACKING && c)
			{
				client_refresh(c);
				client_focus(c, f);
				if (button->state & modifier_names_to_mask(settings[ms_stack_mouse_modifier].s))
				{
					ucell bw = MAX(settings[ms_border_width].u, 1);
					c->rt = window_create(heads->screen->root, c->x, c->y, c->w+bw, bw, 0, NULL, settings[ms_border_focus].s, MUSCA_CLASS, MUSCA_CLASS);
					c->rl = window_create(heads->screen->root, c->x, c->y, bw, c->h+bw, 0, NULL, settings[ms_border_focus].s, MUSCA_CLASS, MUSCA_CLASS);
					c->rr = window_create(heads->screen->root, c->x + c->w+bw, c->y, bw, c->h+bw+bw, 0, NULL, settings[ms_border_focus].s, MUSCA_CLASS, MUSCA_CLASS);
					c->rb = window_create(heads->screen->root, c->x, c->y + c->h+bw, c->w+bw+bw, bw, 0, NULL, settings[ms_border_focus].s, MUSCA_CLASS, MUSCA_CLASS);
					XMapWindow(display, c->rl); XMapWindow(display, c->rt); XMapWindow(display, c->rr); XMapWindow(display, c->rb);
					XRaiseWindow(display, c->rl); XRaiseWindow(display, c->rt); XRaiseWindow(display, c->rr); XRaiseWindow(display, c->rb);

					XGrabPointer(display, c->rl, True, PointerMotionMask|ButtonReleaseMask,
						GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
					c->mxr = button->x_root; c->myr = button->y_root; c->mb = button->button;
					c->mx = c->x; c->my = c->y; c->mw = c->w; c->mh = c->h; mouse_grabbed = 1;
				}
			}
		} else
		{
			head *h = head_by_root(button->root);
			if (h != heads) head_focus(h);
		}
	}
	XAllowEvents(display, ReplayPointer, CurrentTime);
}
void buttonrelease(XEvent *ev)
{
	XButtonEvent *button = &ev->xbutton;
	client *c = client_by_resize_window(button->window);
	if (c)
	{
		XMoveResizeWindow(display, c->win, c->x, c->y, c->w, c->h);
		XUngrabPointer(display, CurrentTime);
		XDestroyWindow(display, c->rl); XDestroyWindow(display, c->rt); XDestroyWindow(display, c->rr); XDestroyWindow(display, c->rb);
		c->rl = 0; c->rr = 0; c->rt = 0; c->rb = 0; mouse_grabbed = 0;
		client_configure(c, NULL);
	}
}
void motionnotify(XEvent *ev)
{
	// we should only get these if we've grabbed the pointer
	while (XCheckTypedEvent(display, MotionNotify, ev));
	XMotionEvent *motion = &ev->xmotion;
	XButtonEvent *button = &ev->xbutton;
	if (motion->same_screen)
	{
		client *c = client_by_resize_window(motion->window);
		if (c)
		{
			int dx = MAX(c->x, button->x_root) - c->mxr, dy = MAX(c->y, button->y_root) - c->myr;
			c->x = c->mx + (c->mb == Button1 ? dx: 0); c->y = c->my + (c->mb == Button1 ? dy: 0);
			// should limit maximum size?
			ucell bw = MAX(settings[ms_border_width].u, 1);
			c->w = MAX(settings[ms_frame_min_wh].u, c->mw + (c->mb == Button3 ? dx : 0));
			c->h = MAX(settings[ms_frame_min_wh].u, c->mh + (c->mb == Button3 ? dy : 0));
			XMoveResizeWindow(display, c->rt, c->x, c->y, c->w+bw, bw);
			XMoveResizeWindow(display, c->rl, c->x, c->y, bw, c->h+bw);
			XMoveResizeWindow(display, c->rr, c->x + c->w+bw, c->y, bw, c->h+bw+bw);
			XMoveResizeWindow(display, c->rb, c->x, c->y + c->h+bw, c->w+bw+bw, bw);
			c->fx = c->x; c->fy = c->y; c->fw = c->w; c->fh = c->h;
		}
	}
}
void propertynotify(XEvent *ev)
{
	XPropertyEvent *pe = &ev->xproperty;
	if (pe->atom == XA_WM_NAME || pe->atom == atoms[NetWMName])
	{
		client *c = client_by_window(pe->window);
		if (c) window_name(c->win, c->name);
	} else
	if (pe->atom == atoms[MuscaCommand])
	{
		char *cmd = NULL; ucell len = 0; char *res = NULL;
		atom_get_string(MuscaCommand, pe->window, &cmd, &len);
		note("received: %s", (char*)cmd);
		if (strtrim(cmd)) res = musca_commands(cmd);
		atom_set(MuscaResult, pe->window, XA_STRING, 8,
			res ? res: "", res ? strlen(res)+1: 1);
		free(res); free(cmd);
	}
}
void clientmessage(XEvent *ev)
{
	// we convert EWMH client messages to musca commands to properly trigger hooks
	char cmd[NOTE];
	XClientMessageEvent *cm = &ev->xclient;
	winstate *ws = quiz_window(cm->window); WINDOW_EVENT(ws);
	if (ws->ok)
	{
		if (cm->message_type == atoms[NetActiveWindow] && ws->c
			&& ws->c->group == ws->c->group->head->groups)
		{
			snprintf(cmd, NOTE, command_formats[cmd_raise], ws->c->id);
			musca_command(cmd);
		}
		else
		if (cm->message_type == atoms[NetCurrentDesktop])
		{
			snprintf(cmd, NOTE, command_formats[cmd_use], (int)cm->data.l[0]);
			musca_command(cmd);
		}
		else
		if (cm->message_type == atoms[NetCloseWindow] && ws->c)
		{
			snprintf(cmd, NOTE, command_formats[cmd_kill], ws->c->id);
			musca_command(cmd);
		}
		else
		if (cm->message_type == atoms[NetNumberOfDesktops])
		{
			group *g; int i; FOR_RING (NEXT, g, heads->groups, i);
			for (; cm->data.l[0] < i; i--)
			{
				snprintf(cmd, NOTE, command_formats[cmd_drop], heads->groups->id);
				musca_command(cmd);
			}
			for (; cm->data.l[0] > i; i++)
			{
				snprintf(cmd, NOTE, command_formats[cmd_add], "untitled");
				musca_command(cmd);
			}
		}
		else
		if (cm->message_type == atoms[WMChangeState] && ws->c)
		{
			snprintf(cmd, NOTE, ws->c->flags & CF_SHRUNK ?
				command_formats[cmd_raise]: command_formats[cmd_shrink], ws->c->id);
			musca_command(cmd);
		}
	}
	free(ws);
}
void focusin(XEvent *ev)
{
}
void focusout(XEvent *ev)
{
}
void leavenotify(XEvent *ev)
{
}
void timeout(int sig)
{
	um("timed out");
	exit(EXIT_FAILURE);
}
// thanks to ratpoison for the -c command communications idea and basic mechanism
ubyte insert_command(char* cmd)
{
	char *result = NULL; ucell len;
	if (!cmd)
	{
		autostr s; str_create(&s);
		for (;;)
		{
			char c = getchar();
			if (c == EOF) break;
			str_push(&s, c);
		}
		cmd = s.pad;
	}
	if (!strlen(cmd)) return 0;
	Window win = window_create(DefaultRootWindow(display), -1, -1, 1, 1, 0,
		None, None, MUSCA_CLASS, MUSCA_CLASS);
	XSelectInput (display, win, PropertyChangeMask);
	// wait for main wm process to set MuscaReady on our window
	signal(SIGALRM, timeout); alarm(5);
	for (;;)
	{
		XEvent ev;
		XMaskEvent (display, PropertyChangeMask, &ev);
		if (ev.xproperty.atom == atoms[MuscaReady]
	          && ev.xproperty.state == PropertyNewValue)
			break;
		usleep(10000);
	}
	// send command
	atom_set_string(MuscaCommand, win, cmd);
	// wait for main wm process to set MuscaResult on our window
	signal(SIGALRM, timeout); alarm(5);
	for (;;)
	{
		XEvent ev;
		XMaskEvent (display, PropertyChangeMask, &ev);
		if (ev.xproperty.atom == atoms[MuscaResult]
	          && ev.xproperty.state == PropertyNewValue)
		{
			atom_get_string(MuscaResult, win, &result, &len);
			break;
		}
		usleep(10000);
	}
	XDestroyWindow(display, win);
	if (result && strlen((char*)result))
		printf("%s\n", (char*)result);
	free(result);
	return 1;
}
void find_window(Window w, ubyte transient)
{
	Window trans;
	char *tmp = NULL; ucell len = 0;
	winstate *ws = quiz_window(w);
	if (ws->ok && !ws->attr.override_redirect && !ws->c && ws->manage)
	{
		if (!transient && XGetTransientForHint(display, w, &trans))
			return;
		if (ws->attr.map_state == IsUnmapped)
		{
			atom_get_string(MuscaType, w, &tmp, &len);
			if (tmp && strcmp(tmp, "client") == 0)
			{
				XMapWindow(display, w);
				manage(w, &ws->attr);
			}
		} else
		if (ws->attr.map_state == IsViewable)
			manage(w, &ws->attr);
	}
	free(ws); free(tmp);
}
void find_clients(head *h)
{
	Window d1, d2, *wins = NULL; ucell num; int i;
	// find any unmanaged running clients and manage them
	if (XQueryTree(display, h->screen->root, &d1, &d2, &wins, &num))
	{
		// normals first
		for (i = 0; i < num; i++) find_window(wins[i], 0);
		for (i = 0; i < num; i++) find_window(wins[i], 1);
		if(wins) XFree(wins);
	}
}
void ungrab_stuff()
{
	head *h; int i;
	FOR_RING (NEXT, h, heads, i)
	{
		XUngrabKey(display, AnyKey, AnyModifier, h->screen->root);
		XUngrabButton(display, AnyButton, AnyModifier, h->screen->root);
	}
}
void grab_stuff()
{
	int i, j, k; head *h; binding *b; ucell *m;
	refresh_bindings();
	ungrab_stuff();
	ucell modifiers[] = { 0, LockMask, NumlockMask, LockMask|NumlockMask };
	FOR_RING (NEXT, h, heads, i)
	{
		FOR_STACK (b, bindings, binding*, j)
		{
			FOR_ARRAY (m, modifiers, ucell, k)
				XGrabKey(display, b->key, b->mod | *m, h->screen->root,
					True, GrabModeAsync, GrabModeAsync);
		}
		XGrabButton(display, AnyButton, AnyModifier, h->screen->root,
			True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
	}
}
void setup_regex()
{
	int i;
	command_hash = hash_create();
	command *c; autostr str; str_create(&str);
	FOR_ARRAY (c, commands, command, i)
	{
		char *keys = c->keys;
		while (keys && *keys)
		{
			char *tok = strnextthese(&keys, ",");
			hash_set(command_hash, tok, c);
			str_print(&str, NOTE, "%s \n", tok);
			free(tok); strskipthese(&keys, ",");
		}
	}
	command_hints = str.pad;
	strtrim(command_hints);
	// compile setting regex
	setting_hash = hash_create();
	setting *s;
	FOR_ARRAY (s, settings, setting, i)
		hash_set(setting_hash, s->name, s);
	// compile modmask regex
	modmask *m;
	FOR_ARRAY (m, modmasks, modmask, i)
		assert(regcomp(&m->re, m->pattern, REG_EXTENDED|REG_ICASE) == 0,
			"could not compile modmask regex: %s", m->pattern);
}
void setup_numlock()
{
	int i, j;
	// init NumlockMask (thanks to Nikita Kanounnikov)
	XModifierKeymap *modmap;
	modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < modmap->max_keypermod; j++)
		{
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(display, XK_Num_Lock))
			{
				NumlockMask = (1 << i);
				break;
			}
		}
	}
	XFreeModifiermap(modmap);
}
void setup_bindings()
{
	int i; keymap *k;
	// prepare key bindings
	bindings = stack_create();
	FOR_ARRAY (k, keymaps, keymap, i)
		create_binding(k->pattern, k->command);
}
void setup_unmanaged()
{
	int i; char **class;
	// initialize unmanaged stack from unmanaged_windows[]
	unmanaged = stack_create();
	FOR_ARRAY (class, unmanaged_windows, char*, i)
		stack_push(unmanaged, strdup(*class));
}
void setup(int argc, char **argv)
{
	int i; head *f, *p, *h; heads = NULL; spoke = 0; sanity = 0;
	mouse_grabbed = 0; silent = 0; hooks = NULL; exec_pointer = 0;
	xerrorxlib = XSetErrorHandler(error_callback);
	assert((display = XOpenDisplay(0x0)), "cannot open display");
	// some framework
	self = allocate(NOTE); sprintf(self, "MUSCA=%s", argv[0]);
	assert(putenv(self) == 0, "cannot set $MUSCA environment variable");
	for (i = 0; i < ms_last; i++)
		if (settings[i].type == mst_str)
			settings[i].s = strdup(settings[i].s);
	for (i = 0; i < AtomLast; i++)
	{
		atoms[i] = XInternAtom(display, atom_names[i], False);
		note("%s %d", atom_names[i], atoms[i]);
	}
	for (i = 0; i < EXEC_MARKERS; i++)
		exec_markers[i].pid = 0;
	setup_regex();
	// process args
	char *arg = NULL;
	arguments = args_to_hash(argc, argv);
	if ((arg = hash_get(arguments, "s")) != NULL)
	{
		free(settings[ms_startup].s);
		settings[ms_startup].s = strdup(arg);
	}
	if ((arg = hash_get(arguments, "c")) != NULL)
	{
		exit(insert_command(arg) ? EXIT_SUCCESS: EXIT_FAILURE);
	}
	if (hash_find(arguments, "i"))
	{
		exit(insert_command(NULL) ? EXIT_SUCCESS: EXIT_FAILURE);
	}
	if (hash_find_one(arguments, "v,version"))
	{
		printf("%s\n", VERSION);
		exit(EXIT_SUCCESS);
	}
	setup_numlock();
	setup_bindings();
	setup_unmanaged();
	// aliases
	aliases = stack_create();
	placements = stack_create();
	// intialize all heads
	f = NULL; p = NULL;
	for (i = 0; i < ScreenCount(display); i++)
	{
		h = allocate(sizeof(head)); h->id = i;
		h->screen = XScreenOfDisplay(display, i);
		h->display_string = NULL;
		h->prev = p; h->next = f;
		h->groups = NULL;
		h->stacked = stack_create();

		h->above = stack_create();
		h->below = stack_create();
		h->fullscreen = stack_create();

		if (!heads && DefaultScreen(display) == i) heads = h;
		group_create(h, "default", 0, 0, h->screen->width, h->screen->height);

		if (p) p->next = h;
		p = h; if (!f) f = h;

		XSelectInput(display, h->screen->root, SubstructureRedirectMask | SubstructureNotifyMask
			| StructureNotifyMask | FocusChangeMask);
		XDefineCursor(display, h->screen->root, XCreateFontCursor(display, XC_left_ptr));
		atom_set(NetSupported, h->screen->root, XA_ATOM, 32, atoms, AtomLast);

		h->ewmh = window_create(h->screen->root, -1, -1, 1, 1, 0, None, None, MUSCA_CLASS, MUSCA_CLASS);
		atom_set(NetSupportingWMCheck, h->screen->root, XA_WINDOW, 32, &h->ewmh, 1);
		atom_set(NetSupportingWMCheck, h->ewmh, XA_WINDOW, 32, &h->ewmh, 1);
		atom_set(NetWMName, h->ewmh, XA_STRING, 8, MUSCA_CLASS, 6);
	}
	f->prev = p; p->next = f;
	// none of below can be moved above as it requires the head linked list ring to be intact,
	// which it isn't until just now :-)
	h = heads;
	do {
		group_focus(heads->groups);
		char *ds = allocate(NOTE);
		sprintf(ds, "DISPLAY=%s", DisplayString(display));
		if (strrchr(DisplayString(display), ':'))
		{
			char *dot = strrchr(ds, '.');
			if (dot) sprintf(dot, ".%i", heads->id);
		}
		heads->display_string = ds;
		find_clients(heads);
		head_next();
	} while (heads != h);
	group_focus(heads->groups);
	FOR_RING (NEXT, h, heads, i)
	{
		if (XGrabKeyboard(display, h->screen->root, False, GrabModeSync, GrabModeSync, CurrentTime) != 0)
			crap("Could not temporarily grab keyboard. Something might be blocking key strokes.");
		XUngrabKeyboard(display, CurrentTime);
	}
	ewmh_groups();
	ewmh_clients();
	grab_stuff();
	//sanity_heads();
	// process startup file asyncronously so main thread can respond to events
	run_file(settings[ms_startup].s);
}
/**/void (*handler[LASTEvent])(XEvent*) = {
	[KeyPress] = keypress,
//	[KeyRelease] = keyrelease,
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[MotionNotify] = motionnotify,
	[EnterNotify] = enternotify,
	[LeaveNotify] = leavenotify,
	[FocusIn] = focusin,
	[FocusOut] = focusout,
//	[KeymapNotify] = keymapnotify,
//	[Expose] = expose,
//	[GraphicsExpose] = graphicsexpose,
//	[NoExpose] = noexpose,
//	[VisibilityNotify] = visibilitynotify,
	[CreateNotify] = createnotify,
	[DestroyNotify] = destroynotify,
	[UnmapNotify] = unmapnotify,
	[MapNotify] = mapnotify,
	[MapRequest] = maprequest,
//	[ReparentNotify] = reparentnotify,
	[ConfigureNotify] = configurenotify,
	[ConfigureRequest] = configurerequest,
//	[GravityNotify] = gravitynotify,
//	[ResizeRequest] = resizerequest,
//	[CirculateNotify] = circulatenotify,
//	[CirculateRequest] = circulaterequest,
	[PropertyNotify] = propertynotify,
//	[SelectionClear] = selectionclear,
//	[SelectionRequest] = selectionrequest,
//	[SelectionNotify] = selectionnotify,
//	[ColormapNotify] = colormapnotify,
	[ClientMessage] = clientmessage,
	[MappingNotify] = mappingnotify,
};
void process_event(XEvent *ev)
{
	if (handler[ev->type])
		handler[ev->type](ev);
	XFlush(display);
}
int main(int argc, char **argv)
{
	XEvent ev;
	try (oops)
	{
		setup(argc, argv);
		for(;;)
		{
			if (sanity)
			{
				//sanity_heads();
				sanity = 0;
			}
			XNextEvent(display, &ev);
			process_event(&ev);
		}
	} catch(oops);
	exit(oops.code ? EXIT_FAILURE: EXIT_SUCCESS);
}
