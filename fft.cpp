/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
 *
 *  This file is part of inspectrum.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fft.h"
#include "string.h"
#include <liquid/liquid.h>

FFT::FFT(int size)
{
    fftSize = size;

    fftwIn = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fftwOut = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fftwPlan = fftwf_plan_dft_1d(fftSize, fftwIn, fftwOut, FFTW_FORWARD, FFTW_MEASURE);
}

FFT::~FFT()
{
    if (fftwPlan) fftwf_destroy_plan(fftwPlan);
    if (fftwIn) fftwf_free(fftwIn);
    if (fftwOut) fftwf_free(fftwOut);
}

void FFT::process(void *dest, void *source)
{
    memcpy(fftwIn, source, fftSize * sizeof(fftwf_complex));
    fftwf_execute(fftwPlan);
    memcpy(dest, fftwOut, fftSize * sizeof(fftwf_complex));
}

void calcFFTWindow(float *window, int N, int timeResolution, float beta)
{
    int zeroCount = (N * timeResolution) / 100;
    if ((zeroCount >= 0) && (zeroCount <= N)) {
        int windowSize = N - zeroCount;
        int leadingZeroCount = zeroCount/2;
        
        for (int i = 0; i < leadingZeroCount; i++) {
            window[i] = 0.0;
        }
        for (int i = 0; i < windowSize; i++) {
            window[i + leadingZeroCount] = kaiser(i, windowSize, beta, 0.0);
        }
        for (int i = windowSize + leadingZeroCount; i < N; i++) {
            window[i] = 0.0;
        }
    } else {
        for(int i = 0; i < N; i++) {
            window[i] = 0.0;
        }
    }
}
