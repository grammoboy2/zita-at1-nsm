// ----------------------------------------------------------------------------
//
//  Copyright (C) 2010-2017 Fons Adriaensen <fons@linuxaudio.org>
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


#ifndef __GUICLASS_H
#define __GUICLASS_H


#include "button.h"
#include "rotary.h"


class Pbutt0 : public PushButton
{
public:

    Pbutt0 (X_window   *parent,
            X_callback *cbobj,
	          int         cbind,
            XImage     *image,
            int         xp,
            int         yp) :
    PushButton (parent, cbobj, cbind, image, xp, yp, 36, 21)
    {
    }

private:

    virtual int handle_press (void);
    virtual int handle_relse (void);
};


class Pbutt1 : public PushButton
{
public:

    Pbutt1 (X_window   *parent,
            X_callback *cbobj,
	          int         cbind,
            XImage     *image,
            int         xp,
            int         yp) :
    PushButton (parent, cbobj, cbind, image, xp, yp, 16, 16)
    {
    }

private:

    virtual int handle_press (void);
    virtual int handle_relse (void);
};


class Rlinctl : public RotaryCtl
{
public:

    Rlinctl (X_window    *parent,
             X_callback  *cbobj,
             int          cbind,
             RotaryGeom  *rgeom,
             int          xp,
             int          yp,
             int          cm,
             int          dd,
             double       vmin,
             double       vmax,
             double       vini);

    virtual void set_value (double v);
    virtual void get_string (char *p, int n);

private:

    virtual int handle_button (void);
    virtual int handle_motion (int dx, int dy);
    virtual int handle_mwheel (int dw);
    int set_count (int u);

    int         _cm;
    int         _dd;
    double      _vmin;
    double      _vmax;
    const char *_form;
};


class Rlogctl : public RotaryCtl
{
public:

    Rlogctl (X_window   *parent,
             X_callback *cbobj,
             int          cbind,
             RotaryGeom  *rgeom,
             int          xp,
             int          yp,
             int          cm,
             int          dd,
             double       vmin,
             double       vmax,
             double       vini);

    virtual void set_value (double v);
    virtual void get_string (char *p, int n);

private:

    virtual int handle_button (void);
    virtual int handle_motion (int dx, int dy);
    virtual int handle_mwheel (int dw);
    int set_count (int u);

    int         _cm;
    int         _dd;
    double      _vmin;
    double      _vmax;
    const char *_form;
};


#endif
