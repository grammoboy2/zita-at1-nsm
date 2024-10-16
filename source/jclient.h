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


#ifndef __JCLIENT_H
#define __JCLIENT_H


#include <jack/jack.h>
#include <clthreads.h>
#include "retuner.h"


class Jclient : public A_thread
{
public:

    Jclient (const char *jname, const char *jserv);
    ~Jclient (void);

    const char *jname (void) { return _jname; }
    unsigned int fsize (void) const { return _fsize; } 
    unsigned int fsamp (void) const { return _fsamp; } 
    Retuner *retuner (void) { return _retuner; }
    void set_notemask (int m) { _notemask = m; } 
    void set_midichan (int c) { _midichan = c; }
    void set_lowlat (bool s) { _retuner->set_lowlat (s); }
    void clr_midimask (void);
    int  get_noteset (void) { return _retuner->get_noteset (); }
    int  get_midiset (void) { return _midimask; }

private:

    virtual void thr_main (void) {}

    void init_jack (const char *jname, const char *jserv);
    void close_jack (void);
    void jack_shutdown (void);
    int  jack_process (int nframes);
    void midi_process (int nframes);

    jack_client_t  *_jack_client;
    jack_port_t    *_ainp_port;
    jack_port_t    *_aout_port;
    jack_port_t    *_midi_port;
    bool            _active;
    const char     *_jname;
    unsigned int    _fsamp;
    unsigned int    _fsize;
    Retuner        *_retuner;
    int             _notes [12];
    int             _notemask;
    int             _midimask;
    int             _midichan;

    static void jack_static_shutdown (void *arg);
    static int  jack_static_process (jack_nframes_t nframes, void *arg);
};


#endif
