#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* config */
const int gap = 1;
const int titlebar_height = 14;
const char *font_name = "fixed,-*";

Display *dpy;
Window   root;
struct size_hint {
   int base_width, base_height;
   int width_inc,  height_inc;
   int min_width,  min_height;
};
struct client {
   Window client_window;
   Window title_window;
   int x, y;
   unsigned int w, h, b;
   char *name;
   struct size_hint hints;
   Bool has_focus;
} clients[80];
int nr_clients;
int screen_width, screen_height;
struct rect { int t, b, l, r; };
XFontSet fset;

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
   int title_width = c->w + 2 * c->b - 2;
   Window w = new_child(root, c->x, c->y - titlebar_height - 3,
                              title_width, titlebar_height);
   c->title_window = w;
   XSelectInput(dpy, w, ButtonPressMask | ButtonReleaseMask |
                                          ButtonMotionMask | EnterWindowMask | ExposureMask);
}

void
get_geometry_xywh(struct client *c)
{
   Window r;
   unsigned int d;

   XGetGeometry(dpy, c->client_window, &r,
         &c->x, &c->y, &c->w, &c->h, &c->b, &d);
}

void
get_size_hints(struct client *c)
{
   XSizeHints hints;
   long supplied;

   XGetWMNormalHints(dpy, c->client_window, &hints, &supplied);
   c->hints.base_width  = (supplied & PBaseSize) ? hints.base_width  :
                          (supplied & PMinSize)  ? hints.min_width   : 32;
   c->hints.base_height = (supplied & PBaseSize) ? hints.base_height :
                          (supplied & PMinSize)  ? hints.min_height  : 32;
   c->hints.width_inc   = (supplied & PResizeInc) ? hints.width_inc  : 0;
   c->hints.height_inc  = (supplied & PResizeInc) ? hints.height_inc : 0;
   c->hints.min_width   = (supplied & PMinSize)  ? hints.min_width   : 32;
   c->hints.min_height  = (supplied & PMinSize)  ? hints.min_height  : 32;
   if (c->hints.min_width  == 0) c->hints.min_width  = 32;
   if (c->hints.min_height == 0) c->hints.min_height = 32;
}

int
detect_collision(struct client *p, struct client *q, struct client *f)
{
   int xhit, yhit;

   for (; q < p; q++) {
      xhit = yhit = 0;
      if ((f->x <= q->x) && ((f->x + f->w + 2 * f->b) > q->x)) xhit = 1;
      if ((q->x <= f->x) && ((q->x + q->w + 2 * q->b) > f->x)) xhit = 1;
      if ((f->y <= q->y) && ((f->y + f->h + 2 * f->b) > q->y)) yhit = 1;
      if ((q->y <= f->y) && ((q->y + q->h + 2 * q->b) > f->y)) yhit = 1;
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

   free(c->name);
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
init_one_client_set_name(struct client *c, Window x)
{
   if (fset) {
      XTextProperty text_prop;
      char **list;
      int n;

      XGetWMName(dpy, x, &text_prop);
      XmbTextPropertyToTextList(dpy, &text_prop, &list, &n);
      c->name = strdup(*list ?: "");
      XFreeStringList(list);
   } else {
      char *name;

      XFetchName(dpy, x, &name);
      c->name = strdup(name ?: "");
      if (name) XFree(name);
   }
}

void
init_one_client(struct client *c, Window x)
{
   c->client_window = x;
   get_geometry_xywh(c);
   get_size_hints(c);
   if (c->y < titlebar_height + 3) {
      c->y = titlebar_height + 3;
      XMoveWindow(dpy, c->client_window, c->x, c->y);
   }
   c->has_focus = False;
   init_one_client_set_name(c, x);
   new_title(c);
   restack_client(c);
   XSelectInput(dpy, c->client_window, EnterWindowMask | PropertyChangeMask | FocusChangeMask);
   XMapWindow(dpy, c->title_window);
}

Bool
window_should_be_ignored(Window w)
{
   XWindowAttributes attr;

   XGetWindowAttributes(dpy, w, &attr);
   if (attr.class != InputOutput)
      return True;
   if (attr.map_state != IsViewable)
      return True;
   if (attr.override_redirect)
      return True;
   return False;
}

void
init_clients(Window root)
{
   struct client *c;
   Window rt, par, *child, focus;
   unsigned int i, n;
   int revert;

   XGetInputFocus(dpy, &focus, &revert);
   XQueryTree(dpy, root, &rt, &par, &child, &n);
   for (i = 0; i < n; i++) {
      if (window_should_be_ignored(child[i]))
         continue;

      c = &clients[nr_clients++];
      init_one_client(c, child[i]);
      if (focus == child[i])
         c->has_focus = True;
   }
   if (child)
      XFree(child);
}

enum { NONE, EAST, NORTH, WEST, SOUTH };
int
direction_collision(struct rect r0, struct rect r1)
{
   int xhit, yhit;

   xhit = yhit = 0;
   if ((r0.l <= r1.l) && (r0.r + gap > r1.l)) xhit = 1;
   if ((r1.l <= r0.l) && (r1.r + gap > r0.l)) xhit = 1;
   if ((r0.t <= r1.t) && (r0.b + gap > r1.t)) yhit = 1;
   if ((r1.t <= r0.t) && (r1.b + gap > r0.t)) yhit = 1;

   if (xhit && yhit) return NONE;

   if (yhit && r0.l < r1.l) return EAST;
   if (xhit && r0.t > r1.t) return NORTH;
   if (yhit && r0.l > r1.l) return WEST;
   if (xhit && r0.t < r1.t) return SOUTH;

   return NONE;
}

void
fix_range_one(struct rect *r, struct rect *s, int dir)
{
   switch (dir) {
   case EAST:  if (r->r > s->l - gap) r->r = s->l - gap; break;
   case NORTH: if (r->t < s->b - gap) r->t = s->b + gap; break;
   case WEST:  if (r->l < s->r - gap) r->l = s->r + gap; break;
   case SOUTH: if (r->b > s->t - gap) r->b = s->t - gap; break;
   }
}

struct rect
client_rect(struct client *c)
{
   struct rect r;

   r.l = c->x;
   r.t = c->y - titlebar_height - 3;
   r.r = c->x + c->w + 2 * c->b;
   r.b = c->y + c->h + 2 * c->b;

   return r;
}

void
fix_range(struct rect *r, struct client *c)
{
   int i;
   struct rect t = client_rect(c);

   for (i = 0; i < nr_clients; i++) {
      struct rect s = client_rect(&clients[i]);
      int dir = direction_collision(t, s);
      fix_range_one(r, &s, dir);
   }
   if (r->b < t.b) return;
   r->b -= c->h + 2 * c->b + 1;
}

void
draw_title(struct client *c)
{
   GC gc = XCreateGC(dpy, c->title_window, 0, NULL);
   int x = c->has_focus ? 12 : 4;

   XClearWindow(dpy, c->title_window);
   if (fset)
      XmbDrawString(dpy, c->title_window, fset, gc, x, 10, c->name, strlen(c->name));
   else
      XDrawString(dpy, c->title_window, gc, x, 10, c->name, strlen(c->name));
   if (c->has_focus)
      XFillRectangle(dpy, c->title_window, gc, 4, 4, 4, 4);
   XFreeGC(dpy, gc);
}

void
draw_geom_on_titlebar(struct client *c)
{
   GC gc = XCreateGC(dpy, c->title_window, 0, NULL);
   char buf[32]; /* 9999x9999+9999+9999_ */
   unsigned int cw = c->w, ch = c->h;

   if (c->hints.width_inc > 0) {
      cw -= c->hints.base_width;
      cw /= c->hints.width_inc;
   }
   if (c->hints.height_inc > 0) {
      ch -= c->hints.base_height;
      ch /= c->hints.height_inc;
   }
   sprintf(buf, "%dx%d+%d+%d", cw, ch, c->x, c->y);
   XClearWindow(dpy, c->title_window);
   XDrawString(dpy, c->title_window, gc, 4, 10, buf, strlen(buf));
   XFreeGC(dpy, gc);
}

void
apply_geom(struct client *c)
{
   int t_y = c->y - titlebar_height - 3;
   int t_w = c->w + 2 * c->b - 2;

   XMoveResizeWindow(dpy, c->client_window, c->x, c->y, c->w, c->h);
   XMoveResizeWindow(dpy, c->title_window,  c->x, t_y,  t_w,  titlebar_height);
}

struct rect
client_rect_body(struct client *c)
{
   struct rect o;

   o.l = c->x;
   o.t = c->y;
   o.r = c->x + (int)c->w;
   o.b = c->y + (int)c->h;

   return o;
}

struct rect
root_rect(void)
{
   struct rect r;

   r.t = r.l = 0;
   r.r = screen_width;
   r.b = screen_height;

   return r;
}

void
titlebar_on_expose(struct client *c, Window w_dragging)
{
   if (c->title_window == w_dragging)
      draw_geom_on_titlebar(c);
   else
      draw_title(c);
}

void
init_set_locale(void)
{
   int n_missing, i;
   char **l_missing;
   char *def_str;

   if (!setlocale(LC_CTYPE, "")) return;
   if (!XSupportsLocale()) return;
   fset = XCreateFontSet(dpy, font_name, &l_missing, &n_missing, &def_str);
   if (!fset) return;

   for (i = 0; i < n_missing; i++)
      fprintf(stderr, "wminor: missing font `%s'\n", l_missing[i]);

   XFreeStringList(l_missing);
}

int
main(void)
{
   int press_button = 0;
   int dx, dy;

   dpy = XOpenDisplay(NULL);
   if (!dpy) return 1;

   root = DefaultRootWindow(dpy);
   XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
   init_set_locale();
   init_clients(root);

   screen_width  = XDisplayWidth(dpy, DefaultScreen(dpy));
   screen_height = XDisplayHeight(dpy, DefaultScreen(dpy));

   for (;;) {
      XEvent e;
      Window w, w_dragging;
      struct client *c;
      struct rect r, o;
      int shift_flag, shift_mode;

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
         if (press_button == 3 && e.xbutton.button == 2) {
            r = root_rect();
            fix_range(&r, c);
         }
         if (press_button) break;
         w_dragging = w;
         shift_flag = e.xbutton.state & ShiftMask;
         shift_mode = 0;
         press_button = e.xbutton.button;
         dx = c->x - e.xbutton.x_root;
         dy = c->y - e.xbutton.y_root;
         o = client_rect_body(c);
         r = root_rect();
         if (press_button == 2)
            fix_range(&r, c);
         draw_geom_on_titlebar(c);
         raise_upper(sort_clients(c));
         break;
      case ButtonRelease:
         w = e.xbutton.window;
         c = find_client_from_title(w);
         if (!c) break;
         if (press_button != e.xbutton.button) break;
         w_dragging = 0;
         press_button = 0;
         draw_title(c);
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
            if ((shift_flag &= e.xmotion.state)) {
               shift_mode = 0;
               if (c->x > o.l) shift_mode |= 1;
               if (c->y > o.t) shift_mode |= 2;
            }
            if (shift_mode & 1) c->x = o.l;
            if (shift_mode & 2) c->y = o.t;
            if (c->x < r.l)     c->x = r.l;
            if (c->y < r.t + titlebar_height + 3)
               c->y = r.t + titlebar_height + 3;
            if (press_button == 3) {
               int d;
               c->w = (o.r - c->x < 0) ? 0 : o.r - c->x;
               if (c->hints.width_inc > 0) {
                  d = c->w;
                  d -= c->hints.base_width;
                  d %= c->hints.width_inc;
                  c->w -= d;
                  c->x += d;
               }
               if (c->w < c->hints.min_width) c->w = c->hints.min_width;
               c->h = (o.b - c->y < 0) ? 0 : o.b - c->y;
               if (c->hints.height_inc > 0) {
                  d = c->h;
                  d -= c->hints.base_height;
                  d %= c->hints.height_inc;
                  c->h -= d;
                  c->y += d;
               }
               if (c->h < c->hints.min_height) c->h = c->hints.min_height;
            }
            if (c->x > r.r - c->w - 2 * c->b)
               c->x = r.r - c->w - 2 * c->b;
            if (c->y > r.b + 1)
               c->y = r.b + 1;
            apply_geom(c);
            draw_geom_on_titlebar(c);
         }
         break;

      case EnterNotify:
         w = e.xcrossing.window;
         c = find_client_from_title(w);
         if (!c) c = find_client(w);
         if (!c) break;
         XSetInputFocus(dpy, c->client_window, RevertToPointerRoot, CurrentTime);
         break;

      case FocusIn:
         w = e.xfocus.window;
         c = find_client(w);
         if (!c) break;
         c->has_focus = True;
         titlebar_on_expose(c, w_dragging);
         break;

      case FocusOut:
         w = e.xfocus.window;
         c = find_client(w);
         if (!c) break;
         c->has_focus = False;
         titlebar_on_expose(c, w_dragging);
         break;

      case PropertyNotify:
         w = e.xproperty.window;
         c = find_client(w);
         if (e.xproperty.atom != XA_WM_NAME) break;
         free(c->name);
         init_one_client_set_name(c, w);
         titlebar_on_expose(c, w_dragging);
         break;

      case Expose:
         w = e.xexpose.window;
         if ((c = find_client_from_title(w)))
            titlebar_on_expose(c, w_dragging);
      }
   }
   XCloseDisplay(dpy);
   return 0;
}
