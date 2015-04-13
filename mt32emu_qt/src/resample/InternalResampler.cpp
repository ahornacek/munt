/* Copyright (C) 2015 Sergey V. Mikayev
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

#include "InternalResampler.h"

#include <QtGlobal>

#include "SincResampler.h"
#include "IIRDecimator.h"

using namespace MT32Emu;

static const double DB_SNR = 100;
static const unsigned int CHANNEL_COUNT = 2;

InternalResampler::InternalResampler(Synth *synth, double targetSampleRate, SRCQuality quality) :
	SampleRateConverter(synth, targetSampleRate, quality),
	inBuffer(new Sample[CHANNEL_COUNT * MAX_SAMPLES_PER_RUN]),
	inBufferPtr(inBuffer),
	inLength()
{
	sincResampler = new SincResampler(targetSampleRate, synth->getStereoOutputSampleRate(), 20000.0, 28000.0, DB_SNR);
}

InternalResampler::~InternalResampler() {
	delete sincResampler;
	delete[] inBuffer;
}

void InternalResampler::getOutputSamples(Sample *buffer, unsigned int length) {
	while (length > 0) {
		if (inLength == 0) {
			inLength = sincResampler->estimateInLength(length);
			inLength = qBound((Bit32u)0, inLength, (Bit32u)MAX_SAMPLES_PER_RUN);
			synth->render(inBuffer, inLength);
			inBufferPtr = inBuffer;
		}
		sincResampler->process(inBufferPtr, inLength, buffer, length);
	}
}
