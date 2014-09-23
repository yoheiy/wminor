#include <X11/Xlib.h>
#include <stdio.h>
Display *dpy;
Window   root;
struct client {
   Window client_window;
   Window title_window;
   int x, y;
   unsigned int w, h;
} clients[80];
int nr_clients;
int screen_width, screen_height;

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
                                          ButtonMotionMask);
   return w;
}

void
get_geometry_xywh(struct client *c)
{
   Window r;
   unsigned int b, d;

   XGetGeometry(dpy, c->client_window, &r, &c->x, &c->y, &c->w, &c->h, &b, &d);
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
   int press_button = 0;
   int dx, dy;
   unsigned int ow, oh;

   dpy = XOpenDisplay(NULL);
   if (!dpy) return 1;

   root = DefaultRootWindow(dpy);
   XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

   screen_width  = XDisplayWidth(dpy, DefaultScreen(dpy));
   screen_height = XDisplayHeight(dpy, DefaultScreen(dpy));

   for (;;) {
      XEvent e;
      Window w, t;
      struct client *c;

      XNextEvent(dpy, &e);

      switch (e.type) {
      case MapRequest:
         c = &clients[nr_clients++];
         w = e.xmaprequest.window;
printf("MapRequest <%x>\n", (int)w);
         c->client_window = w;
         get_geometry_xywh(c);
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
printf("title of <%p> button%d press. (press=%d)\n", c, e.xbutton.button, press_button);
         if (press_button) break;
         press_button = e.xbutton.button;
         dx = c->x - e.xbutton.x_root;
         dy = c->y - e.xbutton.y_root;
         ow = c->w;
         oh = c->h;
printf("title of <%p> button%d press. (press=%d)\n", c, e.xbutton.button, press_button);
         break;
      case ButtonRelease:
         w = e.xbutton.window;
printf("ButtonRelease <%x>\n", (int)w);
         c = find_client_from_title(w);
         if (!c) break;
printf("title of <%p> button%d release. (press=%d)\n", c, e.xbutton.button, press_button);
         if (press_button != e.xbutton.button) break;
         press_button = 0;
printf("title of <%p> button%d release. (press=%d)\n", c, e.xbutton.button, press_button);
         break;
      case MotionNotify:
         w = e.xmotion.window;
         c = find_client_from_title(w);
         if (!c) break;
       switch (press_button) {
       case Button1:
         c->x = e.xmotion.x_root + dx;
         c->y = e.xmotion.y_root + dy;
         if (c->x < 0)  c->x = 0;
         if (c->y < 23) c->y = 23;
         if (c->x > screen_width  - 202) c->x = screen_width - 202;
         if (c->y > screen_height + 1)  c->y = screen_height + 1;
         XMoveWindow(dpy, c->client_window, c->x, c->y);
         XMoveWindow(dpy, c->title_window,  c->x, c->y - 23);
         break;
       case Button3:
         if (e.xmotion.x_root + dx - c->x + (int)ow < 32)
            c->w = 32;
         else
            c->w = e.xmotion.x_root + dx - c->x + ow;
         if (e.xmotion.y_root + dy - c->y + (int)oh < 32)
            c->h = 32;
         else
            c->h = e.xmotion.y_root + dy - c->y + oh;
         XResizeWindow(dpy, c->client_window, c->w, c->h);
       }
      }
   }
   XCloseDisplay(dpy);
   return 0;
}
