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


#ifndef __MAINWIN_H
#define __MAINWIN_H


#include <string>
#include <clxclient.h>
#include "guiclass.h"
#include "jclient.h"
#include "tmeter.h"
#include "global.h"


using namespace std;

class Mainwin : public A_thread, public X_window, public X_callback
{
public:

    enum { XSIZE = 640, YSIZE = 75 };

    Mainwin (X_rootwin *parent, X_resman *xres, int xp, int yp, Jclient *jclient);
    ~Mainwin (void);
    Mainwin (const Mainwin&);
    Mainwin& operator=(const Mainwin&);

    void stop (void) { _stop = true; }
    int process (void); 
    void load_state (void);
    void save_state (void);
    void set_managed (bool);
    void set_statefile (const string s) { _statefile = s; }

private:

    enum { B_LLAT = 12, B_MIDI, B_CHAN };
    enum { R_TUNE, R_FILT, R_BIAS, R_CORR, R_OFFS, NROTARY };
 
    virtual void thr_main (void) {}

    void handle_time (void);
    void handle_stop (void);
    void handle_event (XEvent *);
    void handle_callb (int type, X_window *W, XEvent *E);
    void showval (int k);
    void expose (XExposeEvent *E);
    void clmesg (XClientMessageEvent *E);
    void redraw (void);
    void setchan (int d);

    Atom            _atom;
    bool            _stop;
    bool            _ambis;
    X_resman       *_xres;
    Jclient        *_jclient;
    int             _notes;
    PushButton     *_bllat;
    PushButton     *_bmidi;
    PushButton     *_bnote [12];
    RotaryCtl      *_rotary [NROTARY];
    Tmeter         *_tmeter;
    X_textip       *_textln;
    X_tbutton      *_bchan;
    int             _midich;
    int             _ttimer;
    string          _statefile;
    bool            _dirty;
    bool            _managed;

};


#endif
