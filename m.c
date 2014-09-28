#include <X11/Xlib.h>
#include <stdio.h>
Display *dpy;
Window   root;
struct client {
   Window client_window;
   Window title_window;
   int x, y;
   unsigned int w, h, b;
} clients[80];
int nr_clients;
int screen_width, screen_height;
struct rect { int t, b, l, r; };

Window
new_child(Window parent, int x, int y, int w, int h)
{
   return XCreateSimpleWindow(dpy, parent, x, y, w, h, 1,
      BlackPixel(dpy, DefaultScreen(dpy)),
      WhitePixel(dpy, DefaultScreen(dpy)));
}

void
new_title(struct client *c)
{
   Window w = new_child(root, c->x, c->y - 23, c->w, 20);
   c->title_window = w;
   XSelectInput(dpy, w, ButtonPressMask | ButtonReleaseMask |
                                          ButtonMotionMask);
}

void
get_geometry_xywh(struct client *c)
{
   Window r;
   unsigned int d;

   XGetGeometry(dpy, c->client_window, &r,
         &c->x, &c->y, &c->w, &c->h, &c->b, &d);
}

int
detect_collision(struct client *p, struct client *q, struct client *f)
{
   int xhit, yhit;

   for (; q < p; q++) {
      xhit = yhit = 0;
      if ((f->x <= q->x) && ((f->x + f->w + 2 * f->b) > q->x)) xhit = 1;
      if ((q->x <= f->x) && ((q->x + q->w + 2 * f->b) > f->x)) xhit = 1;
      if ((f->y <= q->y) && ((f->y + f->h + 2 * f->b) > q->y)) yhit = 1;
      if ((q->y <= f->y) && ((q->y + q->h + 2 * f->b) > f->y)) yhit = 1;
      if (xhit && yhit) return 1;
   }
   return 0;
}

int
sort_clients(struct client *y)
{
   struct client *w = clients, q[80], *p = q;
   int i, j, k;

   i = j = k = y - clients;

   *p++ = w[i++];
   for (; i < nr_clients; i++)
      if (detect_collision(p, q, &w[i]))
         *p++ = w[i];
      else
         w[j++] = w[i];

   p = q;
   for (; j < nr_clients; j++)
      w[j] = *p++;

   return k;
}

void
raise_upper(int x)
{
   Window w[160];
   int i, n;

   n = nr_clients - x;

   for (i = 0; i < n; i++) {
      w[2 * i]     = clients[nr_clients - 1 - i].client_window;
      w[2 * i + 1] = clients[nr_clients - 1 - i].title_window; }
   XRestackWindows(dpy, w, 2 * n);
}

struct client *
find_client(Window w)
{
   int i;

   for (i = 0; i < nr_clients; i++)
      if (clients[i].client_window == w)
         return &clients[i];
   return NULL;
}

struct client *
find_client_from_title(Window w)
{
   int i;

   for (i = 0; i < nr_clients; i++)
      if (clients[i].title_window == w)
         return &clients[i];
   return NULL;
}

void
remove_client(struct client *c)
{
   int i;

   for (i = c - clients; i < nr_clients; i++)
      clients[i] = clients[i + 1];
   nr_clients--;
}

void
restack_client(struct client *c)
{
   Window w[2];

   w[0] = c->client_window;
   w[1] = c->title_window;
   XRestackWindows(dpy, w, 2);
}

void
init_one_client(struct client *c, Window x)
{
   c->client_window = x;
   get_geometry_xywh(c);
   if (c->y < 23) {
      c->y = 23;
      XMoveWindow(dpy, c->client_window, c->x, c->y);
   }
   new_title(c);
   restack_client(c);
   XMapWindow(dpy, c->title_window);
}

void
init_clients(Window root)
{
   Window rt, par, *child;
   unsigned int i, n;

   XQueryTree(dpy, root, &rt, &par, &child, &n);
   for (i = 0; i < n; i++)
      init_one_client(&clients[nr_clients++], child[i]);
}

enum { NONE, EAST, NORTH, WEST, SOUTH };
int
direction_collision(struct client *p, struct client *q)
{
   int xhit, yhit;

   xhit = yhit = 0;
   if ((p->x <= q->x) && ((p->x + p->w + 2 * p->b) > q->x)) xhit = 1;
   if ((q->x <= p->x) && ((q->x + q->w + 2 * p->b) > p->x)) xhit = 1;
   if ((p->y <= q->y) && ((p->y + p->h + 2 * p->b) > q->y)) yhit = 1;
   if ((q->y <= p->y) && ((q->y + q->h + 2 * p->b) > p->y)) yhit = 1;

   if (xhit && yhit) return NONE;

   if (yhit && p->x < q->x) return EAST;
   if (xhit && p->y > q->y) return NORTH;
   if (yhit && p->x > q->x) return WEST;
   if (xhit && p->y < q->y) return SOUTH;

   return NONE;
}

void
fix_range_one(struct rect *r, struct rect *s, int dir)
{
   switch (dir) {
   case EAST:  if (r->r > s->l) r->r = s->l; break;
   case NORTH: if (r->t < s->b) r->t = s->b; break;
   case WEST:  if (r->l < s->r) r->l = s->r; break;
   case SOUTH: if (r->b > s->t) r->b = s->t; break;
   }
}

struct rect
client_rect(struct client *c)
{
   struct rect r;

   r.l = c->x;
   r.t = c->y - 23;
   r.r = c->x + c->w + 2 * c->b;
   r.b = c->y + c->h + 2 * c->b;

   return r;
}

void
fix_range(struct rect *r, struct client *c)
{
   int i;

//printf("[%d, %d ; %d, %d]->", r->l, r->t, r->r, r->b);
//{ struct rect t = client_rect(c);
//printf("[%d, %d ; %d, %d]\n", t.l,  t.t,  t.r,  t.b); }

   for (i = 0; i < nr_clients; i++) {
      struct rect s = client_rect(&clients[i]);
      int dir = direction_collision(c, &clients[i]);
      fix_range_one(r, &s, dir);

//printf(" - [%d, %d ; %d, %d]",   r->l, r->t, r->r, r->b);
//printf(" < [%d, %d ; %d, %d]\n", s.l,  s.t,  s.r,  s.b);
   }

   r->b -= c->h + 2 * c->b + 1;
//printf("[%d, %d ; %d, %d]\n", r->l, r->t, r->r, r->b);
}

int
main(void)
{
   int press_button = 0;
   int dx, dy;
   int rx, ry;

   dpy = XOpenDisplay(NULL);
   if (!dpy) return 1;

   root = DefaultRootWindow(dpy);
   XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
   init_clients(root);

   screen_width  = XDisplayWidth(dpy, DefaultScreen(dpy));
   screen_height = XDisplayHeight(dpy, DefaultScreen(dpy));

   for (;;) {
      XEvent e;
      Window w;
      struct client *c;
      struct rect r;

      XNextEvent(dpy, &e);

      switch (e.type) {
      case MapRequest:
         c = &clients[nr_clients++];
         w = e.xmaprequest.window;
         init_one_client(c, w);
         XMapWindow(dpy, w);
         break;
      case UnmapNotify:
         w = e.xunmap.window;
         c = find_client(w);
         if (!c) break;
         XUnmapWindow(dpy, c->title_window);
         remove_client(c); c = NULL;
         break;
      case DestroyNotify:
         w = e.xdestroywindow.window;
         c = find_client(w);
         if (!c) break;
         XUnmapWindow(dpy, c->title_window);
         remove_client(c); c = NULL;
         break;

      case ButtonPress:
         w = e.xbutton.window;
         c = find_client_from_title(w);
         if (!c) break;
         if (press_button) break;
         press_button = e.xbutton.button;
         dx = c->x - e.xbutton.x_root;
         dy = c->y - e.xbutton.y_root;
         rx = c->x + (int)c->w;
         ry = c->y + (int)c->h;
         r.t = r.l = 0;
         r.r = screen_width;
         r.b = screen_height;
         if (press_button == 2)
            fix_range(&r, c);
         raise_upper(sort_clients(c));
         break;
      case ButtonRelease:
         w = e.xbutton.window;
         c = find_client_from_title(w);
         if (!c) break;
         if (press_button != e.xbutton.button) break;
         press_button = 0;
         break;
      case MotionNotify:
         w = e.xmotion.window;
         c = find_client_from_title(w);
         if (!c) break;
         switch (press_button) {
         case Button1:
         case Button2:
         case Button3:
            c->x = e.xmotion.x_root + dx;
            c->y = e.xmotion.y_root + dy;
            if (c->x < r.l)      c->x = r.l;
            if (c->y < r.t + 23) c->y = r.t + 23;
            if (c->x > r.r - c->w - 2)
               c->x = r.r - c->w - 2;
            if (c->y > r.b + 1)
               c->y = r.b + 1;
            if (press_button == 3) {
               c->w = (rx - c->x < 32) ? 32 : rx - c->x;
               c->h = (ry - c->y < 32) ? 32 : ry - c->y; }
            XMoveResizeWindow(dpy, c->client_window,
                  c->x, c->y,      c->w, c->h);
            XMoveResizeWindow(dpy, c->title_window,
                  c->x, c->y - 23, c->w, 20);
         }
      }
   }
   XCloseDisplay(dpy);
   return 0;
}
