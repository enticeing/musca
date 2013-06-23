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
#include "apis.h"

// session
void session_set(char *key, char *val, bool copy)
{
	char *s = hash_get(session, key);
	free(s);
	hash_set(session, key, copy ? strdup(val): val);
	s = hash_encode(session);
	XAtomSetString(display, ApisSession, root, s);
	free(s);
}
char* session_get(char *key, char *def)
{
	char *val = hash_get(session, key);
	return (val) ? val: def;
}
// boxes
void box_init(box *b, int x, int y, int w, int h)
{
	b->x = x; b->y = y; b->w = w; b->h = h;
	b->l = None; b->r = None; b->t = None; b->b = None;
}
void box_free(box *b)
{
	if (b->l) XDestroyWindow(display, b->l);
	if (b->r) XDestroyWindow(display, b->r);
	if (b->t) XDestroyWindow(display, b->t);
	if (b->b) XDestroyWindow(display, b->b);
	box_init(b, 0, 0, 0, 0);
}
ubyte box_adjacent(box *s, int x, int y, int w, int h)
{
	ubyte on_left   = (s->x == x + w);
	ubyte on_right  = (s->x + s->w == x);
	ubyte on_top    = (s->y == y + h);
	ubyte on_bottom = (s->y + s->h == y);

	ubyte overlap_y = !((s->y < y && s->y + s->h < y) || (s->y > y + h && s->y + s->h > y + h));
	ubyte overlap_x = !((s->x < x && s->x + s->w < x) || (s->x > x + w && s->x + s->w > x + w));

	if (on_left   && overlap_y) return LEFT;
	if (on_right  && overlap_y) return RIGHT;
	if (on_top    && overlap_x) return TOP;
	if (on_bottom && overlap_x) return BOTTOM;
	return 0;
}
ubyte box_adjacent_within(box *s, int x, int y, int w, int h)
{
	ubyte in_y = (s->y >= y && s->y + s->h <= y + h);
	ubyte in_x = (s->x >= x && s->x + s->w <= x + w);
	ubyte side = box_adjacent(s, x, y, w, h);
	if (side)
	{
		if (side == LEFT   && in_y) return LEFT;
		if (side == RIGHT  && in_y) return RIGHT;
		if (side == TOP    && in_x) return TOP;
		if (side == BOTTOM && in_x) return BOTTOM;
	}
	return 0;
}
ubyte nearest_side(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
	bx = MAX(ax, bx); by = MAX(ay, by); bw = MIN(aw-(bx-ax), bw); bh = MIN(ah-(by-ay), bh);
	int sides[5], i, max, mirror[5];
	sides[LEFT] = bx-ax; sides[RIGHT] = (ax+aw)-(bx+bw);
	sides[TOP] = by-ay; sides[BOTTOM] = (ay+ah)-(by+bh);
	mirror[LEFT] = RIGHT; mirror[RIGHT] = LEFT; mirror[TOP] = BOTTOM; mirror[BOTTOM] = TOP;
	max = MAX(sides[LEFT], MAX(sides[RIGHT], MAX(sides[TOP], sides[BOTTOM])));
	for (i = LEFT; i < BOTTOM+1; i++) if (sides[i] == max) return mirror[i];
	return LEFT;
}
bool region_overlap_y(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
	return ((ay <= by && (ay + ah) > by) || (by <= ay && (by + bh) > ay)) ? 1:0;
}
bool region_overlap_x(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
	return ((ax <= bx && (ax + aw) > bx) || (bx <= ax && (bx + bw) > ax)) ? 1:0;
}
bool box_intersect(box *s, int x, int y, int width, int height)
{
	bool overlap_x = region_overlap_x(s->x, s->y, s->w, s->h, x, y, width, height);
	bool overlap_y = region_overlap_y(s->x, s->y, s->w, s->h, x, y, width, height);
	return overlap_x && overlap_y ? 1:0;
}
char* grid_dump(grid *g)
{
	return mem2str(g, sizeof(grid));
}
void grid_load(grid *g, char *desc)
{
	str2mem(g, sizeof(grid), desc);
}
void grid_snapshot(grid *g, int id)
{
	char key[NOTE]; sprintf(key, "grid%d", id);
	session_set(key, grid_dump(g), 0);
}
grid* grid_create(int w, int h)
{
	grid *g = allocate(sizeof(grid));
	memset(g, 0, sizeof(grid));
	g->w = w; g->h = h; g->count = 0;
	box_init(&g->boxes[g->count++], 0, 0, w, h);
	g->flags = 0; g->active = 0;
	return g;
}
void grid_free(grid *g)
{
	box *b; int i;
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
		box_free(b);
	free(g);
}
grid *grid_copy(grid *g)
{
	grid *cpy = allocate(sizeof(grid));
	memmove(cpy, g, sizeof(grid));
	return cpy;
}
void grid_make_gap(grid *g, int x, int y, int w, int h, box *except)
{
	int i; box *o;
	FOR_ARRAY_PART (o, g->boxes, box, i, g->count)
	{
		if (o != except && box_intersect(o, x+1, y+1, w-2, h-2))
		{
			int d = 0;
			if (o->x >= x && o->x+o->w > x+w) { d = x+w-o->x; o->x += d; o->w -= d; }
			if (o->x+o->w <= x+w && o->x < x) { o->w = x-o->x; }
			if (o->y >= y && o->y+o->h > y+h) { d = y+h-o->y; o->y += d; o->h -= d; }
			if (o->y+o->h <= y+h && o->y < y) { o->h = y-o->y; }
		}
	}
}
bool grid_fill_gap(grid *g, int x, int y, int w, int h, box *except)
{
	int i; box *b;
	// find boxes adjacent within
	int limits[8]; int counts[4];
	memset(counts, 0, sizeof(int)*4);
	limits[0] = limits[1] = y + (h/2);
	limits[2] = limits[3] = y + (h/2);
	limits[4] = limits[5] = x + (w/2);
	limits[6] = limits[7] = x + (w/2);
	// group by sides
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		if (b == except) continue;
		ubyte side = box_adjacent_within(b, x, y, w, h);
		// sides transposed
		if (side == RIGHT)
		{
			limits[0] = MIN(limits[0], b->y);
			limits[1] = MAX(limits[1], b->y + b->h);
			counts[0] += 1;
		} else
		if (side == LEFT)
		{
			limits[2] = MIN(limits[2], b->y);
			limits[3] = MAX(limits[3], b->y + b->h);
			counts[1] += 1;
		} else
		if (side == BOTTOM)
		{
			limits[4] = MIN(limits[4], b->x);
			limits[5] = MAX(limits[5], b->x + b->w);
			counts[2] += 1;
		} else
		if (side == TOP)
		{
			limits[6] = MIN(limits[6], b->x);
			limits[7] = MAX(limits[7], b->x + b->w);
			counts[3] += 1;
		}
	}
	// find sides of region exactly spanning the region
	int s = -1, c = 0;
	if (limits[0] == y && limits[1] == y+h && (s < 1 || c > counts[0])) s = 0, c = counts[0];
	if (limits[2] == y && limits[3] == y+h && (s < 1 || c > counts[1])) s = 1, c = counts[1];
	if (limits[4] == x && limits[5] == x+w && (s < 1 || c > counts[2])) s = 2, c = counts[2];
	if (limits[6] == x && limits[7] == x+w && (s < 1 || c > counts[3])) s = 3, c = counts[3];
	// if any, choose fewest boxes, resize, exit
	if (s > -1 && c > 0)
	{
		FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
		{
			if (b == except) continue;
			     if (s == 0 && box_adjacent_within(b, x, y, w, h) == RIGHT)  b->w += w;
			else if (s == 1 && box_adjacent_within(b, x, y, w, h) == LEFT) { b->x -= w; b->w += w; }
			else if (s == 2 && box_adjacent_within(b, x, y, w, h) == BOTTOM) b->h += h;
			else if (s == 3 && box_adjacent_within(b, x, y, w, h) == TOP)  { b->y -= h; b->h += h; }
		}
		return 1;
	}
	return 0;
}
bool grid_check_boxes(grid *g)
{
	int i, j; box *b, *o;
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		if (b->w < 50 || b->h < 50)
			return 0;
	}
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		FOR_ARRAY_PART (o, g->boxes, box, j, g->count)
		{
			if (o == b) continue;
			if (box_intersect(o, b->x+1, b->y+1, b->w-2, b->h-2))
				return 0;
		}
	}
	return 1;
}
bool grid_check_empty(grid *g)
{
	// every box should have other boxes adjacent to all sides, or be
	// itself adjacent to a grid side.
	int i, j; box *b, *o;
	if (!g->count) return 0;
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		int left = 0, right = 0, top = 0, bottom = 0;
		if (b->x == 0) left = b->h;
		if (b->y == 0) top  = b->w;
		if (b->x + b->w == g->w) right  = b->h;
		if (b->y + b->h == g->h) bottom = b->w;
		FOR_ARRAY_PART (o, g->boxes, box, j, g->count)
		{
			if (o == b) continue;
			ubyte side = box_adjacent(o, b->x, b->y, b->w, b->h);
			if (side)
			{
				int v = MIN(b->y+b->h, o->y+o->h) - MAX(b->y, o->y);
				int h = MIN(b->x+b->w, o->x+o->w) - MAX(b->x, o->x);
				if (side == LEFT)   right  += v;
				if (side == RIGHT)  left   += v;
				if (side == TOP)    bottom += h;
				if (side == BOTTOM) top    += h;
			}
		}
		if (left < b->h || right < b->h || top < b->w || bottom < b->w)
			return 0;
	}
	return 1;
}
bool grid_check(grid *g)
{
	if (!grid_check_boxes(g)) return 0;
	if (!grid_check_empty(g)) return 0;
	return 1;
}
bool grid_autocommit(grid *g, grid *cpy)
{
	bool res = grid_check(g);
	if (!res) memmove(g, cpy, sizeof(grid));
	free(cpy);
	return res;
}
bool grid_insert(grid *g, int x, int y, int w, int h)
{
	if (g->count == BOXES) return 0;
	grid *cpy = grid_copy(g);
	grid_make_gap(g, x, y, w, h, NULL);
	box_init(&g->boxes[g->count++], x, y, w, h);
	return grid_autocommit(g, cpy);
}
bool grid_remove(grid *g, int index)
{
	if (g->count == 1 || index >= g->count) return 0;
	grid *cpy = grid_copy(g);
	box *b = &g->boxes[index];
	int x = b->x, y = b->y, w = b->w, h = b->h;
	box_free(b); box *n = &g->boxes[index+1];
	memmove(b, n, (g->count - index + 1) * sizeof(box));
	g->count--; grid_fill_gap(g, x, y, w, h, NULL);
	if (index == g->active) g->active = 0;
	return grid_autocommit(g, cpy);
}
bool grid_resize(grid *g, int index, int x, int y, int w, int h)
{
	if (g->count == 1 || index >= g->count) return 0;
	grid *cpy = grid_copy(g);
	box *b = &g->boxes[index];
	// find any siblings, then transpose to covering box opposite
	grid_make_gap(g, x, y, w, h, b);
	if (x > b->x) grid_fill_gap(g, b->x, b->y, x-b->x, b->h, b);
	if (y > b->y) grid_fill_gap(g, b->x, b->y, b->w, y-b->y, b);
	if ((x+w) < (b->x+b->w)) grid_fill_gap(g, x+w, b->y, (b->x+b->w)-(x+w), b->h, b);
	if ((y+h) < (b->y+b->h)) grid_fill_gap(g, b->x, y+h, b->w, (b->y+b->h)-(y+h), b);
	b->x = x; b->y = y; b->w = w; b->h = h;
	return grid_autocommit(g, cpy);
}
bool grid_adjust(grid *g, int w, int h)
{
	grid *cpy = grid_copy(g);
	int cw = g->w, ch = g->h;
	int dw = abs(cw - w), dh = abs(ch - h);
	if (w > cw)
	{
		g->w = w;
		grid_fill_gap(g, cw, 0, dw, ch, NULL);
	} else
	if (w < cw)
	{
		grid_make_gap(g, w, 0, dw, ch, NULL);
		g->w = w;
	}
	cw = g->w; dw = 0;
	if (h > ch)
	{
		g->h = h;
		grid_fill_gap(g, 0, ch, cw, dh, NULL);
	} else
	if (h < cw)
	{
		grid_make_gap(g, 0, h, cw, dh, NULL);
		g->h = h;
	}
	return grid_autocommit(g, cpy);
}
// grid extensions
void grid_hide(grid *g)
{
	if (!(g->flags & GRID_VISIBLE)) return;
	int i; box *b;
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		XUnmapWindow(display, b->l); XUnmapWindow(display, b->r);
		XUnmapWindow(display, b->t); XUnmapWindow(display, b->b);
		XDestroyWindow(display, b->l); XDestroyWindow(display, b->r);
		XDestroyWindow(display, b->t); XDestroyWindow(display, b->b);
		b->l = None; b->r = None; b->t = None; b->b = None;
	}
	g->flags &= ~GRID_VISIBLE;
}
void grid_show(grid *g)
{
	int i; box *b;
	XSetWindowAttributes attr; attr.override_redirect = True;
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		char width = 2;
		char *color = (b == &g->boxes[g->active]) ? "Blue": "Red";
		if (b->l == None)
		{
			b->l = XNewWindow(display, root, b->x+pad_left, b->y+pad_top, width, b->h, 0, None, color, APIS_CLASS, APIS_CLASS);
			b->r = XNewWindow(display, root, b->x+pad_left + b->w - width, b->y+pad_top, width, b->h, 0, None, color, APIS_CLASS, APIS_CLASS);
			b->t = XNewWindow(display, root, b->x+pad_left, b->y+pad_top, b->w, width, 0, None, color, APIS_CLASS, APIS_CLASS);
			b->b = XNewWindow(display, root, b->x+pad_left, b->y+pad_top + b->h - width, b->w, width, 0, None, color, APIS_CLASS, APIS_CLASS);
			XChangeWindowAttributes(display, b->l, CWOverrideRedirect, &attr);
			XChangeWindowAttributes(display, b->r, CWOverrideRedirect, &attr);
			XChangeWindowAttributes(display, b->t, CWOverrideRedirect, &attr);
			XChangeWindowAttributes(display, b->b, CWOverrideRedirect, &attr);
			XMapWindow(display, b->l); XMapWindow(display, b->r);
			XMapWindow(display, b->t); XMapWindow(display, b->b);
		} else
		{
			ucell c = XGetColor(display, color);
			XSetWindowBackground(display, b->l, c);
			XSetWindowBackground(display, b->r, c);
			XSetWindowBackground(display, b->t, c);
			XSetWindowBackground(display, b->b, c);
			XMoveResizeWindow(display, b->l, b->x+pad_left, b->y+pad_top, width, b->h);
			XMoveResizeWindow(display, b->r, b->x+pad_left + b->w - width, b->y+pad_top, width, b->h);
			XMoveResizeWindow(display, b->t, b->x+pad_left, b->y+pad_top, b->w, width);
			XMoveResizeWindow(display, b->b, b->x+pad_left, b->y+pad_top + b->h - width, b->w, width);
			XRefresh(display, b->l); XRefresh(display, b->r);
			XRefresh(display, b->t); XRefresh(display, b->b);
		}
	}
	g->flags |= GRID_VISIBLE;
}
void grid_configure(grid *g)
{
	if (g->flags & GRID_VISIBLE) grid_show(g);
}
box* grid_box_by_region(grid *g, int x, int y, int w, int h)
{
	x += pad_left; y += pad_top;
	box *b, *nearest = NULL; int i, overlap = 0;
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
	{
		ubyte side = box_intersect(b, x, y, w, h);
		if (side)
		{
			int v = MIN(y+h, b->y+b->h) - MAX(y, b->y);
			int h = MIN(x+w, b->x+b->w) - MAX(x, b->x);
			if (overlap < v*h)
			{
				nearest = b;
				overlap = v*h;
			}
		}
	}
	return nearest;
}
// rules
ucell rule_match(rule *r, char *class, char *name, char *role, char *type, char *tag)
{
	bool cf = r->rflags & RULE_CLASS && class
		&& regmatch(r->class, class, 0, NULL, REG_EXTENDED|REG_ICASE) == 0 ? 1:0;
	bool nf = r->rflags & RULE_NAME  && name
		&& regmatch(r->name,  name,  0, NULL, REG_EXTENDED|REG_ICASE) == 0 ? 1:0;
	bool rf = r->rflags & RULE_ROLE  && role
		&& regmatch(r->role,  role,  0, NULL, REG_EXTENDED|REG_ICASE) == 0 ? 1:0;
	bool tf = r->rflags & RULE_TYPE  && type
		&& regmatch(r->type,  type,  0, NULL, REG_EXTENDED|REG_ICASE) == 0 ? 1:0;
	bool gf = r->rflags & RULE_TAG   && tag
		&& regmatch(r->tag,   tag,   0, NULL, REG_EXTENDED|REG_ICASE) == 0 ? 1:0;
	if (r->rflags & RULE_CLASS && !cf) return 0;
	if (r->rflags & RULE_NAME  && !nf) return 0;
	if (r->rflags & RULE_ROLE  && !rf) return 0;
	if (r->rflags & RULE_TYPE  && !tf) return 0;
	if (r->rflags & RULE_TAG   && !gf) return 0;
	return cf + nf + rf + tf + gf;
}
rule* rule_find(char *class, char *name, char *role, char *type, char *tag)
{
	int i; rule *r, *match = NULL;
	if (class || name || role || type)
	{
		ucell most = 0;
		FOR_STACK (r, rules, rule*, i)
		{
			ucell this = rule_match(r, class, name, role, type, tag);
			if (this > most)
			{
				most = this;
				match = r;
			}
		}
	}
	return match;
}
rule* rule_create(ucell rflags, char *class, char *name, char *role, char *type, char *tag, byte desktop, ucell setflags, ucell clrflags, ucell flipflags, int x, int y, int w, int h, int f, int cols, int rows)
{
	rule *r = allocate(sizeof(rule)); memset(r, 0, sizeof(rule));
	r->desktop = -2; r->setflags = 0; r->clrflags = 0; r->x = 0; r->y = 0; r->w = 0; r->h = 0; r->f = 0;
	r->cols = 0; r->rows = 0;
	if (rflags & RULE_CLASS && class) strcpy(r->class, class);
	if (rflags & RULE_NAME  && name ) strcpy(r->name,  name );
	if (rflags & RULE_ROLE  && role ) strcpy(r->role,  role );
	if (rflags & RULE_TYPE  && type ) strcpy(r->type,  type );
	if (rflags & RULE_TAG   && tag  ) strcpy(r->tag,   tag  );
	if (rflags & RULE_DESKTOP) r->desktop = desktop;
	if (rflags & RULE_X) r->x = x;
	if (rflags & RULE_Y) r->y = y;
	if (rflags & RULE_W) r->w = w;
	if (rflags & RULE_H) r->h = h;
	if (rflags & RULE_F) r->f = f;
	if (rflags & RULE_ROWS) r->rows = rows;
	if (rflags & RULE_COLS) r->cols = cols;
	if (rflags & RULE_FLAGS) { r->setflags = setflags; r->clrflags = clrflags; r->flipflags = flipflags; }
	r->rflags = rflags;
	return r;
}
// windows
void window_name(Window win, char *pad)
{
	strcpy(pad, "unknown");
	char *data; ucell len;
	if (XAtomGetString(display, WMName, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
	if (XAtomGetString(display, NetWMName, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
}
void window_class(Window win, char *pad)
{
	strcpy(pad, "unknown");
	XClassHint hint;
	if (XGetClassHint(display, win, &hint))
	{
		snprintf(pad, NOTE, "%s", hint.res_class);
		XFree(hint.res_name); XFree(hint.res_class);
	}
}
void window_role(Window win, char *pad)
{
	strcpy(pad, "unknown");
	char *data; ucell len;
	if (XAtomGetString(display, WMWindowRole, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
}
void window_tag(Window win, char *pad)
{
	strcpy(pad, "unknown");
	char *data; ucell len;
	if (XAtomGetString(display, ApisTag, win, &data, &len) && data && len)
		snprintf(pad, NOTE, "%s", data);
	free(data);
}
bool window_struts(Window win, ucell *l, ucell *r, ucell *t, ucell *b)
{
	bool ok = 0;
	if (l) *l = 0; if (r) *r = 0; if (t) *t = 0; if (b) *b = 0;
	ubyte *data = NULL; ucell len = 0; int *array;
	if (XAtomGet(display, NetWMStrut, win, &data, &len) && data && len)
	{
		ok = 1; array = (int*)data;
		if (l) *l = MIN(screen->width*0.1, MAX(0, array[0]));
		if (r) *r = MIN(screen->width*0.1, MAX(0, array[1]));
		if (t) *t = MIN(screen->height*0.1, MAX(0, array[2]));
		if (b) *b = MIN(screen->height*0.1, MAX(0, array[3]));
	}
	free(data);
	if (XAtomGet(display, NetWMStrutPartial, win, &data, &len) && data && len)
	{
		ok = 1; array = (int*)data;
		if (l) *l = MIN(screen->width*0.1, MAX(0, array[0]));
		if (r) *r = MIN(screen->width*0.1, MAX(0, array[1]));
		if (t) *t = MIN(screen->height*0.1, MAX(0, array[2]));
		if (b) *b = MIN(screen->height*0.1, MAX(0, array[3]));
	}
	free(data);
	return ok;
}
profile* profile_get(Window w)
{
	char name[32];
	sprintf(name, "%u", (ucell)w);
	return hash_get(profiles, name);
}
void profile_set(Window w, profile *p)
{
	char name[32];
	sprintf(name, "%u", (ucell)w);
	hash_set(profiles, name, p);
}
void sanity_cb(hash *h, char *key, void *val)
{
	profile *p = val;
	profile_update(p->win, 0);
}
void hook_run(ucell hook)
{
	char *cmd = hash_get(hook_commands, hook_names[hook]);
	if (cmd) exec_cmd(cmd);
}
void sanity()
{
	hash_iterate(profiles, sanity_cb);
}
int window_get_state(Window w)
{
	int r = WithdrawnState;
	ubyte *udata; ucell len;
	if (XAtomGet(display, WMState, w, &udata, &len) && udata && len)
		r = *((int*)udata);
	free(udata);
	return r;
}
ucell window_get_flags(Window w)
{
	ucell f = 0;
	ubyte *udata; ucell len;
	if (XAtomGet(display, ApisFlags, w, &udata, &len) && udata && len)
		f = *((ucell*)udata);
	free(udata);
	return f;
}
int window_get_desktop(Window w)
{
	int r = DESKTOP_NONE;
	ubyte *udata; ucell len;
	if (XAtomGet(display, NetWMDesktop, w, &udata, &len) && udata && len)
		r = *((int*)udata);
	free(udata);
	return r;
}
profile* profile_update(Window w, bool refresh)
{
	bool ok = 0;
	Window lroot;
	profile *ws = profile_get(w);
	if (!ws)
	{
		ws = allocate(sizeof(profile));
		memset(ws, 0, sizeof(profile));
		ws->win = w; refresh = 1;
		profile_set(w, ws);
	}
	if (XGetWindowAttributes(display, w, &ws->attr))
	{
		ok = 1;
		if (refresh)
		{
			ws->flags = window_get_flags(w);
			XGetTransientForHint(display, w, &ws->trans);
			if (ws->kids) { XFree(ws->kids); ws->kids = NULL; }
			XQueryTree(display, w, &lroot, &ws->parent, &ws->kids, &ws->nkids);
			window_class(w, ws->class); window_name(w, ws->name);
			window_role(w, ws->role); window_tag(w, ws->tag);
			ws->ewmh_type = XGetEWMHType(display, w);
			ws->ewmh_type_name[0] = '\0';
			if (ws->ewmh_type)
			{
				char *type = XGetAtomName(display, ws->ewmh_type);
				strcpy(ws->ewmh_type_name, type); if (type) XFree(type);
			}
			ws->ewmh_state = XGetEWMHState(display, w);
			ws->state = window_get_state(w);
			ws->desktop = window_get_desktop(w);
			window_struts(w, &ws->left, &ws->right, &ws->top, &ws->bottom);
			if (ws->hints) { XFree(ws->hints); ws->hints = NULL; }
			ws->hints = XGetWMHints(display, w);
		}
	}
	if (!ok)
	{
		profile_purge(w);
		ws = NULL;
	}
	return ws;
}
void profile_free(profile *p)
{
	if (p->kids) XFree(p->kids);
	if (p->hints) XFree(p->hints);
	free(p);
}
void profile_purge(Window w)
{
	profile *p = profile_get(w);
	if (p)
	{
		profile_free(p); char name[32];
		sprintf(name, "%u", (ucell)w);
		hash_del(profiles, name);
	}
}
bool window_manage(profile *p)
{
	if (p->win == canvas || p->win == root
		|| p->attr.override_redirect
		|| (p->ewmh_type != None
			&& p->ewmh_type != atoms[NetWMWindowTypeNormal]
			&& p->ewmh_type != atoms[NetWMWindowTypeUtility]
			&& p->ewmh_type != atoms[NetWMWindowTypeToolbar]
			&& p->ewmh_type != atoms[NetWMWindowTypeMenu]
			&& p->ewmh_type != atoms[NetWMWindowTypeDialog])
		|| strcmp(p->class, APIS_CLASS) == 0)
		return 0;
	return 1;
}
bool window_visible(profile *p)
{
	return (p->attr.map_state == IsViewable && p->state == NormalState
		&& !XIsEWMHState(p->ewmh_state, NetWMStateHidden)
		&& (p->desktop == current || p->desktop == DESKTOP_ALL
			|| XIsEWMHState(p->ewmh_state, NetWMStateSticky))
		) ?1:0;
}
Window window_get_active()
{
	Window w = XGetFocus(display);
	profile *p = NULL;
	if (w == root && lastactive != None
		&& (p = profile_update(lastactive, 0)) != NULL
		&& window_manage(p))
		w = lastactive;
	return w;
}
void window_set_state(profile *p, int state)
{
	int s[2]; s[0] = state; s[1] = None;
	XAtomSet(display, WMState, p->win, XA_CARDINAL, 32, &s, 2);
	p->state = state;
	if (state == NormalState)
	{
		p->ewmh_state &= ~(1 << (NetWMStateHidden - NetWMState));
		XSetEWMHState(display, p->win, p->ewmh_state);
	}
}
void window_set_flags(profile *p, ucell flag)
{
	p->flags |= flag|WINDOW_KNOWN;
	XAtomSet(display, ApisFlags, p->win, XA_INTEGER, 32, &p->flags, 1);
}
void window_clr_flags(profile *p, ucell flag)
{
	p->flags &= ~flag;
	XAtomSet(display, ApisFlags, p->win, XA_INTEGER, 32, &p->flags, 1);
}
void window_set_desktop(profile *p, int d)
{
	p->desktop = d;
	XAtomSet(display, NetWMDesktop, p->win, XA_CARDINAL, 32, &d, 1);
}
void window_update_desktop(profile *p, int d)
{
	if (p->desktop != DESKTOP_ALL && p->desktop != d)
		window_set_desktop(p, d);
}
rule* window_rule(profile *p)
{
	return rule_find(p->class, p->name, p->role, p->ewmh_type_name, p->tag);
}
rule* window_prepare(profile *p, ubyte refresh, rule *r)
{
	if (!r) r = window_rule(p);
	if (refresh || !p->flags || !(p->flags & WINDOW_KNOWN))
	{
		ucell nflags = r && r->rflags & RULE_MASK
			? (p->flags | (p->ewmh_state << WINDOW_STATE_RANGE)):
			WINDOW_DEFAULTS;
		window_clr_flags(p, p->flags);
		if (r && r->rflags & RULE_FLAGS)
		{
			nflags ^= r->flipflags;
			nflags &= ~r->clrflags;
			nflags |= r->setflags;
		}
		nflags &= ~WINDOW_CONFIG;
		window_set_flags(p, nflags);
		ucell props = nflags >> WINDOW_STATE_RANGE;
		XSetEWMHState(display, p->win, props);
		p->ewmh_state = props;
		if (p->desktop == DESKTOP_NONE) p->desktop = current;
		if (r && r->rflags & RULE_DESKTOP && r->desktop > DESKTOP_NONE) p->desktop = r->desktop;
		window_set_desktop(p, p->desktop);
	}
	return r;
}
void window_track(profile *p)
{
	winlist_discard(&windows, p->win);
	winlist_push(&windows, p->win);
	XSelectInput(display, p->win, PropertyChangeMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask);
}
void window_raise_kids(Window w, Window except, Window *wins, ucell num, Window *trans, winlist *local, ubyte *used, ubyte *attrok, ubyte *view, ubyte *manage)
{
	int i;
	for (i = 0; i < num; i++)
	{
		// locate main window transients
		if (wins[i] != except && !bm_chk(used, i) && bm_chk(attrok, i) && bm_chk(view, i))
		{
			if (bm_chk(manage, i) && trans[i] == w)
			{
				winlist_push(local, wins[i]); bm_set(used, i);
				window_raise_kids(wins[i], except, wins, num, trans, local, used, attrok, view, manage);
			}
		}
	}
}
void window_raise(profile *p)
{
	Window w = p->win;
	winlist local; winlist_init(&local);
	winlist list;  winlist_init(&list);
	winlist above; winlist_init(&above);
	Window super, parent, *wins = NULL; ucell num; int i;
	if (XQueryTree(display, root, &super, &parent, &wins, &num) && wins && num)
	{
		Window wtrans = None; int windex = -1, wtransindex = -1;
		XWindowAttributes *attr = allocate(sizeof(XWindowAttributes) * num);
		// map of transient relationships
		Window *trans = allocate(sizeof(Window) * num);
		// bit map of windows already in the stack list (double ups cause BadWindow)
		ubyte *used = bm_create(num);
		// bit map of XGetWindowAttributes success
		ubyte *attrok = bm_create(num);
		// bit map of manage flags
		ubyte *manage = bm_create(num);
		// bit map of viewable windows
		ubyte *view = bm_create(num);
		ucell *states = allocate(sizeof(ucell) * num);
		int *desktops = allocate(sizeof(int) * num);
		ucell *types = allocate(sizeof(ucell) * num);
		// build window associations
		for (i = 0; i < num; i++)
		{
			trans[i] = None;
			profile *p = profile_update(wins[i], 0);
			if (p)
			{
				bm_set(attrok, i);
				memmove(&attr[i], &p->attr, sizeof(XWindowAttributes));
				trans[i] = p->trans;
				states[i] = p->ewmh_state;
				desktops[i] = p->desktop;
				types[i] = p->ewmh_type;
				if (p && window_manage(p))
					bm_set(manage, i);
				if (attr[i].map_state == IsViewable)
					bm_set(view, i);
				if (wins[i] == w)
				{
					windex = i;
					wtrans = trans[i];
				}
			}
		}
		if (windex > -1)
		{
			if (wtrans != None)
			{
				for (i = 0; i < num; i++)
					if (wins[i] == wtrans) wtransindex = i;
				if (wtransindex < 0) wtrans = None;
			}
			if (wtrans != None)
			{
				winlist_push(&local, wtrans); bm_set(used, wtransindex);
				window_raise_kids(wtrans, w, wins, num, trans, &local, used, attrok, view, manage);
			}
			winlist_push(&local, w); bm_set(used, windex);
			window_raise_kids(w, None, wins, num, trans, &local, used, attrok, view, manage);
			for (i = 0; i < num; i++)
			{
				// find all _NET_WM_STATE_ABOVE, override_redirect, docks, panels
				if (!bm_chk(used, i) && bm_chk(attrok, i) && bm_chk(view, i))
				{
					if ( // want any window that should be above everything else
						(attr[i].override_redirect || XIsEWMHState(states[i], NetWMStateAbove)
							|| (types[i] == atoms[NetWMWindowTypeDock]
								&& !XIsEWMHState(states[i], NetWMStateBelow)))
						// don't want any managed window that belongs exclusively to another desktop
						&& (XIsEWMHState(states[i], NetWMStateSticky) || desktops[i] == current
							|| desktops[i] == DESKTOP_ALL || desktops[i] == DESKTOP_NONE))
					{
						winlist_push(&above, wins[i]);
						bm_set(used, i);
					}
				}
			}
			if (XIsEWMHState(states[windex], NetWMStateAbove)
				|| (wtrans != None && XIsEWMHState(states[wtransindex], NetWMStateAbove)))
				while (local.depth) winlist_push(&list, winlist_pop(&local));
			while (above.depth) winlist_push(&list, winlist_pop(&above));
			while (local.depth) winlist_push(&list, winlist_pop(&local));
			XRaiseWindow(display, list.items[0]);
			XRestackWindows(display, list.items, list.depth);
		}
		if (wins) XFree(wins);
		free(trans); free(attr);
		free(used); free(attrok); free(manage); free(view);
		free(states); free(desktops); free(types);
	}
	winlist_free(&above); winlist_free(&list); winlist_free(&local);
}
void window_focus(profile *p)
{
	Bool input = (p->hints && p->hints->flags & InputHint) ? p->hints->input: True;
	XSetInputFocus(display, input ? p->win: root, RevertToNone, CurrentTime);
	XAtomSet(display, NetActiveWindow, root, XA_WINDOW, 32, &p->win, 1);
	lastactive = p->win;
}
void window_check_visible(profile *p)
{
}
box* window_configure(profile *p, ucell mask, int x, int y, int w, int h, int b, rule *r, winlist *sibs)
{
	box *f = NULL;
	if (p->win == drag_win) return NULL;
	if (!r) r = window_rule(p);
	if (!(mask & CWX)) x = p->attr.x; if (!(mask & CWY)) y = p->attr.y;
	if (!(mask & CWWidth)) w = p->attr.width; if (!(mask & CWHeight)) h = p->attr.height;
	if (!(mask & CWBorderWidth)) b = 0;
	if (window_manage(p))
	{
		if (p->flags & WINDOW_BORDER) b = 1;
		profile *tp = NULL;
		if (!(p->flags & WINDOW_CONFIG) && p->trans != None
			&& (tp = profile_update(p->trans, 0)) != NULL)
		{
			if (!(mask & CWX)) x = MAX(0, tp->attr.x + ((tp->attr.width  - w) / 2));
			if (!(mask & CWY)) y = MAX(0, tp->attr.y + ((tp->attr.height - h) / 2));
		}
		if (!(p->flags & WINDOW_CONFIG) && r)
		{
			int rx = r->rflags & RULE_XR ? (screen_width  * ((dcell)r->x / 100)): r->x;
			int ry = r->rflags & RULE_YR ? (screen_height * ((dcell)r->y / 100)): r->y;
			int rw = r->rflags & RULE_WR ? (screen_width  * ((dcell)r->w / 100)): r->w;
			int rh = r->rflags & RULE_HR ? (screen_height * ((dcell)r->h / 100)): r->h;
			if (r->rflags & RULE_W) w = rw;
			if (r->rflags & RULE_H) h = rh;
			if (r->rflags & RULE_F)
			{
				int x1 = 0, y1 = 0,
					x2 = (screen_width - w) / 2, y2 = (screen_height - h) / 2,
					x3 = screen_width - w - (b*2), y3 = screen_height - h - (b*2);
				switch (r->f)
				{
					case 1: x = x1; y = y1; break;
					case 2: x = x2; y = y1; break;
					case 3: x = x3; y = y1; break;
					case 4: x = x1; y = y2; break;
					case 5: x = x2; y = y2; break;
					case 6: x = x3; y = y2; break;
					case 7: x = x1; y = y3; break;
					case 8: x = x2; y = y3; break;
					case 9: x = x3; y = y3; break;
				}
			}
			if (r->rflags & RULE_X) { x = rx < 0 ? screen_width  + rx: rx; }
			if (r->rflags & RULE_Y) { y = ry < 0 ? screen_height + ry: ry; }
			if (sibs)
			{
				profile *sp = NULL;
				if (r->rflags & (RULE_COLS|RULE_ROWS))
				{
					int cols = 1, rows = 1;
					if (r->rflags & RULE_COLS) cols = r->cols;
					if (r->rflags & RULE_ROWS) rows = r->rows;
					int bx = x, by = y, slot = MIN((cols*rows)-1, sibs->depth);
					int col = slot % cols, row = slot / cols, sw = w / cols, sh = h / rows;
					x = bx + (col * sw); y = by + (row * sh); w = sw - (b*2); h = sh - (b*2);
				} else
				if (sibs->depth && (sp = profile_update(winlist_top(sibs), 0)) != NULL)
				{
					if (r->rflags & RULE_LEFT  ) x = sp->attr.x - w + (b*2);
					if (r->rflags & RULE_RIGHT ) x = sp->attr.x + sp->attr.width + (b*2);
					if (r->rflags & RULE_TOP   ) y = sp->attr.y - h + (b*2);
					if (r->rflags & RULE_BOTTOM) y = sp->attr.y + sp->attr.height + (b*2);
					if (r->rflags & RULE_COVER ) x = sp->attr.x, y = sp->attr.y;
				}
			}
		}
		window_set_flags(p, WINDOW_CONFIG);
		bool center = 0; int border = b;
		int desktop = p->desktop < 0 ? current: p->desktop;
		if (p->flags & WINDOW_TILED && (
			(f = grid_box_by_region(grids[desktop], x+(w/4), y+(h/4), w/2, h/2)) != NULL
			|| (f = grid_box_by_region(grids[desktop], x, y, w, h)) != NULL))
		{
			x = f->x+pad_left; y = f->y+pad_top;
			w = f->w-(border*2); h = f->h-(border*2);
			center = 1;
		}
		if (XIsEWMHState(p->ewmh_state, NetWMStateMaximizedHorz))
		{
			x = 0; w = screen_width-(border*2);
		}
		if (XIsEWMHState(p->ewmh_state, NetWMStateMaximizedVert))
		{
			y = 0; h = screen_height-(border*2);
		}
		if (XIsEWMHState(p->ewmh_state, NetWMStateFullscreen))
		{
			border = 0; x = 0; y = 0; center = 1;
			w = screen_width; h = screen_height;
		}
		XFriendlyResize(display, p->win, x, y, w, h,
			p->flags & WINDOW_HINTS ?1:0, center);
		XSetWindowBorderWidth(display, p->win, border);
	} else
	{
		XWindowChanges wc;
		wc.width = MAX(1, w); wc.height = MAX(1, h); wc.x = x; wc.y = y;
		if (p->ewmh_type == atoms[NetWMWindowTypeDropdownMenu]
			|| p->ewmh_type == atoms[NetWMWindowTypePopupMenu]
			|| p->ewmh_type == atoms[NetWMWindowTypeTooltip]
			|| p->ewmh_type == atoms[NetWMWindowTypeNotification]
			|| p->ewmh_type == atoms[NetWMWindowTypeCombo])
		{
			wc.x = MIN(screen_width - wc.width,  MAX(0, x));
			wc.y = MIN(screen_width - wc.height, MAX(0, y));
		}
		wc.border_width = b;
		XConfigureWindow(display, p->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	}
	return f;
}
// map a window on the current desktop
box* window_restore(profile *p)
{
	window_set_state(p, NormalState);
	box *b = window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
	XMapWindow(display, p->win);
	return b;
}
void window_iconify(profile *p)
{
	XUnmapWindow(display, p->win);
	window_set_state(p, IconicState);
}
// ewmh
ubyte ewmh_cb(Window win, void *ptr)
{
	profile *p = profile_update(win, 0);
	if (p && window_manage(p) && p->state != WithdrawnState
		&& !(XIsEWMHState(p->ewmh_state, NetWMStateSkipTaskbar)
			|| XIsEWMHState(p->ewmh_state, NetWMStateSkipPager)
			|| XIsEWMHState(p->ewmh_state, NetWMStateHidden))
		) winlist_push(ptr, win);
	return 1;
}
void ewmh_windows()
{
	int i;
	winlist stacked; winlist_init(&stacked);
	XIterateChildren(display, root, ewmh_cb, &stacked);
	XAtomSet(display, NetClientListStacking, root, XA_WINDOW, 32, stacked.items, stacked.depth);
	winlist clients; winlist_init(&clients);
	for (i = 0; i < windows.depth; i++)
		ewmh_cb(windows.items[i], &clients);
	XAtomSet(display, NetClientList, root, XA_WINDOW, 32, clients.items, clients.depth);
	winlist_free(&stacked); winlist_free(&clients);
}
void ewmh_desktops()
{
	int i, len = 0;
	int wa[desktops * 4];
	int vp[desktops * 2];
	char *pad = malloc(16 * desktops);
	for (i = 0; i < desktops; i++)
	{
		len += sprintf(pad+len, "desktop%d", i); pad[len++] = '\0';
		wa[(i*4)] = 0; wa[(i*4)+1] = 0; wa[(i*4)+2] = screen_width; wa[(i*4)+3] = screen_height;
		vp[(i*2)] = 0; vp[(i*2)+1] = 0;
	}
	int xy[2]; xy[0] = screen_width; xy[1] = screen_height;
	XAtomSet(display, NetNumberOfDesktops, root, XA_CARDINAL, 32, &i, 1);
	XAtomSet(display, NetDesktopNames,     root, atoms[UTF8String], 8, pad, len);
	XAtomSet(display, NetCurrentDesktop,   root, XA_CARDINAL, 32, &current, 1);
	XAtomSet(display, NetDesktopGeometry,  root, XA_CARDINAL, 32, xy, 2);
	XAtomSet(display, NetDesktopViewport,  root, XA_CARDINAL, 32, vp, desktops*2);
	XAtomSet(display, NetWorkarea,         root, XA_CARDINAL, 32, wa, desktops*4);
	free(pad);
}
void ewmh()
{
	ewmh_windows();
	ewmh_desktops();
}
// desktops
ubyte desktop_rescue(Window w, void *ptr)
{
	profile *p = profile_update(w, 0);
	if (p && p->desktop >= desktops) window_set_desktop(p, current);
	return 1;
}
ubyte desktop_raise_cb(Window w, void *ptr)
{
	profile *p = profile_update(w, 0);
	if (p && window_visible(p) && window_manage(p))
		winlist_push(ptr, w);
	return 1;
}
void desktop_raise(int d)
{
	current = d;
	winlist raise; winlist_init(&raise);
	winlist_push(&raise, canvas);
	XIterateChildren(display, root, desktop_raise_cb, &raise);
	int i; for (i = 0; i < raise.depth; i++)
		XRaiseWindow(display, raise.items[i]);
	window_raise(profile_update(raise.items[raise.depth-1], 0));
	window_activate_last();
	winlist_free(&raise);
}
bool desktop_refresh_cb(Window w, void *ptr)
{
	int d = *((int*)ptr);
	profile *p = profile_update(w, 0);
	if (p && window_manage(p) && (p->desktop == d || (d == current &&
			(p->desktop == DESKTOP_ALL || XIsEWMHState(p->ewmh_state, NetWMStateSticky))
		)))
		window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
	return 1;
}
void desktop_configure(int d)
{
	XIterateChildren(display, root, desktop_refresh_cb, &d);
}
bool update_struts_cb(Window w, void *ptr)
{
	profile *p = profile_update(w, 0);
	if (p && window_visible(p))
	{
		pad_left   = MAX(p->left,   pad_left);
		pad_right  = MAX(p->right,  pad_right);
		pad_top    = MAX(p->top,    pad_top);
		pad_bottom = MAX(p->bottom, pad_bottom);
		screen_x = pad_left; screen_y = pad_top;
		screen_width  = screen->width  - pad_left - pad_right ;
		screen_height = screen->height - pad_top  - pad_bottom;
	}
	return 1;
}
void update_struts()
{
	bool ok = 0;
	pad_left = 0, pad_right = 0, pad_top = 0, pad_bottom = 0;
	XIterateChildren(display, root, update_struts_cb, NULL);
	ok = grid_adjust(grids[current], screen_width, screen_height);
	grid_snapshot(grids[current], current);
	desktop_configure(current);
}
// map, focus and raise a window, switching desktops if necessary
void window_activate(profile *p, bool refresh)
{
	if (window_get_active() != p->win || refresh)
	{
		box *b = window_restore(p);
		if (b) grids[current]->active = b - grids[current]->boxes;
		window_raise(p);
		grid_configure(grids[current]);
	}
	window_focus(p);
}
void window_activate_last()
{
	Window none = None;
	XAtomSet(display, NetActiveWindow, root, XA_WINDOW, 32, &none, 1);
	Window d1, d2, *wins = NULL; ucell num; int i = -1;
	if (XQueryTree(display, root, &d1, &d2, &wins, &num))
	{
		for (i = num-1; i > -1; i--)
		{
			profile *p = profile_update(wins[i], 0);
			if (p && window_visible(p) && window_manage(p)
				&& (p->ewmh_type == 0 || p->ewmh_type == atoms[NetWMWindowTypeNormal]
					|| p->ewmh_type == atoms[NetWMWindowTypeDialog]))
			{
				window_activate(p, 1);
				break;
			}
		}
		if (wins) XFree(wins);
	}
	if (i == -1) XSetInputFocus(display, root, RevertToNone, CurrentTime);
}
Window window_by_region(int x, int y, int w, int h, Window except)
{
	Window nearest = None; //ucell overlap = 0;
	Window d1, d2, *wins = NULL; ucell num; int i;
	if (XQueryTree(display, root, &d1, &d2, &wins, &num))
	{
		for (i = num-1; i > -1; i--)
		{
			if (wins[i] == except) continue;
			profile *p = profile_update(wins[i], 0);
			if (p && window_visible(p) && window_manage(p))
			{
				if (p->ewmh_type && p->ewmh_type != atoms[NetWMWindowTypeNormal]) continue;
				if (XIsEWMHState(p->ewmh_state, NetWMStateSkipTaskbar)
					|| XIsEWMHState(p->ewmh_state, NetWMStateSkipPager)
					|| XIsEWMHState(p->ewmh_state, NetWMStateHidden)) continue;
				bool overlap_x = region_overlap_x(p->attr.x, p->attr.y, p->attr.width, p->attr.height, x, y, w, h);
				bool overlap_y = region_overlap_y(p->attr.x, p->attr.y, p->attr.width, p->attr.height, x, y, w, h);
				if (overlap_x && overlap_y)
				{
					nearest = wins[i];
					break;
				}
			}
		}
		if (wins) XFree(wins);
	}
	return nearest;
}
//controls
bool parse_flags_type(hash *args, char *name, ucell *f)
{
	bool ok = 0;
	ucell flags = 0;
	char *csv = hash_expect(args, name, ".+", 0, NULL);
	if (csv)
	{
		ok = 1;
		char *s = csv;
		while (*s)
		{
			char *name = strnextthese(&s, ",");
			struct name_to_flag *nf = NULL;
			if (name && (nf = hash_get(flagnames, name)) != NULL)
				flags |= nf->flag;
			strskipthese(&s, ","); free(name);
		}
	}
	*f = flags;
	return ok;
}
bool parse_flags(hash *args, ucell *sf, ucell *cf, ucell *tf)
{
	bool a = parse_flags_type(args, "set",    sf);
	bool b = parse_flags_type(args, "clear",  cf);
	bool c = parse_flags_type(args, "toggle", tf);
	return (a || b || c) ?1:0;
}
ubyte parse_mode(hash *args)
{
	ubyte m = 0;
	char *mode = hash_expect(args, "mode", "^(on|off|toggle)$", 0, NULL);
	if (mode)
	{
		if (strcmp(mode, "on")     == 0) m = 1;
		if (strcmp(mode, "off")    == 0) m = 2;
		if (strcmp(mode, "toggle") == 0) m = 3;
	}
	return m;
}
char* parse_direction(hash *args)
{
	char *direction = hash_expect(args, "direction", "^(horiz|vert|horizontal|vertical)$", 0, NULL);
	if (!direction) direction = "horizontal";
	return direction;
}
bool parse_int(hash *args, char *name, int *r)
{
	char *val = NULL;
	regmatch_t subs[10];
	if ((val = hash_expect(args, name, "^-*[0-9]+$", 0, NULL)) != NULL)
	{
		*r = strtol(val, NULL, 10);
		return 1;
	}
	if ((val = hash_expect(args, name, "^([0-9]+)/([0-9+])$", 10, subs)) != NULL)
	{
		char *a = regsubstr(val, subs, 1);
		char *b = regsubstr(val, subs, 2);
		int an = strtol(a, NULL, 10);
		int bn = strtol(b, NULL, 10);
		*r = (int)(((dcell)(an)/(dcell)(bn)) * 100);
		free(a); free(b);
		return 2;
	} else
	if ((val = hash_expect(args, name, "^[0-9]+%$", 0, NULL)) != NULL)
	{
		*r = strtol(val, NULL, 10);
		return 2;
	}
	return 0;
}
bool op_split(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	char *direction = parse_direction(args);
	dcell n = 0.5; regmatch_t subs[10];
	char *ratio = hash_expect(args, "ratio", "^([0-9]+)/([0-9]+)$", 10, subs);
	if (ratio)
	{
		char *a = regsubstr(ratio, subs, 1);
		char *b = regsubstr(ratio, subs, 2);
		n = strtod(a, NULL) / strtod(b, NULL);
		free(a); free(b);
	}
	grid *g = grids[current];
	int bn; if (!parse_int(args, "box", &bn)) bn = g->active;
	box *b = &g->boxes[MIN(g->count-1, MAX(0, bn))];
	bool ok = (*direction == 'h')
		? grid_insert(g, b->x + ceil((dcell)b->w*n), b->y, b->w - ceil((dcell)b->w*n), b->h)
		: grid_insert(g, b->x, b->y + ceil((dcell)b->h*n), b->w, b->h - ceil((dcell)b->h*n));
	if (ok)
	{
		grid_snapshot(g, current);
		desktop_configure(current);
		grid_configure(g);
	}
	hash_free(args, 1);
	return ok;
}
bool op_size(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	grid *g = grids[current];
	int bn; if (!parse_int(args, "box", &bn)) bn = g->active;
	box *b = &g->boxes[MIN(g->count-1, MAX(0, bn))];
	int x = b->x, y = b->y, w = b->w, h = b->h;
	ubyte res = 0;
	if ((res = parse_int(args, "width",  &w)) > 1) w = screen_width  * ((dcell)w / 100);
	if ((res = parse_int(args, "height", &h)) > 1) h = screen_height * ((dcell)h / 100);
	if ((res = parse_int(args, "xpos",   &x)) > 1) x = screen_width  * ((dcell)x / 100);
	if ((res = parse_int(args, "ypos",   &y)) > 1) y = screen_height * ((dcell)y / 100);
	if (b->x+b->w == screen_width)  x = screen_width-w;
	if (b->y+b->h == screen_height) y = screen_height-h;
	bool ok = grid_resize(g, bn, x, y, w, h);
	if (ok)
	{
		grid_snapshot(g, current);
		desktop_configure(current);
		grid_configure(g);
	}
	hash_free(args, 1);
	return ok;
}
bool op_remove(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	grid *g = grids[current];
	int bn; if (!parse_int(args, "box", &bn)) bn = g->active;
	bool ok = grid_remove(g, MIN(g->count-1, MAX(0, bn)));
	if (ok)
	{
		grid_snapshot(g, current);
		desktop_configure(current);
		grid_configure(g);
	}
	hash_free(args, 1);
	return ok;
}
bool op_overlay(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	ubyte mode = parse_mode(args);
	grid *g = grids[current];
	ubyte ok = 0;
	if (mode)
	{
		     if (mode == 1 && !(g->flags & GRID_VISIBLE)) { grid_show(g); ok = 1; }
		else if (mode == 2 &&   g->flags & GRID_VISIBLE)  { grid_hide(g); ok = 1; }
		else if (mode == 3)
		{
			if (g->flags & GRID_VISIBLE) grid_hide(g); else grid_show(g);
			ok = 1;
		}
	}
	hash_free(args, 1);
	if (ok) grid_snapshot(g, current);
	return ok;
}
rule* parse_rule(hash *args)
{
	ucell rflags = 0, setflags = 0, clrflags = 0, flipflags = 0;
	int x = 0, y = 0, w = 0, h = 0, f = 0, rows = 0, cols = 0;
	char *class = hash_expect(args, "class", ".+", 0, NULL); if (class) rflags |= RULE_CLASS;
	char *name  = hash_expect(args, "name",  ".+", 0, NULL); if (name)  rflags |= RULE_NAME;
	char *role  = hash_expect(args, "role",  ".+", 0, NULL); if (role)  rflags |= RULE_ROLE;
	char *type  = hash_expect(args, "type",  ".+", 0, NULL); if (type)  rflags |= RULE_TYPE;
	char *tag   = hash_expect(args, "tag",   ".+", 0, NULL); if (tag)   rflags |= RULE_TAG;
	char *desktop = hash_expect(args, "desktop", "^[0-9]+$", 0, NULL);
	byte d = desktop ? strtol(desktop, NULL, 10): DESKTOP_NONE; rflags |= RULE_DESKTOP;
	if (parse_flags(args, &setflags, &clrflags, &flipflags)) rflags |= RULE_FLAGS;
	if (flipflags) rflags |= RULE_MASK;
	ubyte res = 0;
	if ((res = parse_int(args, "xpos",   &x)) > 0) { rflags |= RULE_X; if (res == 2) rflags |= RULE_XR; }
	if ((res = parse_int(args, "ypos",   &y)) > 0) { rflags |= RULE_Y; if (res == 2) rflags |= RULE_YR; }
	if ((res = parse_int(args, "width",  &w)) > 0) { rflags |= RULE_W; if (res == 2) rflags |= RULE_WR; }
	if ((res = parse_int(args, "height", &h)) > 0) { rflags |= RULE_H; if (res == 2) rflags |= RULE_HR; }
	if (parse_int(args, "cols",  &cols)) rflags |= RULE_COLS;
	if (parse_int(args, "rows",  &rows)) rflags |= RULE_ROWS;
	regmatch_t subs[3];
	char *grav = hash_expect(args, "place", "^(top|middle|bottom),(left|center|right)$", 3, subs);
	if (grav)
	{
		char *row = regsubstr(grav, subs, 1);
		char *col = regsubstr(grav, subs, 2);
		int r = 0, c = 0;
		switch (*row)
		{
			case 't': r = 0; break;
			case 'm': r = 1; break;
			case 'b': r = 2; break;
		}
		switch (*col)
		{
			case 'l': c = 0; break;
			case 'c': c = 1; break;
			case 'r': c = 2; break;
		}
		f = (r * 3) + c + 1;
		rflags |= RULE_F;
		free(row); free(col);
	}
	char *sibling = hash_expect(args, "sibling", ".+", 0, NULL);
	if (sibling)
	{
		char *s = sibling;
		while (*s)
		{
			char *word = strnextthese(&s, ",");
			     if (strcmp(word, "left")   == 0) rflags |= RULE_LEFT;
			else if (strcmp(word, "right")  == 0) rflags |= RULE_RIGHT;
			else if (strcmp(word, "top")    == 0) rflags |= RULE_TOP;
			else if (strcmp(word, "bottom") == 0) rflags |= RULE_BOTTOM;
			else if (strcmp(word, "cover")  == 0) rflags |= RULE_COVER;
			strskipthese(&s, ","); free(word);
		}
	}
	if (hash_find(args, "raise"))  rflags |= RULE_RAISE;
	if (hash_find(args, "focus"))  rflags |= RULE_FOCUS;
	return rule_create(rflags, class, name, role, type, tag, d, setflags, clrflags, flipflags, x, y, w, h, f, cols, rows);
}
bool op_rule(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	if (hash_find_one(args, "empty"))
	{
		while (rules->depth > 0)
			free(stack_pop(rules));
		return 1;
	}
	ubyte mode = parse_mode(args);
	rule *r = parse_rule(args);
	ubyte ok = 0;
	if (r && (r->class[0] || r->name[0] || r->role[0] || r->type[0] || r->tag[0])
		&& (mode == 1 || mode == 2))
	{
		rule *old = rule_find(r->class, r->name, r->role, r->type, r->tag);
		if (old)
		{
			stack_discard(rules, old);
			free(old);
		}
		if (mode == 1) stack_push(rules, r);
		else free(r);
		ok = 1;
	}
	hash_free(args, 1);
	return ok;
}
bool op_hook(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	int i; char **name;
	if (hash_find_one(args, "empty"))
	{
		FOR_ARRAY (name, hook_names, char*, i)
			free(hash_del(hook_commands, *name));
		return 1;
	}
	ubyte ok = 0;
	ubyte mode = parse_mode(args);
	char *event = hash_expect(args, "event", "^[a-z_]+$", 0, NULL);
	char *exec = hash_expect(args, "execute", ".+", 0, NULL);
	if (event && (mode == 1 || mode == 2))
	{
		FOR_ARRAY (name, hook_names, char*, i)
		{
			if (strcmp(*name, event) == 0)
			{
				free(hash_del(hook_commands, event));
				if (mode == 1) hash_set(hook_commands, event, strdup(exec));
				ok = 1;
				break;
			}
		}
	}
	hash_free(args, 1);
	return ok;
}
bool op_config_cb(Window w, void *ptr)
{
	struct rule_iter *ri = ptr;
	profile *p = profile_update(w, 0);
	if (p && window_visible(p) && window_manage(p)
		&& rule_match(ri->rule, p->class, p->name, p->role, p->ewmh_type_name, p->tag))
	{
		window_prepare(p, 1, ri->rule);
		window_configure(p, 0, 0, 0, 0, 0, 0, ri->rule, &ri->siblings);
		if (ri->rule->rflags & RULE_RAISE) window_raise(p);
		if (ri->rule->rflags & RULE_FOCUS) window_focus(p);
		winlist_push(&ri->siblings, w);
	}
	return 1;
}
bool op_config(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	rule *r = parse_rule(args);
	ubyte ok = 0;
	if (r)
	{
		r->rflags |= RULE_MASK;
		if (!(r->class[0] || r->name[0] || r->role[0] || r->type[0] || r->tag[0]))
		{
			Window w; profile *p;
			if ((w = window_get_active()) != None && w != root && (p = profile_update(w, 0)) != NULL)
			{
				window_prepare(p, 1, r);
				window_configure(p, 0, 0, 0, 0, 0, 0, r, NULL);
			}
		} else
		{
			struct rule_iter ri; ri.rule = r; winlist_init(&ri.siblings);
			XIterateChildrenBack(display, root, op_config_cb, &ri);
			ok = 1; winlist_free(&ri.siblings);
		}
		free(r);
	}
	hash_free(args, 1);
	ewmh_windows();
	return ok;
}
bool op_reset_cb(Window w, void *ptr)
{
	rule *r = ptr;
	profile *p = profile_update(w, 0);
	if (p && window_visible(p) && window_manage(p)
		&& rule_match(r, p->class, p->name, p->role, p->ewmh_type_name, p->tag))
	{
		window_prepare(p, 1, NULL);
		window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
	}
	return 1;
}
bool op_reset(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	rule *r = parse_rule(args);
	ubyte ok = 0;
	if (r)
	{
		if (!(r->class[0] || r->name[0] || r->role[0] || r->type[0]))
		{
			Window w; profile *p;
			if ((w = window_get_active()) != None && w != root && (p = profile_update(w, 0)) != NULL)
			{
				window_prepare(p, 1, NULL);
				window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
			}
		} else
		{
			XIterateChildren(display, root, op_reset_cb, r);
			ok = 1;
		}
		free(r);
	}
	hash_free(args, 1);
	ewmh_windows();
	return ok;
}
void op_show_rules(autostr *out)
{
	int i; rule *r;
	FOR_STACK (r, rules, rule*, i)
		str_print(out, NOTE, "0x%x %s %s %s %s %d 0x%x 0x%x %d %d %d %d %d\n", r->rflags,
			r->class[0] ? r->class: "(null)",
			r->name[0]  ? r->name:  "(null)",
			r->role[0]  ? r->role:  "(null)",
			r->type[0]  ? r->type:  "(null)",
			r->desktop, r->setflags, r->clrflags, r->x, r->y, r->w, r->h, r->f);
}
void op_show_grid(autostr *out)
{
	box *b; int i; grid *g = grids[current];
	FOR_ARRAY_PART (b, g->boxes, box, i, g->count)
		str_print(out, NOTE, "%d %s %d %d %d %d\n", i, i == g->active ? "*": "-", b->x, b->y, b->w, b->h);
}
void op_show_hooks(autostr *out)
{
	int i; char **name;
	FOR_ARRAY (name, hook_names, char*, i)
	{
		char *cmd = hash_get(hook_commands, *name);
		if (cmd) str_print(out, NOTE, "%s: %s\n", *name, cmd);
	}
}
bool op_show(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	char *type = hash_expect(args, "type", "^(rules|grid|hooks)$", 0, NULL);
	bool ok = 0;
	if (type)
	{
		if (strcmp(type, "rules") == 0)
			op_show_rules(out);
		else
		if (strcmp(type, "grid") == 0)
			op_show_grid(out);
		else
		if (strcmp(type, "hooks") == 0)
			op_show_hooks(out);
		ok = 1;
	}
	hash_free(args, 1);
	return ok;
}
bool op_focus(char *in, autostr *out)
{
	hash *args = hash_decode(in);
	char *dir = hash_expect(args, "direction", "^(left|right|up|down)$", 0, NULL);
	bool ok = 0;
	Window active = window_get_active(), next = None; profile *p = NULL;
	if (dir && active != None && active != root && (p = profile_update(active, 0)) != NULL)
	{
		int x = p->attr.x, y = p->attr.y, w = p->attr.width, h = p->attr.height;
		if (*dir == 'l')
			if ((next = window_by_region(0, y, x, h, active)) == None)
				next = window_by_region(0, 0, x, screen_height, active);
		if (*dir == 'r')
			if ((next = window_by_region(x + w, y, screen_width - x - w, h, active)) == None)
				next = window_by_region(x + w, 0, screen_width - x - w, screen_height, active);
		if (*dir == 'u')
			if ((next = window_by_region(x, 0, w, y, active)) == None)
				next = window_by_region(0, 0, screen_width, y, active);
		if (*dir == 'd')
			if ((next = window_by_region(x, y + h, w, screen_height - y - h, active)) == None)
				next = window_by_region(0, y + h, screen_width, screen_height - y - h, active);
		profile *np;
		if (next != None && (np = profile_update(next, 0)) != NULL)
		{
			window_activate(np, 1);
			ok = 1;
		}
	}
	hash_free(args, 1);
	return ok;
}
void grabs()
{
	XUngrabButton(display, AnyButton, AnyModifier, root);
	XGrabButton(display, AnyButton, AnyModifier, root, True, ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
}
void event_KeyPress(XEvent *e)
{
}
void event_KeyRelease(XEvent *e)
{
}
void event_ButtonPress(XEvent *e)
{
	while(XCheckTypedEvent(display, ButtonPress, e));
	XButtonEvent *be = &e->xbutton;
	profile *p = profile_update(be->subwindow, 0);
	if (p && be->subwindow != None && window_manage(p))
	{
		window_activate(p, 0);
		ewmh_windows();
		if (be->state & Mod4Mask)
		{
			drag_win = be->subwindow;
			drag_x = be->x_root; drag_y = be->y_root; drag_button = be->button;
			XGetWindowAttributes(display, p->win, &drag_attr);
			XGrabPointer(display, be->subwindow, True, PointerMotionMask|ButtonReleaseMask,
				GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
			if (drag_button == 3) window_clr_flags(p, WINDOW_TILED);
		}
	} else
	{
		box *b = grid_box_by_region(grids[current], be->x, be->y, 1, 1);
		if (b) grids[current]->active = b - grids[current]->boxes;
		XSetInputFocus(display, root, RevertToNone, CurrentTime);
		grid_configure(grids[current]);
	}
	XAllowEvents(display, ReplayPointer, CurrentTime);
}
void event_ButtonRelease(XEvent *e)
{
	XUngrabPointer(display, CurrentTime);
	profile *p;
	if (drag_win != None && (p = profile_update(drag_win, 0)) != NULL)
	{
		drag_win = None;
		window_activate(p, 1);
		window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
	}
	while(XCheckTypedEvent(display, MotionNotify, e));
}
void event_MotionNotify(XEvent *e)
{
	int xd, yd;
	while(XCheckTypedEvent(display, MotionNotify, e));
	XMotionEvent *motion = &e->xmotion;
	XButtonEvent *button = &e->xbutton;
	if (motion->same_screen)
	{
		profile *p = profile_update(motion->window, 0);
		xd = button->x_root - drag_x;
		yd = button->y_root - drag_y;
		int x = drag_attr.x + (drag_button == 1 ? xd : 0);
		int y = drag_attr.y + (drag_button == 1 ? yd : 0);
		int w = MAX(1, drag_attr.width + (drag_button == 3 ? xd : 0));
		int h = MAX(1, drag_attr.height + (drag_button == 3 ? yd : 0));
		XFriendlyResize(display, motion->window, x, y, w, h,
			p->flags & WINDOW_HINTS ?1:0, 0);
	}
}
void event_EnterNotify(XEvent *e)
{
}
void event_LeaveNotify(XEvent *e)
{
}
void event_FocusIn(XEvent *e)
{
	XFocusInEvent *f = &e->xfocus;
	XSetWindowBorder(display, f->window, XGetColor(display, "Orange"));
}
void event_FocusOut(XEvent *e)
{
	XFocusOutEvent *f = &e->xfocus;
	XSetWindowBorder(display, f->window, XGetColor(display, "Dim Gray"));
}
void event_KeymapNotify(XEvent *e)
{
}
void event_Expose(XEvent *e)
{
	XCompressEvents(display, e);
}
void event_GraphicsExpose(XEvent *e)
{
}
void event_NoExpose(XEvent *e)
{
}
void event_VisibilityNotify(XEvent *e)
{
	XCompressEvents(display, e);
}
void event_CreateNotify(XEvent *e)
{
	XCompressEvents(display, e);
	XCreateWindowEvent *cw = &e->xcreatewindow;
	profile *p = profile_update(cw->window, 1);
	if (!p) return;
	if (window_manage(p))
	{
		window_track(p);
		window_set_state(p, WithdrawnState);
	} else
	if (strcmp(p->class, APIS_CLASS) == 0)
	{
		XSelectInput(display, p->win, PropertyChangeMask);
		XAtomSetString(display, ApisReady, p->win, "ok");
	}
}
void event_DestroyNotify(XEvent *e)
{
	XCompressEvents(display, e);
	winlist_discard(&windows, e->xany.window);
	profile_purge(e->xany.window);
}
void event_UnmapNotify(XEvent *e)
{
	XCompressEvents(display, e);
	XUnmapEvent *ue = &e->xunmap;
	profile *p = profile_update(ue->window, 0);
	if (p && p->state == NormalState)
		window_set_state(p, WithdrawnState);
	ubyte *udata = NULL; ucell len;
	if (XAtomGet(display, NetActiveWindow, root, &udata, &len) && udata && len
		&& *((Window*)udata) == ue->window)
		window_activate_last();
	free(udata);
	update_struts();
	ewmh_windows();
}
void event_MapNotify(XEvent *e)
{
	XCompressEvents(display, e);
	profile *p = profile_update(e->xmap.window, 0);
	if (!p) return;
	window_set_state(p, NormalState);
	if (window_manage(p))
		window_update_desktop(p, current);
	else window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
	if (window_struts(e->xmap.window, NULL, NULL, NULL, NULL))
		update_struts();
	ewmh_windows();
}
void event_MapRequest(XEvent *e)
{
	XCompressEvents(display, e);
	XMapRequestEvent *mr = &e->xmaprequest;
	profile *p = profile_update(mr->window, 1);
	if (!p) return;
	if (window_manage(p))
	{
		window_track(p);
		window_prepare(p, 1, NULL);
		window_activate(p, 1);
	}
	else	XMapWindow(display, p->win);
}
void event_ReparentNotify(XEvent *e)
{
}
void event_ConfigureNotify(XEvent *e)
{
	int i;
	XCompressEvents(display, e);
	XConfigureEvent *ce = &e->xconfigure;
	if (ce->window == root && (screen->width != ce->width || screen->height != ce->height))
	{
		screen->width = ce->width;
		screen->height = ce->height;
		screen_width  = screen->width  - pad_left - pad_right ;
		screen_height = screen->height - pad_top  - pad_bottom;
		for (i = 0; i < MAX_DESKTOPS; i++)
		{
			grid_free(grids[i]);
			grids[i] = grid_create(screen_width, screen_height);
			grid_snapshot(grids[i], i);
		}
	}
}
void event_ConfigureRequest(XEvent *e)
{
	XConfigureRequestEvent *cr = &e->xconfigurerequest;
	profile *p = profile_update(cr->window, 1);
	if (!p) return;
	ucell mask = cr->value_mask;
	int x = mask & CWX ? cr->x: 0;
	int y = mask & CWY ? cr->y: 0;
	int w = mask & CWWidth  ? cr->width : 0;
	int h = mask & CWHeight ? cr->height: 0;
	window_configure(p, mask, x, y, w, h, cr->border_width, NULL, NULL);
}
void event_GravityNotify(XEvent *e)
{
}
void event_ResizeRequest(XEvent *e)
{
}
void event_CirculateNotify(XEvent *e)
{
}
void event_CirculateRequest(XEvent *e)
{
}
void event_PropertyNotify(XEvent *e)
{
	XPropertyEvent *pe = &e->xproperty;
//	char *name = XGetAtomName(display, pe->atom);
//	 if (name) XFree(name);
	if (pe->atom == atoms[ApisCommandCode])
	{
		ubyte *cmd = NULL; char *in = NULL; ucell len = 0; ubyte cc = OP_NOP;
		autostr out; str_create(&out); ucell rc = 0;
		if (XAtomGet(display, ApisCommandCode, pe->window, &cmd, &len) && cmd && len)
			cc = *((ucell*)cmd);
		XAtomGetString(display, ApisCommandIn, pe->window, &in, &len);
		if (opcode[cc]) rc = opcode[cc](in, &out);
		XAtomSetString(display, ApisResultOut, pe->window, out.pad);
		XAtomSet(display, ApisResultCode, pe->window, XA_INTEGER, 32, &rc, 1);
		free(cmd); free(in); str_free(&out);
	}
/*	else
	if (pe->atom == atoms[NetWMState])
	{
		profile *p = profile_update(pe->window, 1);
		if (p && window_manage(p))
			window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
	}*/
}
void event_SelectionClear(XEvent *e)
{
}
void event_SelectionRequest(XEvent *e)
{
}
void event_SelectionNotify(XEvent *e)
{
}
void event_ColormapNotify(XEvent *e)
{
}
void event_ClientMessage(XEvent *e)
{
	XClientMessageEvent *cm = &e->xclient;
//	char *name = XGetAtomName(display, cm->message_type);
//	note("%s", name); if (name) XFree(name);
	profile *p = profile_update(cm->window, 1);
	if (!p) return;
	if (cm->message_type == atoms[NetActiveWindow])
	{
		window_activate(p, 1);
		ewmh_windows();
	} else
	if (cm->message_type == atoms[WMChangeState])
	{
		if (window_manage(p))
		{
			if (p->attr.map_state == IsViewable)
				window_iconify(p);
			else	window_activate(p, 1);
			ewmh_windows();
		}
	} else
	if (cm->message_type == atoms[NetCloseWindow])
	{
		XClose(display, p->win);
	} else
	if (cm->message_type == atoms[NetCurrentDesktop])
	{
		int target = MIN(desktops-1, MAX(0, cm->data.l[0]));
		if (target != current)
		{
			desktop_raise(target);
			ewmh_desktops();
			hook_run(HOOK_SWITCH_DESKTOP);
		}
	} else
	if (cm->message_type == atoms[NetMoveResizeWindow])
	{
		window_configure(p, CWX|CWY|CWWidth|CWHeight,
			cm->data.l[1], cm->data.l[2], cm->data.l[3], cm->data.l[4], 0, NULL, NULL);
	} else
	if (cm->message_type == atoms[NetNumberOfDesktops])
	{
		desktops = MIN(MAX_DESKTOPS, MAX(1, cm->data.l[0]));
		XIterateChildren(display, root, desktop_rescue, NULL);
		ewmh_desktops();
	} else
	if (cm->message_type == atoms[NetWMState])
	{
		int i, j; ucell state = p->ewmh_state; //, ostate = state;
		for (i = 0; i < 2; i++)
		{
			if (!cm->data.l[i+1]) continue;
			for (j = NetWMState; j < AtomLast; j++)
				if (atoms[j] == cm->data.l[i+1]) break;
			if (j == AtomLast) continue;
			ucell atom = (1 << (j - NetWMState));
			switch (cm->data.l[0])
			{
				case 0: state &= ~atom; break;
				case 1: state |= atom;  break;
				case 2: state ^= atom;  break;
			}
		}
		XSetEWMHState(display, cm->window, state); p->ewmh_state = state;
		window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
/*		ucell hstate = (1 << (NetWMStateHidden - NetWMState));
		if ((ostate & hstate) != (state & hstate))
		{
			if (XIsEWMHState(p->ewmh_state, NetWMStateHidden))
				window_iconify(p);
			else	window_restore(p);
		}
*/		ewmh_windows();
	} else
	if (cm->message_type == atoms[NetWMDesktop])
	{
		int desktop = cm->data.l[0];
		window_set_desktop(p, desktop);
		if (desktop != DESKTOP_ALL && desktop != current)
			XLowerWindow(display, p->win);
		ewmh_windows();
	}
}
void event_MappingNotify(XEvent *e)
{
	XMappingEvent *me = &e->xmapping;
	if (me->request == MappingKeyboard || me->request == MappingModifier)
	{
		XRefreshKeyboardMapping(me);
		grabs();
	}
}
#ifdef GenericEvent
void event_GenericEvent(XEvent *e)
{
}
#endif
void timeout(int sig)
{
	fprintf(stderr, "timed out");
	exit(EXIT_FAILURE);
}
bool insert_command(ucell code, char* cmd)
{
	Window win = XNewWindow(display, root, -1, -1, 1, 1, 0, None, None, APIS_CLASS, APIS_CLASS);
	XSelectInput (display, win, PropertyChangeMask);
	// wait for main wm process to set ApisReady on our window
	signal(SIGALRM, timeout); alarm(5);
	for (;;)
	{
		XEvent ev;
		XMaskEvent (display, PropertyChangeMask, &ev);
		if (ev.xproperty.atom == atoms[ApisReady]
	          && ev.xproperty.state == PropertyNewValue)
			break;
		usleep(10000);
	}
	// send command
	XAtomSetString(display, ApisCommandIn, win, cmd);
	XAtomSet(display, ApisCommandCode, win, XA_INTEGER, 32, &code, 1);
	// wait for main wm process to set ApisResult on our window
	ubyte *data = NULL; char *res; ucell len = 0;
	signal(SIGALRM, timeout); alarm(5);
	ucell rc = 0;
	for (;;)
	{
		XEvent ev;
		XMaskEvent (display, PropertyChangeMask, &ev);
		if (ev.xproperty.atom == atoms[ApisResultCode]
	          && ev.xproperty.state == PropertyNewValue)
		{
			if (XAtomGet(display, ApisResultCode, win, &data, &len) && data && len)
			{
				rc = *((ucell*)data);
				if (XAtomGetString(display, ApisResultOut, win, &res, &len) && res && len)
					fprintf(rc ? stdout: stderr, "%s", res);
				free(res);
				break;
			}
			free(data);
		}
		usleep(10000);
	}
	XDestroyWindow(display, win);
	return rc;
}
ubyte setup_window(Window win, void *ptr)
{
	profile *p = profile_update(win, 1);
	if (p)
	{
		if (p->attr.map_state == IsViewable)
			window_set_state(p, NormalState);
		if (window_manage(p))
		{
			window_track(p);
			if (p->desktop == DESKTOP_NONE)
				window_set_desktop(p, current);
			switch (p->state)
			{
				case NormalState:
					XMapWindow(display, win);
					break;
				case IconicState:
					XUnmapWindow(display, win);
					break;
				default:
					XUnmapWindow(display, win);
					window_set_state(p, WithdrawnState);
					break;
			}
			window_prepare(p, !(p->flags & WINDOW_KNOWN)
				|| hash_find(arguments, "refresh") ?1:0, NULL);
			window_configure(p, 0, 0, 0, 0, 0, 0, NULL, NULL);
		}
	}
	return 1;
}
int main(int argc, char *argv[])
{
	ubyte *udata; char *data, tmp[NOTE]; ucell len; int i, j;
	check = time(0);

	display = XOpenDisplay(0);
	assert(display, "cannot open display");
	xerrorxlib = XSetErrorHandler(XWtf);
	screen = XScreenOfDisplay(display, DefaultScreen(display));
	root = screen->root;
	screen_width = screen->width;
	screen_height = screen->height;

	for (i = 0; i < AtomLast; i++)
		atoms[i] = XInternAtom(display, atom_names[i], False);

	flagnames = hash_create();
	struct name_to_flag *nf;
	FOR_ARRAY (nf, names_to_flags, struct name_to_flag, i)
		hash_set(flagnames, nf->name, nf);

	commandnames = hash_create();
	command *com;
	FOR_ARRAY (com, commands, command, i)
		hash_set(commandnames, com->name, com);

	winlist_init(&windows);
	profiles = hash_create();
	hook_commands = hash_create();

	char *arg;
	arguments = args_to_hash(argc, argv);
	if (hash_find_one(arguments, "v,version"))
	{
		printf("%s\n", VERSION);
		exit(EXIT_SUCCESS);
	}
	if ((arg = hash_get_one(arguments, "c,command")) != NULL)
	{
		com = hash_get(commandnames, arg);
		if (com)
		{
			char *args = hash_encode(arguments);
			exit(insert_command(com->code, args) ? EXIT_SUCCESS: EXIT_FAILURE);
		}
		exit(EXIT_FAILURE);
	}
	debug = hash_find(arguments, "debug") ? 1:0;
	rules = stack_create();

	session = (XAtomGetString(display, ApisSession, root, &data, &len) && data && len && !hash_find(arguments, "refresh"))
		? hash_decode(data): hash_create();
	free(data);

	canvas = XNewWindow(display, root, 0, 0, screen->width, screen->height, 0, None, None, NULL, NULL);
	XSetWindowBackgroundPixmap(display, canvas, ParentRelative);
	XLowerWindow(display, canvas); XMapWindow(display, canvas);

	desktops = 1; current = 0;
	for (i = 0; i < MAX_DESKTOPS; i++)
	{
		grids[i] = grid_create(screen->width, screen->height);
		sprintf(tmp, "grid%d", i);
		char *desc = session_get(tmp, NULL);
		if (desc) grid_load(grids[i], desc);
		grid_snapshot(grids[i], i);
	}
	if (XAtomGet(display, NetNumberOfDesktops, root, &udata, &len) && udata && len)
		desktops = MIN(MAX_DESKTOPS, MAX(1, *((int*)udata)));
	XIterateChildren(display, root, setup_window, NULL);
	desktop_raise(current);
	update_struts();

	XDefineCursor(display, root, XCreateFontCursor(display, XC_left_ptr));
	XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask | FocusChangeMask);
	XAtomSet(display, NetSupported, root, XA_ATOM, 32, atoms, AtomLast);

	Window e = XNewWindow(display, root, -1, -1, 1, 1, 0, None, None, APIS_CLASS, APIS_CLASS);
	XAtomSet(display, NetSupportingWMCheck, root, XA_WINDOW, 32, &e, 1);
	XAtomSet(display, NetSupportingWMCheck, e,    XA_WINDOW, 32, &e, 1);
	XAtomSetString(display, NetWMName, e, APIS_CLASS);

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

	ewmh(); grabs();
	XEvent a;
	for (;;)
	{
		if (check < time(0) - 10)
		{
			sanity();
			check = time(0);
		}
		// event compression
		XNextEvent(display, &a);
		event[a.type](&a);
	}
}
