#include "frame.h"

#define DELAY_LINE_SIZE 1<<24

#define MAKE_INTEGRAL_FRACTIONAL(x) \
  int x ## _integral = static_cast<int32_t>(x); \
  float x ## _fractional = x - static_cast<float>(x ## _integral)



template <typename T>
struct DelayLine {

    T DelayBuffer[DELAY_LINE_SIZE] = {0.0f};
    int readPtr = 0; // read ptr
	int desiredReadPtr = 0;
    int writePtr = 0; // write ptr
	float balance = 0;

	T lerp(T v0, T v1, float t) {
		return (1 - t) * v0 + t * v1;
	}

    void write(T in) {
        DelayBuffer[writePtr++] = in; 

		if (writePtr >= DELAY_LINE_SIZE) { 
			writePtr -= DELAY_LINE_SIZE; 
		}
    }

	T getLangrangeInterpolatedDelay(float delay) {
		T out;

		float readPtrf = writePtr - delay;
		readPtrf = std::fmin(readPtrf, writePtr-3.0f);
		int read0 = floor(readPtrf)-1;
		int read1 = read0 + 1;
		int read2 = read0 + 2;
		int read3 = read0 + 3;
		float fDelay = readPtrf - floor(readPtrf) + 1.0f;

		if (read0<0) read0 += DELAY_LINE_SIZE;
		if (read1<0) read1 += DELAY_LINE_SIZE;
		if (read2<0) read2 += DELAY_LINE_SIZE;
		if (read3<0) read3 += DELAY_LINE_SIZE;
	    	
		out =   DelayBuffer[read3] *  fDelay       * (fDelay-1.0f) * (fDelay-2.0f) / 6.0f 
	          - DelayBuffer[read2] *  fDelay       * (fDelay-1.0f) * (fDelay-3.0f) / 2.0f 
        	  + DelayBuffer[read1] *  fDelay       * (fDelay-2.0f) * (fDelay-3.0f) / 2.0f 
        	  - DelayBuffer[read0] * (fDelay-1.0f) * (fDelay-2.0f) * (fDelay-3.0f) / 6.0f;


		return out;
	}

};


//Gonna Hard Code this for FloatFrames for now
template <int WINDOW_SIZE>
struct InterpolatedDelay {

    FloatFrame DelayBuffer[WINDOW_SIZE] = {{0.0f,0.0f}};
	int writePtr = 0;
	FloatFrame accumulator = {0.0,0.0};

	void write(FloatFrame in) {
        DelayBuffer[writePtr++] = in; 

		if (writePtr >= WINDOW_SIZE) { 
			writePtr -= WINDOW_SIZE; 
		}

		accumulator.l = 0.0;
		accumulator.r = 0.0;
    }


	void interpolate(float offset, float scale) {
		MAKE_INTEGRAL_FRACTIONAL(offset); // returns integral and fractional parts (integral is an integer)
		int position = writePtr - offset_integral;
		if(position < 0) {
			position +=WINDOW_SIZE;
		}
		FloatFrame firstValue = DelayBuffer[position];
		float firstValueL = firstValue.l;
		float firstValueR = firstValue.r;

		position--;
		if(position < 0) {
			position +=WINDOW_SIZE;
		}
		FloatFrame secondValue = DelayBuffer[position];
		float secondValueL = secondValue.l;
		float secondValueR = secondValue.r;

		float interpolatedValueL = firstValueL + (secondValueL - firstValueL) * offset_fractional;
		float interpolatedValueR = firstValueR + (secondValueR - firstValueR) * offset_fractional;
		accumulator.l += interpolatedValueL * scale;
		accumulator.r += interpolatedValueR * scale;
    }
};


template <typename T, int N>
struct MultiTapDelayLine {

    T DelayBuffer[DELAY_LINE_SIZE] = {{0.0f, 0.0f}};
    int readPtr[N] = {0}; // read ptr
	int desiredReadPtr[N] = {0};
    int writePtr = 0; // write ptr
	float balance[N] = {0.0};

	float lerp(float v0, float v1, float t) {
		return (1 - t) * v0 + t * v1;
	}

	FloatFrame lerp(FloatFrame v0, FloatFrame v1, float t) {
		return {(1 - t) * v0.l + t * v1.l,(1 - t) * v0.r + t * v1.r};
	}


	void clear() {
		 //std::fill(&buffer_[0], &buffer_[size], 0); //REMEMBER THIS FUNCTION!!!!
		//std::fill(&DelayBuffer[0], &DelayBuffer[DELAY_LINE_SIZE-1], 0); 
		// std::fill(&readPtr[0], &readPtr[N-1], 0); 
		// std::fill(&desiredReadPtr[0], &desiredReadPtr[N-1], 0); 
    	writePtr = 0; // write ptr
	}


	void setDelayTime(int tapNumber, int delaySize) {
		//readPtr[tapNumber] = desiredReadPtr[tapNumber];
		desiredReadPtr[tapNumber] = writePtr - delaySize;
		if (desiredReadPtr[tapNumber] < 0) { 
			desiredReadPtr[tapNumber] += DELAY_LINE_SIZE;
		}
		balance[tapNumber] = 0;
	}

    void write(T in) {
        DelayBuffer[writePtr++] = in; 

		if (writePtr >= DELAY_LINE_SIZE) { 
			writePtr -= DELAY_LINE_SIZE; 
		}
    }

	T getTap(int tapNumber)
	{
		T out;
				
		out = DelayBuffer[readPtr[tapNumber]];

		int distance = desiredReadPtr[tapNumber] - readPtr[tapNumber];
		if(distance !=0) {
			T desiredOut = DelayBuffer[desiredReadPtr[tapNumber]]; 
			out = lerp(out,desiredOut,balance[tapNumber]);
			balance[tapNumber] +=.001;

			if(balance[tapNumber] >= 1) {
				readPtr[tapNumber] = desiredReadPtr[tapNumber];			
			}
		}

		readPtr[tapNumber]++;
		desiredReadPtr[tapNumber]++;


		if (desiredReadPtr[tapNumber] >= DELAY_LINE_SIZE) {
			desiredReadPtr[tapNumber] -= DELAY_LINE_SIZE;
		}
		if (readPtr[tapNumber] >= DELAY_LINE_SIZE) {
			readPtr[tapNumber] -= DELAY_LINE_SIZE;
		}

		return out;

	}
};


struct GranularPitchShifter {
	float ratio;
	float windowSize;
	float phase = 0.0;

	InterpolatedDelay<2048> delayLine;

	FloatFrame process(FloatFrame in, bool useTriangleWindow) {

		phase += (1.0f - ratio) / windowSize;
		if (phase >= 1.0f) {
			phase -= 1.0f;
		}
		if (phase <= 0.0f) {
			phase += 1.0f;
		}

		//Default is Triangle Window - would like other options
		float tri = 1.0f;
		if(useTriangleWindow) { //NOTE: Make windowing functions a parameter
			tri = 2.0f * (phase >= 0.5f ? 1.0f - phase : phase);
		}
		float phaseOffset = phase * windowSize;
		float halfPhaseOffset = phase + windowSize * 0.5f;
		if (halfPhaseOffset >= windowSize) {
			halfPhaseOffset -= windowSize;
		}

		delayLine.write(in);
		delayLine.interpolate(phaseOffset,tri);
		delayLine.interpolate(halfPhaseOffset,1.0f-tri);
		return delayLine.accumulator;
	}
  
	void setRatio(float newRatio) {
		ratio = newRatio;
	}

	//Size comes in as a percentage
	void setSize(float size) {
		//float target_size = 128.0f + (2047.0f - 128.0f) * size * size * size; //WTF?
		float target_size = (2048 * size) -  1; 
		windowSize = target_size;
	}
};