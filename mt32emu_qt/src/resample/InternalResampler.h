#ifndef INTERNAL_RESAMPLER_H
#define INTERNAL_RESAMPLER_H

#include "SampleRateConverter.h"

class SincResampler;

class InternalResampler : public SampleRateConverter {
public:
	InternalResampler(MT32Emu::Synth *synth, double targetSampleRate, SRCQuality quality);
	~InternalResampler();
	void getOutputSamples(MT32Emu::Sample *buffer, unsigned int length);

private:
	MT32Emu::Sample * const inBuffer;
	const MT32Emu::Sample *inBufferPtr;
	MT32Emu::Bit32u inLength;
	SincResampler *sincResampler;
};

#endif // INTERNAL_RESAMPLER_H
