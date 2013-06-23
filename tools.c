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

#include "tools.h"
#include "tools_proto.h"

void notice(const char *file, ucell line, const char *func, const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	if (DEBUG)
	{
		if (file) fprintf(stderr, "%s ", file);
		if (line) fprintf(stderr, "%d ", line);
		if (func) fprintf(stderr, "%s ", func);
	}
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
void bailout(const char *file, ucell line, const char *func, const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	if (DEBUG)
	{
		if (file) fprintf(stderr, "%s ", file);
		if (line) fprintf(stderr, "%d ", line);
		if (func) fprintf(stderr, "%s ", func);
	}
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	throw(1);
}
void noop()
{};

#define assert(f,...) if (!(f)) bailout(__FILE__,__LINE__,__func__,__VA_ARGS__)
#define wtf(...) bailout(__FILE__,__LINE__,__func__,__VA_ARGS__)
#if DEBUG
	#define note(...) notice(__FILE__,__LINE__,__func__,__VA_ARGS__)
#else
	#define note(...) noop()
#endif
#define crap(...) notice(__FILE__,__LINE__,__func__,__VA_ARGS__)

void* allocate(ucell size)
{
	void *p = malloc(size);
	assert(p, "malloc failed for %d bytes", size);
	return p;
}
void* reallocate(void *p, ucell size)
{
	p = realloc(p, size);
	assert(p, "realloc failed for %d bytes", size);
	return p;
}
// few extra string utils
ucell strskip(char **str, str_cb_chr cb)
{
	char *p = *str, *s = p;
	while (*p && cb(*p)) p++;
	*str = p;
	return p - s;
}
ucell strscan(char **str, str_cb_chr cb)
{
	char *p = *str, *s = p;
	while (*p && !cb(*p)) p++;
	*str = p;
	return p - s;
}
ucell strskipthese(char **str, const char *these)
{
	char *p = *str, *s = p;
	while (*p && strchr(these, *p)) p++;
	*str = p;
	return p - s;
}
ucell strscanthese(char **str, const char *these)
{
	char *p = *str, *s = p;
	while (*p && !strchr(these, *p)) p++;
	*str = p;
	return p - s;
}
ucell strltrim(byte *addr)
{
	ucell len = strlen(addr);
	if (!len) return len;
	byte *start = addr;
	strskip(&start, isspace);
	ucell rlen = len - (start - addr);
	memmove(addr, start, rlen);
	addr[rlen] = '\0';
	return rlen;
}
ucell strrtrim(byte *addr)
{
	ucell len = strlen(addr);
	if (!len) return len;
	byte *end = addr + len;
	while (end > addr && isspace(*(end-1))) end--;
	*end = '\0';
	return end - addr;
}
ucell strtrim(byte *addr)
{
	ucell len = strlen(addr);
	len = strltrim(addr);
	len = strrtrim(addr);
	return len;
}
char* strpull(byte *addr, ucell len)
{
	char *cpy = allocate(len+1);
	strncpy(cpy, addr, len);
	cpy[len] = '\0';
	return cpy;
}
// similar use to strtok, but reentrant
char* strnext(byte **addr, str_cb_chr cb)
{
	char *p = *addr;
	ucell len = strscan(addr, cb);
	return strpull(p, len);
}
char* strnextthese(byte **addr, const char *these)
	{
	char *p = *addr;
	ucell len = strscanthese(addr, these);
	return strpull(p, len);
}

// simple string handler
void str_create(autostr *s)
{
	s->pad = allocate(NOTE);
	s->pad[0] = '\0'; s->len = 0; s->lim = NOTE;
}
void str_require(autostr *s, ucell len)
{
	if (s->len + len + 1 > s->lim)
	{
		s->lim += CHUNKS(NOTE, len + 1);
		s->pad = reallocate(s->pad, s->lim);
	}
}
void str_append(autostr *s, char *p, ucell len)
{
	str_require(s, len);
	memmove(s->pad + s->len, p, len);
	s->len += len; s->pad[s->len] = '\0';
}
void str_print(autostr *s, ucell lim, char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	str_require(s, lim+1);
	vsnprintf(s->pad+s->len, lim+1, fmt, ap);
	s->len += strlen(s->pad+s->len);
	va_end(ap);
}
void str_drop(autostr *s, ucell len)
{
	len = MIN(len, s->len);
	s->len -= len;
	s->pad[s->len] = '\0';
}
void str_push(autostr *s, char c)
{
	str_append(s, &c, 1);
}
void str_free(autostr *s)
{
	free(s->pad);
}
int regmatch(char *pattern, char *subject, ucell slots, regmatch_t *subs, ucell flags)
{
	regex_t re;
	regcomp(&re, pattern, flags);
	ubyte match = regexec(&re, subject, slots, subs, 0);
	regfree(&re);
	return match;
}
// extract a matchead substring from a posix regex regmatch_t
char* regsubstr(char *subject, regmatch_t *subs, int slot)
{
	if (subs[slot].rm_so == -1) return NULL;
	int len = subs[slot].rm_eo - subs[slot].rm_so;
	char *pad = allocate(len+1);
	strncpy(pad, subject + subs[slot].rm_so, len);
	pad[len] = '\0';
	return pad;
}
// simple file io
void blurt(const char *name, void *data)
{
	ucell dlen = strlen(data);
	FILE *f = fopen(name, "w+");
	assert(f, "file open failed: %s", name);
	assert(fwrite(data, sizeof(char), strlen(data), f) == dlen, "file write failed");
	fclose(f);
}
ucell suck(FILE *src, byte *pad, ucell len, byte stop)
{
	ucell ptr = 0;
	for (;;)
	{
		byte c = fgetc(src);
		if (c == EOF)
			break;

		pad[ptr++] = c;

		if (len == ptr || c == stop)
			break;
	}
	return ptr;
}
byte* slurp(const char *name, ucell *len)
{
	ucell blocks = 0;
	FILE *f = fopen(name, "r");
	assert(f, "file open failed: %s", name);
	byte *pad = (byte*)allocate(BLOCK);
	ucell read = 0;
	for (;;)
	{
		read = suck(f, pad+(BLOCK*blocks), BLOCK, EOF);
		if (read < BLOCK) break;
		pad = reallocate(pad, BLOCK * (++blocks+1));
	}
	pad[BLOCK * blocks + read] = '\0';
	if (len) *len = BLOCK * blocks + read;
	fclose(f);
	return pad;
}
void stack_push(stack *s, void *v)
{
	if (s->depth == s->limit)
	{
		s->limit += STACK;
		s->items = reallocate(s->items, s->limit * sizeof(void*));
	}
	s->items[s->depth++] = v;
}
void* stack_pop(stack *s)
{
	assert(s->depth > 0, "stack underflow");
	return s->items[--(s->depth)];
}
void* stack_top(stack *s)
{
	assert(s->depth > 0, "stack underflow");
	return s->items[s->depth-1];
}
void* stack_get(stack *s, ucell slot)
{
	assert(s->depth >= slot, "stack underflow");
	return s->items[s->depth-1-slot];
}
void* stack_shift(stack *s)
{
	void *item;
	assert(s->depth > 0, "stack underflow");
	item = s->items[0]; s->depth--;
	memmove(&(s->items[0]), &(s->items[1]), s->depth * sizeof(void*));
	return item;
}
void stack_shove(stack *s, void *item)
{
	if (s->depth == s->limit)
	{
		s->limit += STACK;
		s->items = reallocate(s->items, s->limit * sizeof(void*));
	}
	memmove(&(s->items[1]), &(s->items[0]), s->depth * sizeof(void*));
	s->items[0] = item; s->depth++;
}
void stack_del(stack *s, ucell index)
{
	assert(index < s->depth, "invalid stack item");
	memmove(&(s->items[index]), &(s->items[index+1]), (s->depth - index) * sizeof(void*));
	s->depth--;
}
int stack_find(stack *s, void *item)
{
	int i;
	for (i = 0; i < s->depth; i++)
		if (s->items[i] == item) return i;
	return -1;
}
void stack_discard(stack *s, void *item)
{
	int index = stack_find(s, item);
	if (index >= 0) stack_del(s, index);
}
stack *stack_create()
{
	stack *s = (stack*)allocate(sizeof(stack));
	s->items = allocate(sizeof(void*)*STACK);
	s->limit = STACK;
	s->depth = 0;
	return s;
}
void stack_free(stack *s)
{
	free(s->items);
	free(s);
}
hash* hash_create()
{
	int i;
	hash *h = (hash*)allocate(sizeof(hash));
	for (i = 0; i < HASH; i++)
		h->chains[i] = NULL;
	return h;
}
ubyte hash_index(char *s)
{
	ucell h = 0, i = 0;
	while (s[i])
		h = 33 * h + tolower(s[i++]);
	return h % HASH;
}
bucket* hash_find(hash *h, char *key)
{
	ubyte index = hash_index(key);
	bucket *b = h->chains[index];
	while (b)
	{
		if (strcasecmp(key, b->key) == 0)
			return b;
		b = b->next;
	}
	return NULL;
}
void* hash_get(hash *h, char *key)
{
	bucket *b = hash_find(h, key);
	return b ? b->val: NULL;
}
void* hash_set(hash *h, char *key, void *val)
{
	ubyte index = hash_index(key);
	bucket *b = h->chains[index];
	void* old = NULL;
	while (b)
	{
		if (strcasecmp(key, b->key) == 0)
		{
			old = b->val;
			b->val = val;
			return old;
		}
		b = b->next;
	}
	b = allocate(sizeof(bucket));
	b->key = strdup(key);
	b->val = val;
	b->next = h->chains[index];
	h->chains[index] = b;
	return old;
}
void* hash_del(hash *h, char *key)
{
	ubyte index = hash_index(key);
	bucket *b = h->chains[index], *l = b;
	void *old = NULL;
	while (b)
	{
		if (strcasecmp(key, b->key) == 0)
		{
			if (l == h->chains[index])
				h->chains[index] = b->next;
			else	l->next = b->next;
			old = b->val;
			free(b->key);
			free(b);
			return old;
		}
		l = b;
		b = b->next;
	}
	return old;
}
void hash_free(hash *h, bool vals)
{
	int i; bucket *b, *n;
	for (i = 0; i < HASH; i++)
	{
		b = h->chains[i];
		while (b)
		{
			n = b->next;
			free(b->key);
			if (vals) free(b->val);
			free(b);
			b = n;
		}
	}
}
void hash_iterate(hash *h, void (*cb)(hash*, char*, void *val))
{
	int i; bucket *b;
	stack *buckets = stack_create();
	for (i = 0; i < HASH; i++)
	{
		b = h->chains[i];
		while (b)
		{
			stack_push(buckets, b);
			b = b->next;
		}
	}
	FOR_STACK (b, buckets, bucket*, i)
		cb(h, b->key, b->val);
	stack_free(buckets);
}
stack* strsplit(char *addr, str_cb_chr cb)
{
	stack *s = stack_create();
	while (addr && *addr)
	{
		stack_push(s, strnext(&addr, cb));
		strskip(&addr, cb);
	}
	return s;
}
stack* strsplitthese(char *addr, char *join)
{
	stack *s = stack_create();
	while (addr && *addr)
	{
		stack_push(s, strnextthese(&addr, join));
		strskipthese(&addr, join);
	}
	return s;
}

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}
pid_t exec_cmd_io(const char *command, int *infp, int *outfp)
{
	signal(SIGCHLD, catch_exit);
	int p_stdin[2], p_stdout[2];
	pid_t pid;

	if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
		return -1;
	pid = fork();
	if (pid < 0)
		return pid;
	else if (pid == 0)
	{
		close(p_stdin[WRITE]);
		dup2(p_stdin[READ], READ);
		close(p_stdout[READ]);
		dup2(p_stdout[WRITE], WRITE);
		execlp("/bin/sh", "sh", "-c", command, NULL);
		exit(EXIT_FAILURE);
	}
	if (infp == NULL)
		close(p_stdin[WRITE]);
	else
		*infp = p_stdin[WRITE];
	if (outfp == NULL)
		close(p_stdout[READ]);
	else
		*outfp = p_stdout[READ];
	close(p_stdin[READ]);
	close(p_stdout[WRITE]);
	return pid;
}
pid_t exec_cmd(char *cmd)
{
	pid_t pid;
	signal(SIGCHLD, catch_exit);
	pid = fork();
	if (!pid)
	{
		setsid();
		execlp("/bin/sh", "sh", "-c", cmd, NULL);
		exit(EXIT_FAILURE);
	}
	return pid;
}
void ppm_pixel(ppm *ppm, int x, int y, ubyte r, ubyte g, ubyte b)
{
	if (x > -1 && y > -1 && x < ppm->w && y < ppm->h)
	{
		int offset = (y * ppm->w) + x;
		ubyte *pixel = &ppm->data[offset*3];
		pixel[0] = r; pixel[1] = g; pixel[2] = b;
	}
}
ppm* ppm_create(int w, int h, ubyte r, ubyte g, ubyte b)
{
	ppm *p = allocate(sizeof(ppm));
	p->w = w; p->h = h;
	p->data = allocate(w*h*3);
	int i, j;
	for (i = 0; i < h; i++)
		for (j = 0; j < w; j++)
			ppm_pixel(p, j, i, r, g, b);
	return p;
}
void ppm_save(ppm *ppm, const char *name)
{
	autostr s; str_create(&s);
	str_print(&s, BLOCK*64, "P3\n#test\n%d %d\n255\n", ppm->w, ppm->h);
	int i, pixels = ppm->w * ppm->h;
	for (i = 0; i < pixels; i++)
	{
		ubyte *pixel = &ppm->data[i*3];
		str_print(&s, NOTE, "%d %d %d\n", pixel[0], pixel[1], pixel[2]);
	}
	blurt(name, s.pad);
	str_free(&s);
}
void ppm_free(ppm *ppm)
{
	free(ppm->data);
	free(ppm);
}
// simple args processing
hash* args_to_hash(int argc, char *argv[])
{
	int i; char *key = NULL;
	hash *h = hash_create();
	for (i = 0; i < argc; i++)
	{
		char *s = argv[i];
		if (*s == '-' && !isdigit(*(s+1)))
		{
			if (key) hash_set(h, key, NULL);
			key = s+1;
		} else
		if (key)
		{
			hash_set(h, key, s);
			key = NULL;
		}
	}
	if (key) hash_set(h, key, NULL);
	return h;
}
bucket* hash_find_one(hash *h, char *csv)
{
	bucket *b = NULL;
	while (*csv && !b)
	{
		char *key = strnextthese(&csv, ",");
		strskipthese(&csv, ",");
		b = hash_find(h, key);
		free(key);
	}
	return b;
}
char* hash_get_one(hash *h, char *csv)
{
	bucket *b = hash_find_one(h, csv);
	return b ? b->val: NULL;
}
char* hash_expect(hash *h, char *keys, char *pattern, int maxsubs, regmatch_t *subs)
{
	char *val = hash_get_one(h, keys);
	return (val && regmatch(pattern, val, maxsubs, subs, REG_EXTENDED|REG_ICASE) == 0) ? val: NULL;
}
char* hash_encode(hash *h)
{
	int i;
	autostr s; str_create(&s);
	for (i = 0; i < HASH; i++)
	{
		bucket *b = h->chains[i];
		while (b)
		{
			str_print(&s, BLOCK*64, "%d %s %d %s\n", strlen(b->key), b->key,
				b->val ? strlen(b->val): -1, b->val ? b->val: "");
			b = b->next;
		}
	}
	return s.pad;
}
hash* hash_decode(char *s)
{
	hash *h = hash_create();
	while (*s)
	{
		char *key, *val = NULL; int len = 0;
		len = strtol(s, &s, 10); s++;
		key = strpull(s, len);
		s += len; strskip(&s, isspace);
		len = strtol(s, &s, 10); s++;
		if (len >= 0)
		{
			val = strpull(s, len);
			s += len; strskip(&s, isspace);
		}
		hash_set(h, key, val);
		free(key);
	}
	return h;
}
// dump mem region to string. decent compression if non-random data.
char* mem2str(void *addr, ucell len)
{
	autostr s; str_create(&s);
	int i, c = 0; ubyte v = 0, *p = addr;
	v = p[0]; c++; str_require(&s, BLOCK);
	for (i = 1; i < len; i++)
	{
		if (p[i] != v)
		{
			str_print(&s, 32, "%x %x ", c, v);
			v = p[i]; c = 0;
		}
		c++;
	}
	if (c) str_print(&s, 32, "%x %x ", c, v);
	return s.pad;
}
void str2mem(void *addr, ucell len, char *s)
{
	ubyte *p = addr, *e = p + len;
	while (*s && p < e)
	{
		int c = strtol(s, &s, 16); if (*s) s++;
		ubyte v = strtol(s, &s, 16); if (*s) s++;
		c = MIN(c, e-p); memset(p, v, c); p += c;
	}
}
// bms
ubyte* bm_create(ucell bits)
{
	ubyte *b = allocate((bits/8)+1);
	memset(b, 0, (bits/8)+1);
	return b;
}
void bm_set(ubyte *b, ucell item)
{
	b[item / 8] |= 1 << (item % 8);
}
void bm_clr(ubyte *b, ucell item)
{
	b[item / 8] &= ~(1 << (item % 8));
}
bool bm_chk(ubyte *b, ucell item)
{
	return b[item / 8] & (1 << (item % 8)) ? 1:0;
}
void bm_flip(ubyte *b, ucell item)
{
	b[item / 8] ^= 1 << (item % 8);
}
