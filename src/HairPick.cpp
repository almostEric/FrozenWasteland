#include "FrozenWasteland.hpp"
#include "dsp/samplerate.hpp"
#include "dsp/digital.hpp"
#include "ringbuffer.hpp"
#include <iostream>

#define HISTORY_SIZE (1<<22)
#define NUM_TAPS 64
#define CHANNELS 2
#define DIVISIONS 21
#define NUM_PATTERNS 16
#define NUM_FEEDBACK_TYPES 4


struct HairPick : Module {
	typedef float T;

	enum ParamIds {
		CLOCK_DIV_PARAM,
		SIZE_PARAM,
		PATTERN_TYPE_PARAM,
		NUMBER_TAPS_PARAM,
		EDGE_LEVEL_PARAM,
		TENT_LEVEL_PARAM,
		TENT_TAP_PARAM,
		FEEDBACK_TYPE_PARAM,
		FEEDBACK_AMOUNT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		CLOCK_DIVISION_CV_INPUT,
		VOLT_OCTAVE_INPUT,
		SIZE_CV_INPUT,
		PATTERN_TYPE_CV_INPUT,
		NUMBER_TAPS_CV_INPUT,
		EDGE_LEVEL_CV_INPUT,
		TENT_LEVEL_CV_INPUT,
		TENT_TAP_CV_INPUT,
		FEEDBACK_TYPE_CV_INPUT,
		FEEDBACK_CV_INPUT,
		IN_L_INPUT,
		IN_R_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	enum FeedbackTypes {
		FEEDBACK_GUITAR,
		FEEDBACK_SITAR,
		FEEDBACK_CLARINET,
		FEEDBACK_RAW,
	};

	const char* combPatternNames[NUM_PATTERNS] = {"Uniform","Flat Middle","Early Comb","Fibonacci","Flat Comb","Late Comb","Rev. Fibonacci","Ess Comb","Rand Uniform","Rand Middle","Rand Early","Rand Fibonacci","Rand Flat","Rand Late","Rand Rev Fib","Rand Ess"};
	const float combPatterns[NUM_PATTERNS][NUM_TAPS] = {
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Uniform
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,24.5,25.0,25.5,26.0,26.5,27.0,27.5,28.0,28.5,29.0,29.5,30.0,30.5,31.5,32.0,32.5,33.0,33.5,34.0,34.5,35.0,35.5,36.0,36.5,37.0,37.5,38.0,38.5,39.0,39.5,40.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Flat Middle
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,1.0,15.5,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,34.0,36.0,38.0,40.0,42.0,44.0,46.0,48.0,50.0,52.0,54.0,56.0,58.0,60.0,62.0,64.0}, // Early Comb
		{0.031f,0.037f,0.043f,0.05f,0.056f,0.062f,0.068f,0.074f,0.087f,0.099f,0.111f,0.124f,0.142f,0.161f,0.18f,0.198f,0.229f,0.26f,0.291f,0.322f,0.372f,0.421f,0.471f,0.52f,0.601f,0.681f,0.762f,0.842f,0.972f,1.102f,1.232f,1.362f,1.573f,1.783f,1.994f,2.204f,2.545f,2.885f,3.226f,3.567f,4.118f,4.669f,5.22f,5.771f,6.663f,7.554f,8.446f,9.337f,10.78f,12.223f,13.666f,15.108f,17.443f,19.777f,22.111f,24.446f,28.223f,32.0f,35.777f,39.554f,45.666f,51.777f,57.889f,64.0f}, // Fibonacci
		{2.0f,4.0f,6.0,8.0,10.0,12.0,14.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,24.5,25.0,25.5,26.0,26.5,27.0,27.5,28.0,28.5,29.0,29.5,30.0,30.5,31.0,31.5,32.0,32.5,33.0,33.5,34.0,34.5,35.0,35.5,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,50.0,52.0,54.0,56.0,58.0,60.0,62.0,64.0}, // Flat Comb
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{0.031f,6.142f,12.254f,18.365f,24.477f,28.254f,32.031f,35.808f,39.585f,41.92f,44.254f,46.588f,48.923f,50.365f,51.808f,53.251f,54.693f,55.585f,56.477f,57.368f,58.26f,58.811f,59.362f,59.913f,60.464f,60.805f,61.146f,61.486f,61.827f,62.037f,62.248f,62.458f,62.669f,62.799f,62.929f,63.059f,63.189f,63.269f,63.35f,63.43f,63.511f,63.56f,63.61f,63.659f,63.709f,63.74f,63.771f,63.802f,63.833f,63.851f,63.87f,63.889f,63.907f,63.92f,63.932f,63.944f,63.957f,63.963f,63.969f,63.975f,63.981f,63.988f,63.994f,64.0f}, // Rev Fibonacci
		{0.5f,1.0f,1.5,2.0,2.5,3.0,3.5,4.0,4.5,5.0,5.5,6.0,6.5,7.0,7.5,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,18.0,20.0,22.0,24.0,26.0,30.0,32.0,34.0,36.0,38.0,40.0,42.0,44.0,46.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,56.5,57.0,57.5,58.0,58.5,59.0,59.5,60.0,60.5,61.0,61.5,62.0,62.5,63.0,63.5,64.0,64.0}, // Ess Comb
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0,18.0,19.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0,31.0,32.0,33.0,34.0,35.0,36.0,37.0,38.0,39.0,40.0,41.0,42.0,43.0,44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0,52.0,53.0,54.0,55.0,56.0,57.0,58.0,59.0,60.0,61.0,62.0,63.0,64.0}, // Straight time
	};

	const char* feedbackTypeNames[NUM_FEEDBACK_TYPES] = {"Guitar","Sitar","Clarinet","Raw"};


	int combPattern = 0;
	int feedbackType = 0;	
	float edgeLevel = 0.0f;
	float tentLevel = 1.0f;
	int tentTap = 32;
	
	SchmittTrigger clockTrigger;
	float divisions[DIVISIONS] = {1/256.0,1/192.0,1/128.0,1/96.0,1/64.0,1/48.0,1/32.0,1/24.0,1/16.0,1/13.0,1/12.0,1/11.0,1/8.0,1/7.0,1/6.0,1/5.0,1/4.0,1/3.0,1/2.0,1/1.5,1};
	const char* divisionNames[DIVISIONS] = {"/256","/192","/128","/96","/64","/48","/32","/24","/16","/13","/12","/11","/8","/7","/6","/5","/4","/3","/2","/1.5","x 1"};
	int division;
	float time = 0.0;
	float duration = 0;
	float baseDelay;
	bool secondClockReceived = false;


	bool combActive[NUM_TAPS];
	float combLevel[NUM_TAPS];


	MultiTapDoubleRingBuffer<float, HISTORY_SIZE,NUM_TAPS> historyBuffer[CHANNELS];
	DoubleRingBuffer<float, 16> outBuffer[NUM_TAPS][CHANNELS]; 
	SampleRateConverter<1> src;
	float lastFeedback[CHANNELS] = {0.0f,0.0f};

	float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	int muteTap(int tapIndex)
    {
        int tapNumber = 0;
        for (int levelIndex = 0; levelIndex < 6; levelIndex += 1)
        {
            tapNumber += (tapIndex & (1 << levelIndex)) > 0 ? (1 << (5-levelIndex)) : 0;
        }
        return 64-tapNumber;     
    }

	float envelope(int tapNumber, float edgeLevel, float tentLevel, int tentNumber) {
        float t;
        if (tapNumber <= tentNumber)
        {
            t = (float)tapNumber / tentNumber;
            return (1 - t) * edgeLevel + t * tentLevel;
        }
        else
        {
            t = (float)(tapNumber-tentNumber) / (63-tentNumber);
            return (1 - t) * tentLevel+ t * edgeLevel;
        }
    }

	HairPick() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
	}

	const char* tapNames[NUM_TAPS+2] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","ALL","EXT"};



	void step() override;
};


void HairPick::step() {

	combPattern = (int)clamp(params[PATTERN_TYPE_PARAM].value + (inputs[PATTERN_TYPE_CV_INPUT].value * 1.5f),0.0f,15.0);
	feedbackType = (int)clamp(params[FEEDBACK_TYPE_PARAM].value + (inputs[FEEDBACK_TYPE_CV_INPUT].value / 10.0f),0.0f,3.0);

	int tapCount = (int)clamp(params[NUMBER_TAPS_PARAM].value + (inputs[NUMBER_TAPS_CV_INPUT].value * 6.4f),1.0f,64.0);


	edgeLevel = clamp(params[EDGE_LEVEL_PARAM].value + (inputs[EDGE_LEVEL_CV_INPUT].value / 10.0f),0.0f,1.0);
	tentLevel = clamp(params[TENT_LEVEL_PARAM].value + (inputs[TENT_LEVEL_CV_INPUT].value / 10.0f),0.0f,1.0);

	tentTap = (int)clamp(params[TENT_TAP_PARAM].value + (inputs[TENT_TAP_CV_INPUT].value * 6.3f),1.0f,63.0f);


	//Initialize muting - set all active first
	for(int tapNumber = 0;tapNumber<NUM_TAPS;tapNumber++) {
		combActive[tapNumber] = true;	
	}
	//Turn off as needed
	for(int tapIndex = NUM_TAPS-1;tapIndex >= tapCount;tapIndex--) {
		int tapNumber = muteTap(tapIndex);
		combActive[tapNumber] = false;
	}

	float divisionf = params[CLOCK_DIV_PARAM].value;
	if(inputs[CLOCK_DIVISION_CV_INPUT].active) {
		divisionf +=(inputs[CLOCK_DIVISION_CV_INPUT].value * (DIVISIONS / 10.0));
	}
	divisionf = clamp(divisionf,0.0f,20.0f);
	division = (DIVISIONS-1) - int(divisionf); //TODO: Reverse Division Order

	time += 1.0 / engineGetSampleRate();
	if(inputs[CLOCK_INPUT].active) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if(secondClockReceived) {
				duration = time;
			}
			time = 0;
			//secondClockReceived = true;			
			secondClockReceived = !secondClockReceived;			
		}
		//lights[CLOCK_LIGHT].value = time > (duration/2.0);
	}
	
	if(inputs[CLOCK_INPUT].active) {
		baseDelay = duration / divisions[division];
	} else {
		baseDelay = clamp(params[SIZE_PARAM].value, 0.001f, 10.0f);			
	}
	float pitchShift = powf(2.0f,inputs[VOLT_OCTAVE_INPUT].value);
	baseDelay = baseDelay / pitchShift;

	
	for(int channel = 0;channel < CHANNELS;channel++) {
		// Get input to delay block
		float in = 0.0f;
		if(channel == 0) {
			in = inputs[IN_L_INPUT].value;
		} else {
			in = inputs[IN_R_INPUT].active ? in = inputs[IN_R_INPUT].value : in = inputs[IN_L_INPUT].value;			
		}
		float feedbackAmount = clamp(params[FEEDBACK_AMOUNT_PARAM].value + (inputs[FEEDBACK_CV_INPUT].value / 10.0f), 0.0f, 1.0f);
		float feedbackInput = lastFeedback[channel];

		float dry = in + feedbackInput * feedbackAmount;

		// Push dry sample into history buffer
		if (!historyBuffer[channel].full(NUM_TAPS-1)) {
			historyBuffer[channel].push(dry);
		}

		float delayNonlinearity = 1.0f;
		float percentChange = 10.0f;
		//Apply non-linearity
		if(feedbackType == FEEDBACK_SITAR) {
			if(in > 0) {
				delayNonlinearity = 1 + (in/(10.0f * 100.0f)*percentChange); //Test sitar will change length by 1%
			}
		}


		float wet = 0.0f; // This is the mix of delays and input that is outputed
		float feedbackValue = 0.0f; // This is the output of a tap that gets sent back to input
		for(int tap = 0; tap < NUM_TAPS;tap++) { 

			int inFrames = min(historyBuffer[channel].size(tap), 16); 
		
			// Compute delay time in seconds
			float delay = baseDelay * delayNonlinearity * combPatterns[combPattern][tap] / NUM_TAPS; 

			//float delayMod = 0.0f;
			// Number of delay samples
			float index = delay * engineGetSampleRate();


			// How many samples do we need consume to catch up?
			float consume = index - historyBuffer[channel].size(tap);

			if (outBuffer[tap][channel].empty()) {
				
				double ratio = 1.0;
				if (consume <= -16) 
					ratio = 0.5;
				else if (consume >= 16) 
					ratio = 2.0;

				float inSR = engineGetSampleRate();
		        float outSR = ratio * inSR;

		        int outFrames = outBuffer[tap][channel].capacity();
		        src.setRates(inSR, outSR);
		        src.process((const Frame<1>*)historyBuffer[channel].startData(tap), &inFrames, (Frame<1>*)outBuffer[tap][channel].endData(), &outFrames);
		        outBuffer[tap][channel].endIncr(outFrames);
		        historyBuffer[channel].startIncr(tap, inFrames);
			}

			float wetTap = 0.0f;
			if (!outBuffer[tap][channel].empty()) {
				wetTap = outBuffer[tap][channel].shift();
				if(tap == NUM_TAPS-1) {
					feedbackValue = wetTap;
				}
				if(!combActive[tap]) {
					wetTap = 0.0f;
				} else {
					wetTap = wetTap * envelope(tap,edgeLevel,tentLevel,tentTap);
				}
			}

			wet += wetTap;
		}


		float feedbackWeight = 0.5;
		switch(feedbackType) {
			case FEEDBACK_GUITAR :
				feedbackValue = (feedbackWeight * feedbackValue) + ((1-feedbackWeight) * lastFeedback[channel]);
				break;
			case FEEDBACK_SITAR :
				feedbackValue = (feedbackWeight * feedbackValue) + ((1-feedbackWeight) * lastFeedback[channel]);
				break;
			case FEEDBACK_CLARINET :
				feedbackValue = (feedbackWeight * feedbackValue) + ((1-feedbackWeight) * lastFeedback[channel]);
				break;
			case FEEDBACK_RAW :
				//feedbackValue = wet;
				break;
		}
		
		//feedbackValue = clamp(feedbackValue,-10.0f,10.0f);


		lastFeedback[channel] = feedbackValue;

		float out = wet;
		
		outputs[OUT_L_OUTPUT + channel].value = out;
	}
}

struct HPStatusDisplay : TransparentWidget {
	HairPick *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	HPStatusDisplay() {
		font = Font::load(assetPlugin(plugin, "res/fonts/01 Digit.ttf"));
	}

	void drawProgress(NVGcontext *vg, float phase) 
	{
		const float rotate90 = (M_PI) / 2.0;
		float startArc = 0 - rotate90;
		float endArc = (phase * M_PI * 2) - rotate90;

		// Draw indicator
		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		{
			nvgBeginPath(vg);
			nvgArc(vg,75.8,170,35,startArc,endArc,NVG_CW);
			nvgLineTo(vg,75.8,170);
			nvgClosePath(vg);
		}
		nvgFill(vg);
	}

	void drawDivision(NVGcontext *vg, Vec pos, int division) {
		nvgFontSize(vg, 20);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->divisionNames[division]);
		nvgText(vg, pos.x, pos.y, text, NULL);
	}

	void drawDelayTime(NVGcontext *vg, Vec pos, float delayTime) {
		nvgFontSize(vg, 28);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%6.0f", delayTime*1000);	
		nvgText(vg, pos.x, pos.y, text, NULL);
	}

	void drawPatternType(NVGcontext *vg, Vec pos, int patternType) {
		nvgFontSize(vg, 11);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->combPatternNames[patternType]);
		nvgText(vg, pos.x, pos.y, text, NULL);
	}

	void drawEnvelope(NVGcontext *vg, Vec pos,float edgeLevel, float tentLevel, int tentTap) {
		nvgStrokeWidth(vg, 1);
		nvgStrokeColor(vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));	
		nvgBeginPath(vg);
		float xOffset = (float)tentTap/64*97;
		nvgMoveTo(vg, pos.x+0, pos.y+20*(1-edgeLevel));
		nvgLineTo(vg, pos.x+xOffset, pos.y+20*(1-tentLevel));
		nvgLineTo(vg, pos.x+97, pos.y+20*(1-edgeLevel));
			
		nvgStroke(vg);
	}


	void drawFeedbackType(NVGcontext *vg, Vec pos, int feedbackType) {
		nvgFontSize(vg, 12);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->feedbackTypeNames[feedbackType]);
		nvgText(vg, pos.x, pos.y, text, NULL);
	}

	void draw(NVGcontext *vg) override {
		
		//drawProgress(vg,module->oscillator.progress());
		drawDivision(vg, Vec(91,60), module->division);
		drawDelayTime(vg, Vec(350,65), module->baseDelay);
		drawPatternType(vg, Vec(65,135), module->combPattern);
		drawEnvelope(vg,Vec(56,166),module->edgeLevel,module->tentLevel,module->tentTap);
		drawFeedbackType(vg, Vec(60,303), module->feedbackType);
	}
};


struct HairPickWidget : ModuleWidget {
	HairPickWidget(HairPick *module);
};

HairPickWidget::HairPickWidget(HairPick *module) : ModuleWidget(module) {
	box.size = Vec(RACK_GRID_WIDTH*14, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/HairPick.svg")));
		addChild(panel);
	}


	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 745)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 745)));

	{
		HPStatusDisplay *display = new HPStatusDisplay();
		display->module = module;
		display->box.pos = Vec(0, 0);
		display->box.size = Vec(box.size.x, 200);
		addChild(display);
	}

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(45, 33), module, HairPick::CLOCK_DIV_PARAM, 0, DIVISIONS-1, 0));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 33), module, HairPick::SIZE_PARAM, 0.001f, 10.0f, 0.350f));
	//addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(257, 40), module, HairPick::GRID_PARAM, 0.001f, 10.0f, 0.350f));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(17, 115), module, HairPick::PATTERN_TYPE_PARAM, 0.0f, 15.0f, 0.0f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 115), module, HairPick::NUMBER_TAPS_PARAM, 0.0f, 15.0f, 0.0f));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(17, 200), module, HairPick::EDGE_LEVEL_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(87, 200), module, HairPick::TENT_LEVEL_PARAM, 0.0f, 1.0f, 0.5f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 200), module, HairPick::TENT_TAP_PARAM, 0.0f, 63.0f, 32.0f));


	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(17, 283), module, HairPick::FEEDBACK_TYPE_PARAM, 0.0f, 3, 0.0f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 283), module, HairPick::FEEDBACK_AMOUNT_PARAM, 0.0f, 1.0f, 0.0f));

//	addChild(ModuleLightWidget::create<MediumLight<BlueLight>>(Vec(474, 189), module, HairPick::PING_PONG_LIGHT));


	

	addInput(Port::create<PJ301MPort>(Vec(12, 32), Port::INPUT, module, HairPick::CLOCK_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(12, 74), Port::INPUT, module, HairPick::VOLT_OCTAVE_INPUT));

	addInput(Port::create<PJ301MPort>(Vec(50, 74), Port::INPUT, module, HairPick::CLOCK_DIVISION_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(162, 74), Port::INPUT, module, HairPick::SIZE_CV_INPUT));
	//addInput(Port::create<PJ301MPort>(Vec(300, 45), Port::INPUT, module, HairPick::GRID_CV_INPUT));

	addInput(Port::create<PJ301MPort>(Vec(22, 156), Port::INPUT, module, HairPick::PATTERN_TYPE_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(162, 156), Port::INPUT, module, HairPick::NUMBER_TAPS_CV_INPUT));

	addInput(Port::create<PJ301MPort>(Vec(22, 241), Port::INPUT, module, HairPick::EDGE_LEVEL_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(92, 241), Port::INPUT, module, HairPick::TENT_LEVEL_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(162, 241), Port::INPUT, module, HairPick::TENT_TAP_CV_INPUT));


	addInput(Port::create<PJ301MPort>(Vec(22, 324), Port::INPUT, module, HairPick::FEEDBACK_TYPE_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(162, 324), Port::INPUT, module, HairPick::FEEDBACK_CV_INPUT));


	addInput(Port::create<PJ301MPort>(Vec(50, 336), Port::INPUT, module, HairPick::IN_L_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(75, 336), Port::INPUT, module, HairPick::IN_R_INPUT));
	addOutput(Port::create<PJ301MPort>(Vec(110, 336), Port::OUTPUT, module, HairPick::OUT_L_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(135, 336), Port::OUTPUT, module, HairPick::OUT_R_OUTPUT));
}


Model *modelHairPick = Model::create<HairPick, HairPickWidget>("Frozen Wasteland", "HairPick", "Hair Pick", FILTER_TAG);
