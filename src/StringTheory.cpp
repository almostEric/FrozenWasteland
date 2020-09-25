#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "dsp-delay/delayLine.cpp"
#include "dsp-noise/noise.hpp"
#include "filters/Compressor.hpp"

using namespace frozenwasteland::dsp;

#define HISTORY_SIZE (1<<21)
#define MAX_GRAINS 8
#define GRAIN_SPACING 256 //This will undoubtably become a parameter

struct StringTheory : Module {
	enum ParamIds {
		COARSE_TIME_PARAM,
		FINE_TIME_PARAM,
		SAMPLE_TIME_PARAM,
		FEEDBACK_PARAM,
		FEEDBACK_SHIFT_PARAM,
		COLOR_PARAM,
		PLUCK_PARAM,
		NOISE_TYPE_PARAM,
		GRAIN_COUNT_PARAM,
		PHASE_OFFSET_PARAM,
		SPREAD_PARAM,
		RING_MOD_GRAIN_PARAM,
		RING_MOD_MIX_PARAM,
		WINDOW_FUNCTION_PARAM,
		COMPRESSION_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		COARSE_TIME_INPUT,
		FINE_TIME_INPUT,
		SAMPLE_TIME_INPUT,
		V_OCT_INPUT,
		FEEDBACK_INPUT,
		FEEDBACK_SHIFT_INPUT,
		COLOR_INPUT,
		IN_INPUT,
		PLUCK_INPUT,
		FB_RETURN_INPUT,
		SPREAD_INPUT,
		PHASE_OFFSET_INPUT,
		RING_MOD_GRAIN_INPUT,
		RING_MOD_MIX_INPUT,
		EXTERNAL_RING_MOD_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		FB_SEND_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
        NOISE_TYPE_LIGHT,
		COMPRESSION_MODE_LIGHT = NOISE_TYPE_LIGHT + 3,
		WINDOW_FUNCTION_LIGHT = COMPRESSION_MODE_LIGHT + 3,
		NUM_LIGHTS = WINDOW_FUNCTION_LIGHT + 3
	};

	enum NoiseTypes {
		WHITE_NOISE,
		PINK_NOISE,
		GAUSSIAN_NOISE,	
		NUM_NOISE_TYPES	
	};

	enum WindowFunctions {
		NO_WINDOW_FUNCTION,
		HANNING_WINDOW_FUNCTION,
		BLACKMAN_WINDOW_FUNCTION,
		NUM_WINDOW_FUNCTIONS
	};

	enum CompressionModes {
		COMPRESSION_NONE,
		COMPRESSION_CLAMP,
		COMPRESSION_HARD,
		COMPRESSION_ADAPTIVE,
		NUM_COMPRESSION_MODES
	};



	// dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer[MAX_GRAINS];
	// dsp::DoubleRingBuffer<float, 16> outBuffer[MAX_GRAINS];
	// SRC_STATE *src[MAX_GRAINS];

	//Consider Changing to FloatFrame to make this stereo
	DelayLine<float> delayLine[MAX_GRAINS];

	dsp::RCFilter lowpassFilter;
	dsp::RCFilter highpassFilter;

	WhiteNoiseGenerator _whiteNoise;
	PinkNoiseGenerator _pinkNoise;
	GaussianNoiseGenerator _gaussianNoise;

	Compressor compressor[MAX_GRAINS];

	dsp::SchmittTrigger pluckTrigger, noiseTypeTrigger,windowFunctionTrigger,compressionModeTrigger;

	float lastDelayTime = -1;
	float lastWet[MAX_GRAINS] = {0.f};
	float individualWet[MAX_GRAINS] = {0.f};
	bool acceptingInput[MAX_GRAINS]; //If true, means pluck has been activated and will accept input 
	float timeDelay[MAX_GRAINS] = {0.0};
	float timeElapsed[MAX_GRAINS] = {0.0};
	int noiseType = WHITE_NOISE;
	int windowFunction = NO_WINDOW_FUNCTION;
	int grainCount = MAX_GRAINS;
	uint8_t compressionMode = COMPRESSION_NONE;

	float sampleRate;

	void changeCompressionMode (uint8_t newMode) {
		if (newMode == COMPRESSION_HARD) {
			// reset to hard compression
			for(int i=0;i<MAX_GRAINS;i++) {
				compressor[i].setCoefficients(1.0f, 30.0f, 5.0f, 20.0f, sampleRate);
			}
		}

		// compressionMode = newMode;
	}

	inline float limit (float in, int which) {
		if (compressionMode == COMPRESSION_NONE) {
			return in;
		} else if (compressionMode == COMPRESSION_CLAMP) {
			return clamp(in, -6.0f, 6.0f);
		} else if (compressionMode == COMPRESSION_HARD) {
			return compressor[which].process(in);
		} else {
			// adaptive compression	
			float ratio = fabs(in) / 4.0f;
			compressor[which].setCoefficients(1.0f, 30.0f, 5.0f, ratio, sampleRate);
			return compressor[which].process(in);
		}
	}


	float HanningWindow(float phase) {
		return 0.5f * (1 - cosf(2 * M_PI * phase));
	}

	float BlackmanWindow(float phase) {
		float a0 = 0.42;
		float a1 = 0.5;
		float a2 = 0.08;
		return a0 - (a1 * cosf(2 * M_PI * phase)) + (a2 * cosf(4 * M_PI * phase)) ;
	}

	float lerp(float v0, float v1, float t) {
		return (1 - t) * v0 + t * v1;
	}
	

	StringTheory() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(COARSE_TIME_PARAM, 0.0f, 1.f, 0.5f, "Coarse Time", " ms",0.5f / 1e-3,1);
		configParam(FINE_TIME_PARAM, 0.01f, 20.f, 0.01f, "Fine Time", " ms");
		configParam(SAMPLE_TIME_PARAM, 0.0f, 200.f, 0.0f, "Samples");
		configParam(GRAIN_COUNT_PARAM, 1.0f, MAX_GRAINS, 1.0f, "Grain Count");
		configParam(PHASE_OFFSET_PARAM, 0.0f, 1.f, 0.0f, "Phase Offset", "%", 0, 100);
		configParam(SPREAD_PARAM, 0.0f, 1.f, 0.0f, "Spread", "%", 0, 100);
		configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.5f, "Feedback", "%", 0, 100);
		configParam(FEEDBACK_SHIFT_PARAM, 0.f, MAX_GRAINS, 0.0f, "Feedback Shift");
		configParam(COLOR_PARAM, 0.f, 1.f, 0.5f, "Color", "%", 0, 100);
		configParam(PLUCK_PARAM, 0.f, 1.f, 0.0f);
		configParam(RING_MOD_GRAIN_PARAM, 0.f, MAX_GRAINS, 0.0f, "Ring Mod Grain");
		configParam(RING_MOD_MIX_PARAM, 0.f, 1.f, 0.0f, "Ring Mod Mix", "%", 0, 100);
		configParam(NOISE_TYPE_PARAM, 0.f, 1.f, 0.0f);
		configParam(WINDOW_FUNCTION_PARAM, 0.f, 1.f, 0.0f);
		configParam(COMPRESSION_MODE_PARAM, 0.0f, 1.0f, 0.0f);

		// for(int i=0;i<MAX_GRAINS;i++) {
		// 	src[i] = src_new(SRC_SINC_FASTEST, 1, NULL);
		// 	assert(src[i]);
		// }
		//src = src_new(SRC_LINEAR, 1, NULL);
	}

	// ~StringTheory() {
	// 	// for(int i=0;i<MAX_GRAINS;i++) {
	// 	// 	src_delete(src[i]);
	// 	// }
	// }

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
        json_object_set_new(rootJ, "noiseType", json_integer((int) noiseType));
        json_object_set_new(rootJ, "windowFunction", json_integer((int) windowFunction));
        json_object_set_new(rootJ, "compressionMode", json_integer((int) compressionMode));

		
		
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

        json_t *sdnt = json_object_get(rootJ, "noiseType");
		if (sdnt)
			noiseType = json_integer_value(sdnt);		

        json_t *sdwf = json_object_get(rootJ, "windowFunction");
		if (sdwf)
			windowFunction = json_integer_value(sdwf);		

        json_t *sdcm = json_object_get(rootJ, "compressionMode");
		if (sdcm) {
			compressionMode = json_integer_value(sdcm);	
			changeCompressionMode(compressionMode);
		}

	}

	void process(const ProcessArgs &args) override {
		

		sampleRate = args.sampleRate;

		grainCount = params[GRAIN_COUNT_PARAM].getValue();

		// Compute delay time in seconds - eventually milliseconds
		float coarseDelay = params[COARSE_TIME_PARAM].getValue() + inputs[COARSE_TIME_INPUT].getVoltage() / 10.f;
		coarseDelay = clamp(coarseDelay, 0.f, 1.f);
		coarseDelay = 1e-3 * std::pow(0.5f / 1e-3, coarseDelay);

		float fineDelay = params[FINE_TIME_PARAM].getValue() + inputs[FINE_TIME_INPUT].getVoltage() / 10.f;
		fineDelay = 1e-3 * fineDelay;

		// Number of delay samples
		float delay = coarseDelay + fineDelay;

		float pitch = inputs[V_OCT_INPUT].getVoltage();
		delay = delay / std::pow(2.0f, pitch);

		float index = (delay * sampleRate) + params[SAMPLE_TIME_PARAM].getValue() ; // Maybe get rid of rounding

		float pluckInput = params[PLUCK_PARAM].getValue();
		if(inputs[PLUCK_INPUT].isConnected()) {
			pluckInput += inputs[PLUCK_INPUT].getVoltage();
		} 
		if(pluckTrigger.process(pluckInput)) {
			for(int i=0; i<grainCount;i++) {
				acceptingInput[i] = true;
				timeElapsed[i] = 0.0;
				timeDelay[i] = ((float) i) * index / 2.0 * (params[PHASE_OFFSET_PARAM].getValue() + inputs[PHASE_OFFSET_INPUT].getVoltage() / 10.0f );
			}
		}	

		if(noiseTypeTrigger.process(params[NOISE_TYPE_PARAM].getValue())) {
			noiseType = (noiseType + 1) % NUM_NOISE_TYPES;
		}	

		if(windowFunctionTrigger.process(params[WINDOW_FUNCTION_PARAM].getValue())) {
			windowFunction = (windowFunction + 1) % NUM_WINDOW_FUNCTIONS;
		}	
		switch (windowFunction) {
			case NO_WINDOW_FUNCTION :
				lights[WINDOW_FUNCTION_LIGHT].value = 0.0f;
				lights[WINDOW_FUNCTION_LIGHT+1].value = 0.0f;
				lights[WINDOW_FUNCTION_LIGHT+2].value = 0.0f;
				break;
			case HANNING_WINDOW_FUNCTION :
				lights[WINDOW_FUNCTION_LIGHT].value = 0.0f;
				lights[WINDOW_FUNCTION_LIGHT+1].value = 1.0f;
				lights[WINDOW_FUNCTION_LIGHT+2].value = 0.0f;
				break;
			case BLACKMAN_WINDOW_FUNCTION :
				lights[WINDOW_FUNCTION_LIGHT].value = 0.0f;
				lights[WINDOW_FUNCTION_LIGHT+1].value = 0.0f;
				lights[WINDOW_FUNCTION_LIGHT+2].value = 1.0f;
				break;
		}

		if(compressionModeTrigger.process(params[COMPRESSION_MODE_PARAM].getValue())) {
			compressionMode = (compressionMode + 1) % NUM_COMPRESSION_MODES;
			changeCompressionMode(compressionMode);
		}
		switch(compressionMode) {
			case COMPRESSION_NONE :
				lights[COMPRESSION_MODE_LIGHT].value = 0;
				lights[COMPRESSION_MODE_LIGHT+1].value = 0;
				lights[COMPRESSION_MODE_LIGHT+2].value = 0;
				break;
			case COMPRESSION_CLAMP :
				lights[COMPRESSION_MODE_LIGHT].value = 1;
				lights[COMPRESSION_MODE_LIGHT+1].value = 0;
				lights[COMPRESSION_MODE_LIGHT+2].value = 0;
				break;
			case COMPRESSION_HARD :
				lights[COMPRESSION_MODE_LIGHT].value = 1;
				lights[COMPRESSION_MODE_LIGHT+1].value = 1;
				lights[COMPRESSION_MODE_LIGHT+2].value = 0;
				break;
			case COMPRESSION_ADAPTIVE :
				lights[COMPRESSION_MODE_LIGHT].value = 0;
				lights[COMPRESSION_MODE_LIGHT+1].value = 1;
				lights[COMPRESSION_MODE_LIGHT+2].value = 0;
				break;
		}


		int feedBackShift = clamp(params[FEEDBACK_SHIFT_PARAM].getValue() + inputs[FEEDBACK_SHIFT_INPUT].getVoltage() / 10.0f,0.0,(float)grainCount);
		int ringModGrain = clamp(params[RING_MOD_GRAIN_PARAM].getValue() + inputs[RING_MOD_GRAIN_INPUT].getVoltage() / 10.0f,0.0,(float)grainCount);
		float ringModMix = clamp(params[RING_MOD_MIX_PARAM].getValue() + inputs[RING_MOD_MIX_INPUT].getVoltage() / 10.0f,0.0f,1.0f);


		float ringModIn = 0.0;
		switch(noiseType) {
			case WHITE_NOISE :
				lights[NOISE_TYPE_LIGHT].value = 1;
				lights[NOISE_TYPE_LIGHT + 1].value = 1;
				lights[NOISE_TYPE_LIGHT + 2].value = 1;
				ringModIn = _whiteNoise.next() * 5.0f;
				break;
			case PINK_NOISE :
				lights[NOISE_TYPE_LIGHT].value = 1;
				lights[NOISE_TYPE_LIGHT + 1].value = 0.1;
				lights[NOISE_TYPE_LIGHT + 2].value = 0.1;
				ringModIn = _pinkNoise.next() * 5.0f;
				break;
			case GAUSSIAN_NOISE :
				lights[NOISE_TYPE_LIGHT].value = 0.2f;
				lights[NOISE_TYPE_LIGHT + 1].value = 0.2f;
				lights[NOISE_TYPE_LIGHT + 2].value = 0.2f;
				ringModIn = _gaussianNoise.next() * 5.0f;
				break;
		}
		if(inputs[EXTERNAL_RING_MOD_INPUT].isConnected()) {
			ringModIn = inputs[EXTERNAL_RING_MOD_INPUT].getVoltage();
		}

		float feedback = params[FEEDBACK_PARAM].getValue() + inputs[FEEDBACK_INPUT].getVoltage() / 10.f;
		feedback = clamp(feedback, 0.f, 1.f);
	
		for(int i=0; i<grainCount;i++) {
			timeDelay[i] -= 1.0;
			if(timeDelay[i] > 0)
				continue;

			timeElapsed[i] += 1.0;
			if(timeElapsed[i] > index * (1.0 + (float)i / (float)grainCount * (params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getVoltage() / 10.0f ))) {
				acceptingInput[i] = false;
			}

			// if(lastDelayTime != index) {
			// 	delayLine[i].setDelayTime(index);
			// }


			float in = 0.0;
			if(acceptingInput[i]) {
				float phase = timeElapsed[i] / index;
				// Get input to delay block
				if(inputs[IN_INPUT].isConnected()) {
					in = inputs[IN_INPUT].getVoltage();
				} else {
					switch(noiseType) {
						case WHITE_NOISE :
							in = _whiteNoise.next() * 5.0f;
							break;
						case PINK_NOISE :
							in = _pinkNoise.next() * 5.0f;
							break;
						case GAUSSIAN_NOISE :
							in = _gaussianNoise.next() * 5.0f;
							break;
					}
				}
				switch (windowFunction) {
					case NO_WINDOW_FUNCTION :
						break;
					case HANNING_WINDOW_FUNCTION :
						in = in * HanningWindow(phase);
						break;
					case BLACKMAN_WINDOW_FUNCTION :
						in = in * BlackmanWindow(phase);
						break;
				}
			}

			float dry = clamp(in + lastWet[i] * feedback,-10.0,10.0);

			// Push dry sample into history buffer
			delayLine[i].write(dry);
		
			//individualWet[i] = delayLine[i].getDelay();
			individualWet[i] = delayLine[i].getLangrangeInterpolatedDelay(index);
		
			if(i < ringModGrain) {
				float ringModdedValue = ringModIn * individualWet[i] / 5.0f;
				individualWet[i] = lerp(individualWet[i], ringModdedValue, ringModMix);
			}

			outputs[FB_SEND_OUTPUT].setVoltage(individualWet[i],i);

			if(inputs[FB_RETURN_INPUT].isConnected()) {
				individualWet[i] = clamp(inputs[FB_RETURN_INPUT].getPolyVoltage(i),-10.0,10.0);	
			}

			//Apply compression
			individualWet[i] = limit(individualWet[i],i);

			// Apply color to delay wet output
			float color = params[COLOR_PARAM].getValue() + inputs[COLOR_INPUT].getVoltage() / 10.f;
			color = clamp(color, 0.f, 1.f);
			float colorFreq = std::pow(100.f, 2.f * color - 1.f);

			float lowpassFreq = clamp(20000.f * colorFreq, 20.f, 20000.f);
			lowpassFilter.setCutoffFreq(lowpassFreq / sampleRate);
			lowpassFilter.process(individualWet[i]);
			individualWet[i] = lowpassFilter.lowpass();

			float highpassFreq = clamp(20.f * colorFreq, 20.f, 20000.f);
			highpassFilter.setCutoff(highpassFreq / sampleRate);
			highpassFilter.process(individualWet[i]);
			individualWet[i] = highpassFilter.highpass();
			
		}

		lastDelayTime = index;

		float wet = 0.f;
		for(int i= 0; i<grainCount;i++) {
			lastWet[i] = individualWet[(i + feedBackShift) % grainCount];
			if(i < ringModGrain) {
				float ringModdedValue = ringModIn * individualWet[i] / 5.0f;
				individualWet[i] = lerp(individualWet[i], ringModdedValue, ringModMix);
			}
			wet += individualWet[i];
		}
		wet = wet / std::sqrt((float)grainCount); //RMS 
		
		outputs[FB_SEND_OUTPUT].setChannels(grainCount);
		outputs[OUT_OUTPUT].setVoltage(wet);
	}
};


struct StringTheoryWidget : ModuleWidget {
	StringTheoryWidget(StringTheory *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/StringTheory.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundSmallFWKnob>(Vec(5, 40), module, StringTheory::COARSE_TIME_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(40, 40), module, StringTheory::FINE_TIME_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 40), module, StringTheory::SAMPLE_TIME_PARAM));

		addParam(createParam<RoundSmallFWKnob>(Vec(5, 105), module, StringTheory::FEEDBACK_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(45, 105), module, StringTheory::FEEDBACK_SHIFT_PARAM));

		addParam(createParam<RoundSmallFWSnapKnob>(Vec(5, 165), module, StringTheory::GRAIN_COUNT_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(40, 165), module, StringTheory::PHASE_OFFSET_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 165), module, StringTheory::SPREAD_PARAM));

		addParam(createParam<RoundSmallFWKnob>(Vec(5, 222), module, StringTheory::COLOR_PARAM));
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(40, 222), module, StringTheory::RING_MOD_GRAIN_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 222), module, StringTheory::RING_MOD_MIX_PARAM));

		addParam(createParam<TL1105>(Vec(30, 307), module, StringTheory::PLUCK_PARAM));
		addParam(createParam<TL1105>(Vec(10, 278), module, StringTheory::COMPRESSION_MODE_PARAM));
		addParam(createParam<TL1105>(Vec(60, 280), module, StringTheory::NOISE_TYPE_PARAM));
		addParam(createParam<TL1105>(Vec(60, 307), module, StringTheory::WINDOW_FUNCTION_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(8, 67), module, StringTheory::COARSE_TIME_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(42, 67), module, StringTheory::FINE_TIME_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(77, 67), module, StringTheory::SAMPLE_TIME_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(7, 133), module, StringTheory::FEEDBACK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(48, 133), module, StringTheory::FEEDBACK_SHIFT_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(80, 133), module, StringTheory::FB_RETURN_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(42, 192), module, StringTheory::PHASE_OFFSET_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(77, 192), module, StringTheory::SPREAD_INPUT));
		

		addInput(createInput<FWPortInSmall>(Vec(7, 249), module, StringTheory::COLOR_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(42, 249), module, StringTheory::RING_MOD_GRAIN_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(77, 249), module, StringTheory::RING_MOD_MIX_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(7, 305), module, StringTheory::PLUCK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(6, 340), module, StringTheory::V_OCT_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(32, 340), module, StringTheory::IN_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(58, 340), module, StringTheory::EXTERNAL_RING_MOD_INPUT));



		addOutput(createOutput<FWPortOutSmall>(Vec(80, 105), module, StringTheory::FB_SEND_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(82, 340), module, StringTheory::OUT_OUTPUT));

		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(31, 278), module, StringTheory::COMPRESSION_MODE_LIGHT));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(81, 280), module, StringTheory::NOISE_TYPE_LIGHT));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(81, 307), module, StringTheory::WINDOW_FUNCTION_LIGHT));

	}
};


Model *modelStringTheory = createModel<StringTheory, StringTheoryWidget>("StringTheory");
