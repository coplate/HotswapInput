/**
 * Copyright (c) 2013 Nicolas Hillegeer <nicolas at hillegeer dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <X11/X.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <ctype.h>

#include <sys/select.h>
#include <sys/time.h>

#include <signal.h>
#include <time.h>
#include <getopt.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int gIdleTimeout = 1;
static int gVerbose     = 0;

static volatile sig_atomic_t working;

static void signalHandler(int signo) {
    working = 0;
}

enum e_action {doDefault, doBlack, doWhite, doSolid, doNone, doRoot};

int xrefresh(Display *dpy, char *ProgramName, char *displayname, char *geom, enum e_action action, char *solidcolor);

static int setupSignals() {
    struct sigaction act;

    memset(&act, 0, sizeof(act));

    /* Use the sa_sigaction field because the handles has two additional parameters */
    act.sa_handler = signalHandler;
    act.sa_flags   = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGTERM, &act, NULL) == -1) {
        perror("hhpc: could not register SIGTERM");
        return 0;
    }

    if (sigaction(SIGHUP, &act, NULL) == -1) {
        perror("hhpc: could not register SIGHUP");
        return 0;
    }

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("hhpc: could not register SIGINT");
        return 0;
    }

    if (sigaction(SIGQUIT, &act, NULL) == -1) {
        perror("hhpc: could not register SIGQUIT");
        return 0;
    }

    return 1;
}

/**
 * milliseconds over 1000 will be ignored
 */
static void delay(time_t sec, long msec) {
    struct timespec sleep;

    sleep.tv_sec  = sec;
    sleep.tv_nsec = (msec % 1000) * 1000 * 1000;

    if (nanosleep(&sleep, NULL) == -1) {
        signalHandler(0);
    }
}

/**
 * generates an empty cursor,
 * don't forget to destroy the cursor with XFreeCursor
 *
 * do we need to use XAllocColor or will it always just work
 * as I've observed?
 */
static Cursor nullCursor(Display *dpy, Drawable dw) {
    XColor color  = { 0 };
    const char data[] = { 0 };

    Pixmap pixmap = XCreateBitmapFromData(dpy, dw, data, 1, 1);
    Cursor cursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &color, &color, 0, 0);

    XFreePixmap(dpy, pixmap);

    return cursor;
}

/**
 * returns 0 for failure, 1 for success
 */
static int grabPointer(Display *dpy, Window win, Cursor cursor, unsigned int mask) {
    int rc;

    /* retry until we actually get the pointer (with a suitable delay)
     * or we get an error we can't recover from. */
    while (working) {
        rc = XGrabPointer(dpy, win, True, mask, GrabModeSync, GrabModeAsync, None, cursor, CurrentTime);

        switch (rc) {
            case GrabSuccess:
                if (gVerbose) fprintf(stderr, "hhpc: succesfully grabbed mouse pointer\n");
                return 1;

            case AlreadyGrabbed:
                if (gVerbose) fprintf(stderr, "hhpc: XGrabPointer: already grabbed mouse pointer, retrying with delay\n");
                delay(0, 500);
                break;

            case GrabFrozen:
                if (gVerbose) fprintf(stderr, "hhpc: XGrabPointer: grab was frozen, retrying after delay\n");
                delay(0, 500);
                break;

            case GrabNotViewable:
                fprintf(stderr, "hhpc: XGrabPointer: grab was not viewable, exiting\n");
                return 0;

            case GrabInvalidTime:
                fprintf(stderr, "hhpc: XGrabPointer: invalid time, exiting\n");
                return 0;

            default:
                fprintf(stderr, "hhpc: XGrabPointer: could not grab mouse pointer (%d), exiting\n", rc);
                return 0;
        }
    }

    return 0;
}

static void waitForMotion(Display *dpy, Window win, int timeout) {
    int ready = 0;
    int xfd   = ConnectionNumber(dpy);
    int xrefresh_argc = 3;
    char xrefresh_argv[32][32] ={0};

    const unsigned int mask = PointerMotionMask | ButtonPressMask;

    fd_set fds;

    XEvent event;
sprintf(xrefresh_argv[0], "pointer.lix");
    sprintf(xrefresh_argv[1], "-g");
    sprintf(xrefresh_argv[2], "50x50+50+50");

    
    working = 1;

    if (!setupSignals()) {
        fprintf(stderr, "hhpc: could not register signals, program will not exit cleanly\n");
    }

    while (working && grabPointer(dpy, win, None, mask)) {
        /* we grab in sync mode, which stops pointer events from processing,
         * so we explicitly have to re-allow it with XAllowEvents. The old
         * method was to just grab in async mode so we wouldn't need this,
         * but that disables replaying the pointer events */
        XAllowEvents(dpy, SyncPointer, CurrentTime);

        /* syncing is necessary, otherwise the X11 FD will never receive an
         * event (and thus will never be ready, strangely enough) */
        XSync(dpy, False);

        /* add the X11 fd to the fdset so we can poll/select on it */
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);

        /* we poll on the X11 fd to see if an event has come in, select()
         * is interruptible by signals, which allows ctrl+c to work. If we
         * were to just use XNextEvent() (which blocks), ctrl+c would not
         * work. */
        ready = select(xfd + 1, &fds, NULL, NULL, NULL);

        if (ready > 0) {
            if (gVerbose) fprintf(stderr, "hhpc: event received, ungrabbing and sleeping\n");

            /* event received, replay event, release mouse, drain, sleep, regrab */
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
            XUngrabPointer(dpy, CurrentTime);

//            /* drain events */
//            while (XPending(dpy)) {
//                XMaskEvent(dpy, mask, &event);
//
//                if (gVerbose) fprintf(stderr, "hhpc: draining event Type %d\n", event.type);
//
//            }

	if( XPending(dpy) ){
                XMaskEvent(dpy, mask, &event);

                if (gVerbose) fprintf(stderr, "hhpc: draining event Type %d\n", event.type);

            }
xrefresh(dpy, xrefresh_argv[0], NULL, xrefresh_argv[2], doDefault, NULL);
            delay(timeout, 0);
		
	
        }
        else if (ready == 0) {
            if (gVerbose) fprintf(stderr, "hhpc: timeout\n");
        }
        else {
            if (working) perror("hhpc: error while select()'ing");
        }
    }

    XUngrabPointer(dpy, CurrentTime);
}

static int parseOptions(int argc, char *argv[]) {
    int option = 0;

    while ((option = getopt(argc, argv, "i:v")) != -1) {
        switch (option) {
            case 'i': gIdleTimeout = atoi(optarg); break;
            case 'v': gVerbose = 1; break;
            default: return 0;
        }
    }

    return 1;
}

static void usage() {
    printf("hhpc [-i] seconds [-v]\n");
}

int main(int argc, char *argv[]) {
    if (!parseOptions(argc, argv)) {
        usage();

        return 1;
    }

    char *displayName = getenv("DISPLAY");

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        if (!displayName || strlen(displayName) == 0) {
            fprintf(stderr, "hhpc: could not open display, DISPLAY environment variable not set, are you sure the X server is started?\n");
            return 2;
        }
        else {
            fprintf(stderr, "hhpc: could not open display %s, check if your X server is running and/or the DISPLAY environment value is correct\n", displayName);
            return 1;
        }
    }

    int scr        = DefaultScreen(dpy);
    Window rootwin = RootWindow(dpy, scr);

    if (gVerbose) fprintf(stderr, "hhpc: got root window, screen = %d, display = %p, rootwin = %d\n", scr, (void *) dpy, (int) rootwin);

    waitForMotion(dpy, rootwin, gIdleTimeout);

    XCloseDisplay(dpy);

    return 0;
}

char *ProgramName;

static void 
Syntax(void)
{
    fprintf (stderr, "usage:  %s [-options] [geometry] [display]\n\n", 
    	     ProgramName);
    fprintf (stderr, "where the available options are:\n");
    fprintf (stderr, "    -display host:dpy       or -d\n");
    fprintf (stderr, "    -geometry WxH+X+Y       or -g spec\n");
    fprintf (stderr, "    -black                  use BlackPixel\n");
    fprintf (stderr, "    -white                  use WhitePixel\n");
    fprintf (stderr, "    -solid colorname        use the color indicated\n");
    fprintf (stderr, "    -root                   use the root background\n");
    fprintf (stderr, "    -none                   no background in window\n");
    fprintf (stderr, "\nThe default is:  %s -none\n\n", ProgramName);
    exit (1);
}

/*
 * The following parses options that should be yes or no; it returns -1, 0, 1
 * for error, no, yes.
 */

static int 
parse_boolean_option(char *option)
{
    static struct _booltable {
        char *name;
        int value;
    } booltable[] = {
        { "off", 0 }, { "n", 0 }, { "no", 0 }, { "false", 0 },
        { "on", 1 }, { "y", 1 }, { "yes", 1 }, { "true", 1 },
        { NULL, -1 }};
    register struct _booltable *t;
    register char *cp;

    for (cp = option; *cp; cp++) {
        if (isascii (*cp) && isupper (*cp)) *cp = tolower (*cp);
    }

    for (t = booltable; t->name; t++) {
        if (strcmp (option, t->name) == 0) return (t->value);
    }
    return (-1);
}


/*
 * The following is a hack until XrmParseCommand is ready.  It determines
 * whether or not the given string is an abbreviation of the arg.
 */

static Bool 
isabbreviation(char *arg, char *s, int minslen)
{
    int arglen;
    int slen;

    /* exact match */
    if (strcmp (arg, s) == 0) return (True);

    arglen = strlen (arg);
    slen = strlen (s);

    /* too long or too short */
    if (slen >= arglen || slen < minslen) return (False);

    /* abbreviation */
    if (strncmp (arg, s, slen) == 0) return (True);

    /* bad */
    return (False);
}

struct s_pair {
	char *resource_name;
	enum e_action action;
} pair_table[] = {
	{ "Black", doBlack },
	{ "White", doWhite },
	{ "None", doNone },
	{ "Root", doRoot },
	{ NULL, doDefault }};

int
xrefresh(Display *dpy, char *ProgramName, char *displayname, char *geom, enum e_action action, char *solidcolor)
{
    Visual visual;
    XSetWindowAttributes xswa;
    int i;
    Colormap cmap;
    unsigned long mask;
    int screen;
    int x, y, width, height;
    int geom_result;
    int display_width, display_height;
    XColor cdef;
int win_x, win_y, root_x, root_y = 0;
    unsigned int query_mask = 0;
    Window child_win, root_win;


    if (action == doDefault) {
	char *def;

	if ((def = XGetDefault (dpy, ProgramName, "Solid")) != NULL) {
	    solidcolor = strdup (def);
	    if (solidcolor == NULL) {
		fprintf (stderr,
			 "%s:  unable to allocate memory for string.\n",
			 ProgramName);
		exit (1);
	    }
	    action = doSolid;
	} else {
	    struct s_pair *pp;

	    for (pp = pair_table; pp->resource_name != NULL; pp++) {
		def = XGetDefault (dpy, ProgramName, pp->resource_name);
		if (def && parse_boolean_option (def) == 1) {
		    action = pp->action;
		}
	    }
	}
    }

    if (geom == NULL) geom = XGetDefault (dpy, ProgramName, "Geometry");

    screen = DefaultScreen (dpy);
    display_width = DisplayWidth (dpy, screen);
    display_height = DisplayHeight (dpy, screen);
    x = y = 0; 
    width = display_width;
    height = display_height;

    if (DisplayCells (dpy, screen) <= 2 && action == doSolid) {
	if (strcmp (solidcolor, "black") == 0)
	    action = doBlack;
	else if (strcmp (solidcolor, "white") == 0) 
	    action = doWhite;
	else {
	    fprintf (stderr, 
	    	     "%s:  can't use colors on a monochrome display.\n",
		     ProgramName);
	    action = doNone;
	}
    }

    if (geom) 
        geom_result = XParseGeometry (geom, &x, &y,
				      (unsigned int *)&width,
				      (unsigned int *)&height);
    else
	geom_result = NoValue;

    /*
     * For parsing geometry, we want to have the following
     *     
     *     =                (0,0) for (display_width,display_height)
     *     =WxH+X+Y         (X,Y) for (W,H)
     *     =WxH-X-Y         (display_width-W-X,display_height-H-Y) for (W,H)
     *     =+X+Y            (X,Y) for (display_width-X,display_height-Y)
     *     =WxH             (0,0) for (W,H)
     *     =-X-Y            (0,0) for (display_width-X,display_height-Y)
     *
     * If we let any missing values be taken from (0,0) for 
     * (display_width,display_height) we just have to deal with the
     * negative offsets.
     */

    if (geom_result & XNegative) {
	if (geom_result & WidthValue) {
	    x = display_width - width + x;
	} else {
	    width = display_width + x;
	    x = 0;
	}
    } 
    if (geom_result & YNegative) {
	if (geom_result & HeightValue) {
	    y = display_height - height + y;
	} else {
	    height = display_height + y;
	    y = 0;
	}
    }

    mask = 0;
    switch (action) {
	case doBlack:
	    xswa.background_pixel = BlackPixel (dpy, screen);
	    mask |= CWBackPixel;
	    break;
	case doWhite:
	    xswa.background_pixel = WhitePixel (dpy, screen);
	    mask |= CWBackPixel;
	    break;
	case doSolid:
	    cmap = DefaultColormap (dpy, screen);
	    if (XParseColor (dpy, cmap, solidcolor, &cdef) &&
		XAllocColor (dpy, cmap, &cdef)) {
		xswa.background_pixel = cdef.pixel;
		mask |= CWBackPixel;
	    } else {
		fprintf (stderr,"%s:  unable to allocate color '%s'.\n",
			 ProgramName, solidcolor);
		action = doNone;
	    }
	    break;
	case doDefault:
	case doNone:
	    xswa.background_pixmap = None;
	    mask |= CWBackPixmap;
	    break;
	case doRoot:
	    xswa.background_pixmap = ParentRelative;
	    mask |= CWBackPixmap;
	    break;
    }
    xswa.override_redirect = True;
    xswa.backing_store = NotUseful;
    xswa.save_under = False;
    mask |= (CWOverrideRedirect | CWBackingStore | CWSaveUnder);
    visual.visualid = CopyFromParent;

XQueryPointer(dpy, XRootWindow(dpy, screen),
        &child_win, &root_win,
        &root_x, &root_y, &win_x, &win_y, &query_mask);
fprintf (stderr,"root_x = %d, root_y=%d, win_x=%d, win_y=%d\n",
			 root_x, root_y, win_x, win_y);
    Window win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, display_width,display_height,
	    0, DefaultDepth(dpy, screen), InputOutput, &visual, mask, &xswa);
//Window win = XCreateWindow(dpy, DefaultRootWindow(dpy), x, y, width, height,
//	    0, DefaultDepth(dpy, screen), InputOutput, &visual, mask, &xswa);

    /*
     * at some point, we really ought to go walk the tree and turn off 
     * backing store;  or do a ClearArea generating exposures on all windows
     */
    XMapWindow (dpy, win);
    XDestroyWindow( dpy, win);


 

}