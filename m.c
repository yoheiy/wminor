#include <X11/Xlib.h>
#include <stdio.h>
Display *dpy;
Window   root;
struct client {
   Window client_window;
   Window title_window;
   int x, y;
} clients[80];
int nr_clients;

Window
new_child(Window parent, int x, int y, int w, int h)
{
   return XCreateSimpleWindow(dpy, parent, x, y, w, h, 1,
      BlackPixel(dpy, DefaultScreen(dpy)),
      WhitePixel(dpy, DefaultScreen(dpy)));
}

Window
new_title(struct client *c)
{
   Window w = new_child(root, c->x, c->y - 23, 200, 20);
   XSelectInput(dpy, w, ButtonPressMask | ButtonReleaseMask |
                                          Button1MotionMask);
   return w;
}

void
get_geometry_xy(struct client *c)
{
   Window r;
   unsigned int w, h, b, d;

   XGetGeometry(dpy, c->client_window, &r, &c->x, &c->y, &w, &h, &b, &d);
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
   *c = clients[--nr_clients];
}

int
main(void)
{
   dpy = XOpenDisplay(NULL);
   if (!dpy) return 1;

   root = DefaultRootWindow(dpy);
   XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

   for (;;) {
      XEvent e;
      Window w, t;
      struct client *c;
      int dx, dy;

      XNextEvent(dpy, &e);

      switch (e.type) {
      case MapRequest:
         c = &clients[nr_clients++];
         w = e.xmaprequest.window;
printf("MapRequest <%x>\n", (int)w);
         c->client_window = w;
         get_geometry_xy(c);
         t = new_title(c);
         c->title_window = t;
         XMapRaised(dpy, t);
         XMapRaised(dpy, w);
         break;
      case UnmapNotify:
         w = e.xunmap.window;
printf("UnmapNotify <%x>\n", (int)w);
         c = find_client(w);
         if (!c) break;
printf("Client <%p> unmapped.\n", c);
         XUnmapWindow(dpy, c->title_window);
         remove_client(c); c = NULL;
printf("number of clients = %d\n", nr_clients);
         break;
      case DestroyNotify:
         w = e.xdestroywindow.window;
printf("DestroyNotify <%x>\n", (int)w);
         c = find_client(w);
         if (!c) break;
printf("Client <%p> destroyed.\n", c);
         XUnmapWindow(dpy, c->title_window);
         remove_client(c); c = NULL;
printf("number of clients = %d\n", nr_clients);
         break;
      case ButtonPress:
         w = e.xbutton.window;
printf("ButtonPress <%x>\n", (int)w);
         c = find_client_from_title(w);
         if (!c) break;
         dx = c->x - e.xbutton.x_root;
         dy = c->y - e.xbutton.y_root;
printf("title of <%p> button press.\n", c);
         break;
      case ButtonRelease:
         w = e.xbutton.window;
printf("ButtonRelease <%x>\n", (int)w);
         c = find_client_from_title(w);
         if (!c) break;
printf("title of <%p> button release.\n", c);
         break;
      case MotionNotify:
         w = e.xmotion.window;
         c = find_client_from_title(w);
         if (!c) break;
         c->x = e.xmotion.x_root + dx;
         c->y = e.xmotion.y_root + dy;
         XMoveWindow(dpy, c->client_window, c->x, c->y);
         XMoveWindow(dpy, c->title_window,  c->x, c->y - 23);
      }
   }
   XCloseDisplay(dpy);
   return 0;
}
