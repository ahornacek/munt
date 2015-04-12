#ifndef IIR_DECIMATOR_H
#define IIR_DECIMATOR_H

typedef double IIRCoefficient;
typedef double BufferedSample;
typedef float FloatSample;

static const unsigned int CHANNEL_COUNT = 2;

class IIRDecimator {
public:
	IIRDecimator(const unsigned int order, const IIRCoefficient numerator[], const IIRCoefficient denominator[]);
	~IIRDecimator();
	void process(const FloatSample *&inSamples, unsigned int &inLength, FloatSample *&outSamples, unsigned int &outLength);
	unsigned int estimateInLength(const unsigned int outLength) const;

private:
	const struct C {
		// Filter coefficients
		const IIRCoefficient *numerator, *denominator;
		// Filter order
		unsigned int order;
		// Index of last delay line element, generally greater than (order + 1) to form a proper binary mask
		unsigned int delayLineMask;
		// Delay line
		BufferedSample(*ringBuffer)[CHANNEL_COUNT];

		C(const unsigned int order, const IIRCoefficient numerator[], const IIRCoefficient denominator[]);
	} c;
	// Index of current sample in delay line
	unsigned int ringBufferPosition;
	// Current phase
	unsigned int phase;

	bool needNextInSample() const;
	void addInSamples(const FloatSample *&inSamples);
	void getOutSamples(FloatSample *&outSamples);
};

#endif // IIR_DECIMATOR_H
