// ----------------------------------------------------------------------------
//
//  Copyright (C) 2010-2024 Fons Adriaensen <fons@linuxaudio.org>
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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "retuner.h"


Retuner::Retuner (int fsamp) :
    _fsamp (fsamp),
    _refpitch (440.0f),
    _notebias (0.0f),
    _corrfilt (1.0f),
    _corrgain (1.0f),
    _corroffs (0.0f),
    _notemask (0xFFF)
{
    int   i, h;
    float t, x, y;

    if (_fsamp < 64000)
    {
        // At 44.1 and 48 kHz resample to double rate.
        _upsamp = true;
        _ipsize = 4096;
        _fftlen = 2048;
        _frsize = 128;
        _resampler.setup (1, 2, 1, 32); // 32 is medium quality.
        // Prefeed some input samples to remove delay.
        _resampler.inp_count = _resampler.filtlen () - 1;
        _resampler.inp_data = 0;
        _resampler.out_count = 0;
        _resampler.out_data = 0;
        _resampler.process ();
    }
    else if (_fsamp < 128000)
    {
        // 88.2 or 96 kHz.
        _upsamp = false;
        _ipsize = 4096;
        _fftlen = 4096;
        _frsize = 256;
    }
    else
    {
        // 192 kHz, double time domain buffers sizes.
        _upsamp = false;
        _ipsize = 8192;
        _fftlen = 8192;
        _frsize = 512;
    }

    // Accepted correlation peak range, corresponding to 75..1200 Hz.
    _ifmin = _fsamp / 1200;
    _ifmax = _fsamp / 75;

    // Various buffers
    _ipbuff = new float[_ipsize + 3];  // Resampled or filtered input
    _xffunc = new float[_frsize];      // Crossfade function
    _Twind = (float *) fftwf_malloc (_fftlen * sizeof (float)); // Window function 
    _Wcorr = (float *) fftwf_malloc (_fftlen * sizeof (float)); // Autocorrelation of window 
    _Tdata = (float *) fftwf_malloc (_fftlen * sizeof (float)); // Time domain data for FFT
    _Fdata = (fftwf_complex *) fftwf_malloc ((_fftlen / 2 + 1) * sizeof (fftwf_complex));
    
    // FFTW3 plans
    _fwdplan = fftwf_plan_dft_r2c_1d (_fftlen, _Tdata, _Fdata, FFTW_ESTIMATE);
    _invplan = fftwf_plan_dft_c2r_1d (_fftlen, _Fdata, _Tdata, FFTW_ESTIMATE);

    // Clear input buffer.
    memset (_ipbuff, 0, (_ipsize + 1) * sizeof (float));

    // Create crossfade function, half of raised cosine.
    for (i = 0; i < _frsize; i++)
    {
        _xffunc [i] = 0.5 * (1 - cosf (M_PI * i / _frsize));
    }

    // Create window, raised cosine.
    // Normalise forward FFT gain.
    t = 2.0f / _fftlen;
    for (i = 0; i < _fftlen; i++)
    {
        _Twind [i] = t * (1 - cosf (2 * M_PI * i / _fftlen)) ;
    }

    // Compute window autocorrelation and normalise it.
    fftwf_execute_dft_r2c (_fwdplan, _Twind, _Fdata);    
    h = _fftlen / 2;
    for (i = 0; i < h; i++)
    {
        x = _Fdata [i][0];
        y = _Fdata [i][1];
        _Fdata [i][0] = x * x + y * y;
        _Fdata [i][1] = 0;
    }
    _Fdata [h][0] = 0;
    _Fdata [h][1] = 0;
    fftwf_execute_dft_c2r (_invplan, _Fdata, _Wcorr);    
    t = _Wcorr [0];
    for (i = 0; i < _fftlen; i++) _Wcorr [i] /= t;

    // Initialise all counters and other state.
    _notebits = 0;
    _lastnote = -1;
    _count = 0;
    _cycle = _frsize;
    _error = 0.0f;
    _ratio = 1.0f;
    _xfade = false;
    _latency = _ipsize / 2;
    _ipindex = _latency;
    _frindex = 0;
    _frcount = 0;
    _rindex1 = 0;
    _rindex2 = 0;
}


Retuner::~Retuner (void)
{
    delete[] _ipbuff;
    delete[] _xffunc;
    fftwf_free (_Twind);
    fftwf_free (_Wcorr);
    fftwf_free (_Tdata);
    fftwf_free (_Fdata);
    fftwf_destroy_plan (_fwdplan);
    fftwf_destroy_plan (_invplan);
}


int Retuner::process (int nfram, float *inp, float *out)
{
    int    i, k, fi, rt;
    float  r1, r2, dr, u1, u2, v, ns, d1;

    // Pitch shifting is done by resampling the input at the
    // required ratio, and eventually jumping forward or back
    // by one or more pitch period(s). Processing is done in
    // fragments of '_frsize' frames, and the decision to jump
    // forward or back is taken at the start of each fragment.
    // If a jump happens we crossfade over one fragment size. 
    // Every 4 fragments a new pitch estimate is made. Since
    // _fftsize = 16 * _frsize, the estimation window moves
    // by 1/4 of the FFT length.

    fi = _frindex;  // Offset in current fragment.
    r1 = _rindex1;  // First read index.
    r2 = _rindex2;  // Second read index while crossfading. 

    // No assumptions are made about fragments being aligned
    // with process() calls, so we may be in the middle of
    // a fragment here. 

    while (nfram)
    {
        // Don't go past the end of the current fragment.
        k = _frsize - fi;
        if (nfram < k) k = nfram;
        nfram -= k;

        // At 44.1 and 48 kHz upsample by 2.
        if (_upsamp)
        {
            _resampler.inp_count = k;
            _resampler.inp_data = inp;
            _resampler.out_count = 2 * k;
            _resampler.out_data = _ipbuff + _ipindex;
            _resampler.process ();
            _ipindex += 2 * k;
        }
	else
	{
            memcpy (_ipbuff + _ipindex, inp, k * sizeof (float));
            _ipindex += k;
	}

        // Extra samples for interpolation.
        _ipbuff [_ipsize + 0] = _ipbuff [0];
        _ipbuff [_ipsize + 1] = _ipbuff [1];
        _ipbuff [_ipsize + 2] = _ipbuff [2];
        inp += k;
        if (_ipindex == _ipsize) _ipindex = 0;

        // Process available samples.
        dr = _ratio;
        if (_upsamp) dr *= 2;
        if (_xfade)
        {
            // Interpolate and crossfade.
            while (k--)
            {
                i = (int) r1;
                u1 = cubic (_ipbuff + i, r1 - i);
                i = (int) r2;
                u2 = cubic (_ipbuff + i, r2 - i);
                v = _xffunc [fi++];
                *out++ = (1 - v) * u1 + v * u2;
                r1 += dr;
                if (r1 >= _ipsize) r1 -= _ipsize;
                r2 += dr;
                if (r2 >= _ipsize) r2 -= _ipsize;
            }
        }
        else
        {
            // Interpolation only.
            fi += k;
            while (k--)
            {
                i = (int) r1;
                *out++ = cubic (_ipbuff + i, r1 - i);
                r1 += dr;
                if (r1 >= _ipsize) r1 -= _ipsize;
            }
        }
 
        // If at end of fragment check for jump.
        if (fi == _frsize) 
        {
            fi = 0;
            // Estimate the pitch every 4th fragment.
            if (++_frcount == 4)
            {
                _frcount = 0;
                v = findcycle ();
                if (v)
                {
                    // If the pitch estimate succeeds, find the
                    // nearest note and required resampling ratio.
                    _count = 0;
                    _cycle = v;
                    finderror ();
                }
                else if (++_count > 5)
                {
                    // If the pitch estimate fails, the current
                    // ratio is kept for 5 fragments. After that
                    // the signal is considered unvoiced and the
                    // pitch error is reset.
                    _count = 5;
                    _cycle = _frsize;
                    _error = 0;
                }
                else if (_count == 2)
                {
                    // Bias is removed after two unvoiced fragments.
                    _lastnote = -1;
                }
                
                _ratio = powf (2.0f, _corroffs / 12.0f - _error * _corrgain);
            }

            // If the previous fragment was crossfading,
            // the end of the new fragment that was faded
            // in becomes the current read position.
            if (_xfade) r1 = r2;

            // A jump must correspond to an integer number
            // of pitch periods, and to minimise the number
	    // the jumps at low periods we also require it
	    // to be at least one fragment.

            dr = _cycle * (int)(ceilf (_frsize / _cycle));
            if (_upsamp) dr *= 2;

            // We try to keep the read index as close as
	    // possible to the write index minus the latency.
	    // Additionally, to make sure we don't read past
	    // the end of the input we need _frsize * _ratio
	    // samples in the next period, and maybe up to
	    // _frsize * 1.6 in the following one, where
	    // we will have _frsize more input. Combining
	    // these two conditions, if we don't jump we
	    // must have at least this number of samples
	    // available:

	    ns = _frsize * 2.2f + 3;

	    // rt = target for read index to be close to.
	    rt = _ipindex -_latency;
	    if (rt < 0) rt += _ipsize;

	    // d1 = distance to target reading index.
	    d1 = r1 - rt;
	    if      (d1 >  _ipsize / 2) d1 -= _ipsize;
	    else if (d1 < -_ipsize / 2) d1 += _ipsize;

	    // Check for crossfade.
	    _xfade = false;
	    if ((d1 > dr / 2) || (d1 + ns >= _latency))
	    {
		_xfade = true;
		r2 = r1 - dr;
		if (r2 < 0) r2 += _ipsize;
	    }
	    else if (d1 < -dr / 2)
	    {
		_xfade = true;
		r2 = r1 + dr;
		if (r2 >= _ipsize) r2 -= _ipsize;
	    }
        }
    }

    // Save local state.
    _frindex = fi;
    _rindex1 = r1;
    _rindex2 = r2;

    return 0;
}


// Find peak by linear regression on the derivative,
// using n samples before and after position k.
//
float findpeak (float *Y, int k, int n)
{
    int i;
    float x, y, sy1, sxy, sx2;
    
    sy1 = sxy = sx2 = 0.0f;
    for (i = -n; i < n; i++)
    {
        x = i + 0.5f;
        y = Y [k + i] - Y [k + i + 1];
        sy1 += y;
        sx2 += x * x;
        sxy += x * y;
    }
    // Return offset from k.
    return -0.5f * (sy1 * sx2) / (n * sxy);
}        


// Find fundamental pitch using autocorrelation.
// Returns period in samples, or zero for unvoiced.
//
float Retuner::findcycle (void)
{
    int    d, h, i, j, k;
    float  f, x, y, z, m, di, i1, im, y1, ym, a1, am; 

    d = _upsamp ? 2 : 1;
    h = _fftlen / 2;
    j = _ipindex;
    k = _ipsize - 1;

    // Apply window (includes FFT scale factor).
    for (i = 0; i < _fftlen; i++)
    {
        _Tdata [i] = _Twind [i] * _ipbuff [j & k];
        j += d;
    }
    fftwf_execute_dft_r2c (_fwdplan, _Tdata, _Fdata);    

    // Power spectrum, attenuated above 8 kHz.
    f = _fsamp / (_fftlen * 8e3f);
    for (i = 0; i < h; i++)
    {
        x = _Fdata [i][0];
        y = _Fdata [i][1];
        m = i * f;
        _Fdata [i][0] = (x * x + y * y) / (1 + m * m);
        _Fdata [i][1] = 0;
    }
    _Fdata [h][0] = 0;
    _Fdata [h][1] = 0;

    // Inverse FFT of power spectrum is autocorrelation.
    fftwf_execute_dft_c2r (_invplan, _Fdata, _Tdata);    

    // Normalise by total power, and apply window correction.
    m = _Tdata [0] + 1e-10f;
    for (i = 0; i < h; i++) _Tdata [i] /= (m * _Wcorr [i]);
    // Ensure m = 1 for a full scale sine wave, so
    // we can compare to spectrum values in Fdata.
    m /= 3.0f; 

    if (m < 1e-5f)
    {
	// Signal level below -50 dB, assume unvoiced.
	return 0;
    }
    
    // Find first zero crossing.
    i = 0;
    while ((i < _ifmax / 2) && (_Tdata [i] > 0)) i++;
    if (i <= _ifmin / 2)
    {
	// Looks like noise, assume unvoiced. 
	return 0;
    }

    // Search for autocorrelation peaks.
    im = 0.0f; // Period.
    ym = 0.3f; // Autocorrelation.
    am = 0.0f; // Relative power.

    y = _Tdata [i-1];
    z = _Tdata [i];
    while (i < _ifmax)
    {
        x = y;
	y = z;
	z = _Tdata [i + 1];
        if ((y > ym) && (y > x) && (y > z))
	{		    
	    // Find real peak position, using 10 samples
	    // before and after.
            di = findpeak (_Tdata, i, _ifmin / 4);
            if (fabs (di) > _ifmin / 4)
	    {
		// Unreliable peak, reject.
		i++;
                continue;
	    }
	    // Real peak position.
            i1 = i + di;
	    // Corresponding frequency bin.
	    j = (int)(_fftlen / i1 + 0.5f); 
	    // Real peak value.
            y1 = _Tdata [(int)(i1 + 0.5f)];
	    // Relative power in frequency bin.
            a1 = _Fdata [j][0] / m;

	    if (a1 < 1e-4f)
	    {
		// Too low power in spectrum, probably
		// sub-harmonic, reject.
	     	i++;
	     	continue;
	    }
            if (im)
	    {
		// Compare to previous peak.
                if (a1 / am < 1e-2f)
		{
 		    // More than 20 dB below current peak,
  		    // probably not fundamental, reject.
		    i++;
		    continue;
	        }
	    }
	    // Update current best estimate.
	    im = i1;  // Period.
            ym = y1;  // Autocorrelation.
	    am = a1;  // Relative power.
        }
        i++;
    }
    // Best estimate has low autocorrelation,
    // assume unvoiced.
    if (ym < 0.6f) im = 0;
    return im;
}


void Retuner::finderror (void)
{
    int    i, m, im;
    float  a, am, d, dm, f;

    if (!_notemask)
    {
        _error = 0;
        _lastnote = -1;
        return;
    }
    f = log2f (_fsamp / (_cycle * _refpitch));
    dm = 0;
    am = 1;
    im = -1;
    for (i = 0, m = 1; i < 12; i++, m <<= 1)
    {
        if (_notemask & m)
        {
            d = f - (i - 9) / 12.0f;
            d -= floorf (d + 0.5f);
            a = fabsf (d);
            if (i == _lastnote) a -= _notebias;
            if (a < am)
            {
                am = a;
                dm = d;
                im = i;
            }
        }
    }
    
    if (_lastnote == im)
    {
        _error += _corrfilt * (dm - _error);
    }
    else
    {
        _error = dm;
        _lastnote = im;
    }

    // For display only.
    _notebits |= 1 << im;
}


float Retuner::cubic (float *v, float a)
{
    float b, c;

    b = 1 - a;
    c = a * b;
    return (1.0f + 1.5f * c) * (v[1] * b + v[2] * a)
            - 0.5f * c * (v[0] * b + v[1] + v[2] + v[3] * a);
}
