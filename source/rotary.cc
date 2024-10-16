// ----------------------------------------------------------------------------
//
//  Copyright (C) 2008-2015 Fons Adriaensen <fons@linuxaudio.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------


#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <math.h>
#include "rotary.h"


cairo_t         *RotaryCtl::_cairotype = 0;
cairo_surface_t *RotaryCtl::_cairosurf = 0;


int RotaryCtl::_wb_up = 4;
int RotaryCtl::_wb_dn = 5;
int RotaryCtl::_keymod = 0;
int RotaryCtl::_button = 0;
int RotaryCtl::_rcount = 0;
int RotaryCtl::_rx = 0;
int RotaryCtl::_ry = 0;


RotaryCtl::RotaryCtl (X_window     *parent,
                      X_callback   *cbobj,
		                  int           cbind,
                      RotaryGeom   *rgeom,
		                  int           xp,
                      int           yp) :

    X_window (parent,
              rgeom->_x0 + xp, rgeom->_y0 + yp,
              rgeom->_dx, rgeom->_dy,
              rgeom->_backg->pixel),
    _cbobj (cbobj),
    _cbind (cbind),
    _rgeom (rgeom),
    _state (0),
    _count (0),
    _value (0),
    _angle (0)
{
    x_add_events (  ExposureMask
                  | Button1MotionMask | ButtonPressMask | ButtonReleaseMask);
} 


RotaryCtl::~RotaryCtl (void)
{
}


void RotaryCtl::init (X_display *disp)
{
    _cairosurf = cairo_xlib_surface_create (disp->dpy (), 0, disp->dvi (), 50, 50);
    _cairotype = cairo_create (_cairosurf);
}


void RotaryCtl::fini (void)
{
    cairo_destroy (_cairotype);
    cairo_surface_destroy (_cairosurf);
}


void RotaryCtl::handle_event (XEvent *E)
{
    switch (E->type)
    {
    case Expose:
        render ();
        break;
 
    case ButtonPress:
        bpress ((XButtonEvent *) E);
        break;

    case ButtonRelease:
        brelse ((XButtonEvent *) E);
        break;

    case MotionNotify:
        motion ((XMotionEvent *) E);
        break;

    default: 
        fprintf (stderr, "RotaryCtl: event %d\n", E->type );
    }
}


void RotaryCtl::bpress (XButtonEvent *E)
{
    int    r = 0;
    double d;

    d = hypot (E->x - _rgeom->_xref, E->y - _rgeom->_yref);
    if (d > _rgeom->_rad + 3) return;
    _keymod  = E->state;
    if (E->button < 4)
    {
        _rx = E->x;
        _ry = E->y;
        _button = E->button;
        r = handle_button ();
        _rcount = _count;
    }
    else if (_button) return;
    else if ((int)E->button == _wb_up)
    {
        r = handle_mwheel (1);
    }
    else if ((int)E->button == _wb_dn)
    {
        r = handle_mwheel (-1);
    } 
    if (r)
    {
        callback (r);
        render ();
    }
}


void RotaryCtl::brelse (XButtonEvent *E)
{
    if (_button == (int)E->button)
    {
        _button = 0;
        callback (RELSE);
    }
}


void RotaryCtl::motion (XMotionEvent *E)
{
    int dx, dy, r;

    if (_button)
    {
        _keymod = E->state;
        dx = E->x - _rx;
        dy = E->y - _ry;
        r = handle_motion (dx, dy);
        if (r)
        {
            callback (r);
            render ();
        }
    }
}


void RotaryCtl::set_state (int s)
{
    if (_state != s)
    {
        _state = s;
        render ();
    }
}


void RotaryCtl::render (void)
{
    double  a, c, r, x, y;

    // A very weird bugfix. Without this, the cairo line
    // draw below fails if the line is exactly horizontal
    // or vertical. No amount of XFlush(), cairo_surface_flush()
    // or cairo_surface_mark_dirty() seems to help, but this 
    // XDrawline does.
    XDrawLine (dpy (), win (), dgc (), 0, 0, 0, 0);

    XPutImage (dpy (), win (), dgc (), _rgeom->_image [_state],
               _rgeom->_x0, _rgeom->_y0, 0, 0, _rgeom->_dx, _rgeom->_dy);
    XFlush (dpy ());
    cairo_xlib_surface_set_drawable (_cairosurf, win(),_rgeom->_dx, _rgeom->_dy);
    c = _rgeom->_lncol [_state] ? 1.0 : 0.0;
    a = _angle * M_PI / 180;
    r = _rgeom->_rad;
    x = _rgeom->_xref;
    y = _rgeom->_yref;
    cairo_new_path (_cairotype);
    cairo_move_to (_cairotype, x, y);
    x += r * sin (a);
    y -= r * cos (a);
    cairo_line_to (_cairotype, x, y);
    cairo_set_source_rgb (_cairotype, c, c, c);
    cairo_set_line_width (_cairotype, c ? 2.2 : 1.8);
    cairo_stroke (_cairotype);
    cairo_surface_flush (_cairosurf);
}


