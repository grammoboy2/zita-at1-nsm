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


#ifndef __BUTTON_H
#define	__BUTTON_H


#include <clxclient.h>


class PushButton : public X_window
{
public:

    PushButton (X_window    *parent,
                X_callback  *cbobj,
                int          cbind,
		XImage      *image,
	        int xp,
                int yp,
		int xs,
		int ys);

    virtual ~PushButton (void);

    enum { NOP = 100, PRESS, RELSE, DRAG };

    int    cbind (void) { return _cbind; }
    int    state (void) { return _state; }

    virtual void set_state (int s);

    static int keymod (void) { return _keymod; }
    static int button (void) { return _button; }

    static void init (X_display *disp);
    static void fini (void);

protected:

    X_callback  *_cbobj;
    int          _cbind;
    XImage      *_image;
    int          _state;
    int          _xs;
    int          _ys;

    void render (void);
    void callback (int k) { _cbobj->handle_callb (k, this, 0); }

private:

    void handle_event (XEvent *E);
    void bpress (XButtonEvent *E);
    void brelse (XButtonEvent *E);

    virtual int handle_press (void) = 0;
    virtual int handle_relse (void) = 0;

    static int _keymod;
    static int _button;
};


#endif
