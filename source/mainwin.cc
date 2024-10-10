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

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "styles.h"
#include "global.h"
#include "mainwin.h"
#include "nsm.h"


extern NSM_Client *nsm;

Mainwin::Mainwin (X_rootwin *parent, X_resman *xres, int xp, int yp, Jclient *jclient) :
    A_thread ("Main"),
    X_window (parent, xp, yp, XSIZE, YSIZE, XftColors [C_MAIN_BG]->pixel),
    _stop (false),
    _ambis (false),
    _xres (xres),
    _jclient (jclient),
    _dirty (false),
    _managed (false)
{
    X_hints     H;
    char        s [256];
    int         i, j, x, y;

    _atom = XInternAtom (dpy (), "WM_DELETE_WINDOW", True);
    XSetWMProtocols (dpy (), win (), &_atom, 1);
    _atom = XInternAtom (dpy (), "WM_PROTOCOLS", True);

    sprintf (s, "%s  (zita-at1-%s)", jclient->jname (), VERSION);
    x_set_title (s);
    H.position (xp, yp);
    H.minsize (XSIZE, YSIZE);
    H.maxsize (XSIZE, YSIZE);
    H.rname (xres->rname ());
    H.rclas (xres->rclas ());
    x_apply (&H); 

    x = 22;
    _bllat = new Pbutt0 (this, this, B_LLAT, b_llat_img, x, 5);
    _bllat->x_map ();
    _bmidi = new Pbutt0 (this, this, B_MIDI, b_midi_img, x, 29);
    _bmidi->x_map ();

    bstyle1.size.x = 50;
    bstyle1.size.y = 20;
    _bchan = new X_tbutton (this, this, &bstyle1, x - 7, 49, "Omni", 0, B_CHAN);
    _bchan->x_map ();
    _midich = 0;
    _jclient->set_midichan (-1);

    _tmeter = new Tmeter (this, 80, 53);
    _tmeter->x_map ();
    y = 23;
    x = 100;
    for (i = j = 0; i < 12; i++, j++)
    {
        _bnote [i] = new Pbutt1 (this, this, i, b_note_img, x, y);
        _bnote [i]->set_state (1);
        _bnote [i]->x_map ();
        if (j == 4) 
        {
            x += 20;
            j++;
        }
        else
        {
            x += 10;
            if (j & 1) y += 18;
            else y -= 18;
        } 
    }


    RotaryCtl::init (disp ());
    x = 270;
    _rotary [R_TUNE] = new Rlinctl (this, this, R_TUNE, &r_tune_geom, x, 0, 400,  5, 400.0, 480.0, 440.0);
    _rotary [R_BIAS] = new Rlinctl (this, this, R_BIAS, &r_bias_geom, x, 0, 270,  5,   0.0,   1.0,   0.5);
    _rotary [R_FILT] = new Rlogctl (this, this, R_FILT, &r_filt_geom, x, 0, 200,  5,   0.50,  0.02,  0.1);
    _rotary [R_CORR] = new Rlinctl (this, this, R_CORR, &r_corr_geom, x, 0, 270,  5,   0.0,   1.0,   1.0);
    _rotary [R_OFFS] = new Rlinctl (this, this, R_OFFS, &r_offs_geom, x, 0, 400, 10,  -2.0,   2.0,   0.0);
    for (i = 0; i < NROTARY; i++) _rotary [i]->x_map ();

    _textln = new X_textip (this, 0, &tstyle1, 0, 0, 50, 15, 15);
    _textln->set_align (0);
    _ttimer = 0;

    _notes = 0xFFF;
    _jclient->set_notemask (_notes);
    _jclient->retuner ()->set_refpitch (_rotary [R_TUNE]->value ());
    _jclient->retuner ()->set_notebias (_rotary [R_BIAS]->value ());
    _jclient->retuner ()->set_corrfilt (_rotary [R_FILT]->value ());
    _jclient->retuner ()->set_corrgain (_rotary [R_CORR]->value ());
    _jclient->retuner ()->set_corroffs (_rotary [R_OFFS]->value ());

    x_add_events (ExposureMask);
    x_map ();
    set_time (0);
    inc_time (500000);
}

 
Mainwin::~Mainwin (void)
{
    RotaryCtl::fini ();
}

 
int Mainwin::process (void)
{
    int e;

    if (_stop) handle_stop ();

    e = get_event_timed ();
    switch (e)
    {
    case EV_TIME:
        handle_time ();
        break;
    }
    return e;
}


void Mainwin::handle_event (XEvent *E)
{
    switch (E->type)
    {
    case Expose:
        expose ((XExposeEvent *) E);
        break;
 
    case ClientMessage:
        clmesg ((XClientMessageEvent *) E);
        break;
    }
}


void Mainwin::expose (XExposeEvent *E)
{
    if (E->count) return;
    redraw ();
}


void Mainwin::clmesg (XClientMessageEvent *E)
{
    if (E->message_type == _atom) _stop = true;
}


void Mainwin::handle_time (void)
{
    int   i, k, s;
    float v;

    v = _jclient->retuner ()->get_error ();
    _tmeter->update (v, v);
    k = _jclient->retuner ()->get_noteset ();
    for (i = 0; i < 12; i++)
    {
        s = _bnote [i]->state ();
        if (k & 1) s |= 2;
        else s &= ~2;
        _bnote [i]->set_state (s);
        k >>= 1;
    }
    k = _jclient->get_midiset();
    if (k) _bmidi->set_state (_bmidi->state () | 1);
    else   _bmidi->set_state (_bmidi->state () & ~1);
    if (_ttimer)
    {
        if (--_ttimer == 0) _textln->x_unmap ();
    }
    inc_time (50000);
    XFlush (dpy ());
}


void Mainwin::handle_stop (void)
{
    put_event (EV_EXIT, 1);
}


void Mainwin::handle_callb (int type, X_window *W, XEvent *E)
{
    PushButton *B;
    RotaryCtl  *R;
    int         k, s;
    float       v;

    switch (type)
    {
    case X_callback::BUTTON | X_button::PRESS:
    {
        X_button     *Z = (X_button *) W;
	XButtonEvent *X = (XButtonEvent *) E;
	switch (Z->cbid ())
	{
	case B_CHAN:
	    switch (X->button)
	    {
	    case 1:
	    case 4:
		setchan (1);
	        break;
	    case 3:
	    case 5:
		setchan (-1);
	        break;
	    }
            break;
	}
	break;
    }
    case PushButton::PRESS:
	B = (PushButton *) W;
	k = B->cbind ();
	if (k < B_LLAT)
	{
	    k = 1 << k;
	    if (B->state () & 1) _notes |=  k;
	    else                 _notes &= ~k;
	    _jclient->set_notemask (_notes);
	}
	else if (k == B_LLAT)
	{
	    s = (B->state () & 1) ^ 1;
	    B->set_state (s);
	    _jclient->set_lowlat (s);
	}
	else if (k == B_MIDI)
	{
	    _jclient->clr_midimask ();
	}
	break;

    case RotaryCtl::PRESS:
	R = (RotaryCtl *) W;
	k = R->cbind ();
	switch (k)
	{
        case R_TUNE:
        case R_OFFS:
	    showval (k);
	    break;
	}
	break;

    case RotaryCtl::DELTA:
	R = (RotaryCtl *) W;
	k = R->cbind ();
	switch (k)
	{
        case R_TUNE:
            v = _rotary [R_TUNE]->value ();
            _jclient->retuner ()->set_refpitch (v);
	    showval (k);
	    break;
	case R_BIAS:   
            _jclient->retuner ()->set_notebias (_rotary [R_BIAS]->value ());
	    break;
	case R_FILT:   
            _jclient->retuner ()->set_corrfilt (_rotary [R_FILT]->value ());
	    break;
	case R_CORR:   
            _jclient->retuner ()->set_corrgain (_rotary [R_CORR]->value ());
	    break;
	case R_OFFS:   
            _jclient->retuner ()->set_corroffs (_rotary [R_OFFS]->value ());
	    showval (k);
	    break;
	}
	break;
    }

    if (!_dirty)
    {
        if (nsm) nsm->is_dirty ();
        _dirty = true;
    }
}


void Mainwin::setchan (int d)
{
    char s [16];

    _midich += d;
    if (_midich <  0) _midich =  0;
    if (_midich > 16) _midich = 16;
    if (_midich)
    {
        sprintf (s, "Ch %d\n", _midich);
	_bchan->set_text (s, 0);
    }
    else _bchan->set_text ("Omni", 0);
    _jclient->set_midichan (_midich - 1);
}   


void Mainwin::showval (int k)
{
    char s [16];

    switch (k)
    {
    case R_TUNE:
        sprintf (s, "%5.1lf", _rotary [R_TUNE]->value ());
	_textln->x_move (285, 58);
	break;
    case R_OFFS:
        sprintf (s, "%5.2lf", _rotary [R_OFFS]->value ());
	_textln->x_move (525, 58);
	break;
    }
    _textln->set_text (s);
    _textln->x_map ();
    _ttimer = 40;
}


void Mainwin::redraw (void)
{
    int x;

    x = 80;
    XPutImage (dpy (), win (), dgc (), notesect_img, 0, 0, x, 0, 190, 75);
    x += 190;
    XPutImage (dpy (), win (), dgc (), ctrlsect_img, 0, 0, x, 0, 315, 75);
    x = XSIZE - 35;
    XPutImage (dpy (), win (), dgc (), redzita_img, 0, 0, x, 0, 35, 75);
    if (_managed)
    {
        x += 10;
        XPutImage (dpy (), win (), dgc (), sm_img,      0, 0,   x, 60, 19, 10);
    }
}


void Mainwin::load_state ()
{
    ifstream statefile(_statefile.c_str());

    if (statefile.is_open())
    {
        string parameter;
        float  tune = 0.0f;
        float  bias = 0.0f;
        float  filt = 0.0f;
        float  corr = 0.0f;
        float  offs = 0.0f;
        int   notes = 0xFFF;
        int      xp = 100;
        int      yp = 100;
        int i;

        while (!statefile.eof())
        {
            statefile >> parameter;
            if (parameter == "/autotune/tune")
            {
                statefile >> dec >> tune;
            }
            else if (parameter == "/autotune/bias")
            {
                statefile >> dec >> bias;
            }
            else if (parameter == "/autotune/filt")
            {
                statefile >> dec >> filt;
            }
            else if (parameter == "/autotune/corr")
            {
                statefile >> dec >> corr;
            }
            else if (parameter == "/autotune/offs")
            {
                statefile >> dec >> offs;
            }
            else if (parameter == "/autotune/notes")
            {
                statefile >> hex >> notes;
            }
            else if (parameter == "/window/x")
            {
                statefile >> dec >> xp;
            }
            else if (parameter == "/window/y")
            {
                statefile >> dec >> yp;
            }
        }

        statefile.close();

        _rotary [R_TUNE]->set_value (tune);
        _jclient->retuner ()->set_refpitch (_rotary [R_TUNE]->value ());
        _rotary [R_BIAS]->set_value (bias);
        _jclient->retuner ()->set_notebias (_rotary [R_BIAS]->value ());
        _rotary [R_FILT]->set_value (filt);
        _jclient->retuner ()->set_corrfilt (_rotary [R_FILT]->value ());
        _rotary [R_CORR]->set_value (corr);
        _jclient->retuner ()->set_corrgain (_rotary [R_CORR]->value ());
        _rotary [R_OFFS]->set_value (offs);
        _jclient->retuner ()->set_corroffs (_rotary [R_OFFS]->value ());

        _notes = notes;
        _jclient->set_notemask (_notes);

        for (i = 0; i < 12; i++)
        {
            _bnote [i]->set_state (1 * (0x1 & notes));
            notes >>= 1;
        }
        x_move (xp, yp);
        redraw ();
    }
}


void Mainwin::save_state (void)
{
    ofstream statefile(_statefile.c_str());

    if (statefile.is_open())
    {
        float tune = _rotary [R_TUNE]->value ();
        float bias = _rotary [R_BIAS]->value ();
        float filt = _rotary [R_FILT]->value ();
        float corr = _rotary [R_CORR]->value ();
        float offs = _rotary [R_OFFS]->value ();
        int  notes = _notes;

        statefile << "/autotune/tune\t"  << dec << tune  << endl;
        statefile << "/autotune/bias\t"  << dec << bias  << endl;
        statefile << "/autotune/filt\t"  << dec << filt  << endl;
        statefile << "/autotune/corr\t"  << dec << corr  << endl;
        statefile << "/autotune/offs\t"  << dec << offs  << endl;
        statefile << "/autotune/notes\t" << hex << notes << endl;

        Window w_return;
        int x_s, y_s, x, y;
        unsigned int w, h;
        unsigned int b_w;
        unsigned int d;

        XGetGeometry (dpy (), win (), &w_return, &x, &y, &w, &h, &b_w, &d);
        XTranslateCoordinates (dpy (), win (), pwin ()->win (),
                               -x, -y, &x_s, &y_s, &w_return);

        statefile << "/window/x\t" << dec << x_s << endl;
        statefile << "/window/y\t" << dec << y_s << endl;

        statefile.close();

        _dirty = false;
        if (nsm) nsm->is_clean();
    }
}


void Mainwin::set_managed (bool m)
{
    _managed = m;
    redraw ();
}


