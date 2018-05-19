#include "FrozenWasteland.hpp"
#include "dsp/samplerate.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "ringbuffer.hpp"
#include "StateVariableFilter.h"
#include "clouds/dsp/fx/pitch_shifter.h"

#define HISTORY_SIZE (1<<21)
#define MAX_GRAIN_SIZE (1<<10)
#define NUM_TAPS 16
#define MAX_GRAINS 4
#define CHANNELS 2
#define DIVISIONS 21
#define NUM_GROOVES 16


struct PortlandWeather : Module {
	typedef float T;

	enum ParamIds {
		CLOCK_DIV_PARAM,
		TIME_PARAM,
		GRID_PARAM,
		GROOVE_TYPE_PARAM,
		GROOVE_AMOUNT_PARAM,
		FEEDBACK_PARAM,
		FEEDBACK_TAP_L_PARAM,
		FEEDBACK_TAP_R_PARAM,
		FEEDBACK_TONE_PARAM,
		PING_PONG_PARAM,
		MIX_PARAM,
		TAP_MUTE_PARAM,
		TAP_STACKED_PARAM = TAP_MUTE_PARAM+NUM_TAPS,
		TAP_MIX_PARAM = TAP_STACKED_PARAM+NUM_TAPS,
		TAP_PAN_PARAM = TAP_MIX_PARAM+NUM_TAPS,
		TAP_FILTER_TYPE_PARAM = TAP_PAN_PARAM+NUM_TAPS,
		TAP_FC_PARAM = TAP_FILTER_TYPE_PARAM+NUM_TAPS,
		TAP_Q_PARAM = TAP_FC_PARAM+NUM_TAPS,
		TAP_PITCH_SHIFT_PARAM = TAP_Q_PARAM+NUM_TAPS,
		TAP_DETUNE_PARAM = TAP_PITCH_SHIFT_PARAM+NUM_TAPS,
		NUM_PARAMS = TAP_DETUNE_PARAM+NUM_TAPS
	};
	enum InputIds {
		CLOCK_INPUT,
		CLOCK_DIVISION_CV_INPUT,
		TIME_CV_INPUT,
		GRID_CV_INPUT,
		GROOVE_TYPE_CV_INPUT,
		GROOVE_AMOUNT_CV_INPUT,
		FEEDBACK_INPUT,
		FEEDBACK_TAP_L_INPUT,
		FEEDBACK_TAP_R_INPUT,
		FEEDBACK_TONE_INPUT,
		MIX_INPUT,
		TAP_MIX_CV_INPUT,
		TAP_PAN_CV_INPUT = TAP_MIX_CV_INPUT + NUM_TAPS,
		TAP_FC_CV_INPUT = TAP_PAN_CV_INPUT + NUM_TAPS,
		TAP_Q_CV_INPUT = TAP_FC_CV_INPUT + NUM_TAPS,
		TAP_PITCH_SHIFT_CV_INPUT = TAP_Q_CV_INPUT + NUM_TAPS,
		TAP_DETUNE_CV_INPUT = TAP_PITCH_SHIFT_CV_INPUT + NUM_TAPS,
		IN_L_INPUT = TAP_DETUNE_CV_INPUT+NUM_TAPS,
		IN_R_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PING_PONG_LIGHT,
		TAP_MUTED_LIGHT,
		TAP_STACKED_LIGHT = TAP_MUTED_LIGHT+NUM_TAPS,
		FREQ_LIGHT = TAP_STACKED_LIGHT+NUM_TAPS,
		NUM_LIGHTS
	};
	enum FilterModes {
		FILTER_NONE,
		FILTER_LOWPASS,
		FILTER_HIGHPASS,
		FILTER_BANDPASS,
		FILTER_NOTCH
	};

	const char* grooveNames[NUM_GROOVES] = {"Straight","Swing","Hard Swing","Reverse Swing","Alternate Swing","Accellerando","Ritardando","Waltz Time","Half Swing","Roller Coaster","Quintuple","Random 1","Random 2","Random 3","Early Reflection","Late Reflection"};
	const float tapGroovePatterns[NUM_GROOVES][NUM_TAPS] = {
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0f}, // Straight time
		{1.25,2.0,3.25,4.0,5.25,6.0,7.25,8.0,9.25,10.0,11.25,12.0,13.25,14.0,15.25,16.0}, // Swing
		{1.75,2.0,3.75,4.0,5.75,6.0,7.75,8.0,9.75,10.0,11.75,12.0,13.75,14.0,15.75,16.0}, // Hard Swing
		{0.75,2.0,2.75,4.0,4.75,6.0,6.75,8.0,8.75,10.0,10.75,12.0,12.75,14.0,14.75,16.0}, // Reverse Swing
		{1.25,2.0,3.0,4.0,5.25,6.0,7.0,8.0,9.25,10.0,11.0,12.0,13.25,14.0,15.0,16.0}, // Alternate Swing
		{3.0,5.0,7.0,9.0,10.0,11.0,12.0,13.0,13.5,14.0,14.5,15.0,15.25,15.5,15.75,16.0}, // Accellerando
		{0.25,0.5,0.75,1.0,1.5,2.0,2.5,3.0,4.0,5.0,6.0,7.0,9.0,11.0,13.0,16.0}, // Ritardando
		{1.25,2.75,3.25,4.0,5.25,6.75,7.25,8.0,9.25,10.75,11.25,12.0,13.25,14.75,15.25,16.0}, // Waltz Time
		{1.5,2.0,3.5,4.0,5.0,6.0,7.0,8.0,9.5,10.0,11.5,12.0,13.0,14.0,15.0,16.0}, // Half Swing
		{1.0,2.0,4.0,5.0,6.0,8.0,10.0,12.0,12.5,13.0,13.5,14.0,14.5,15.0,15.5,16.0}, // Roller Coaster
		{1.75,2.5,3.25,4.0,4.75,6.5,7.25,8.0,9.75,10.5,11.25,12.0,12.75,14.5,15.25,16.0}, // Quintuple
		{0.25,0.75,1.0,1.25,4.0,5.5,7.25,7.5,8.0,8.25,10.0,11.0,13.5,15.0,15.75,16.0}, // Uniform Random 1
		{0.25,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0}, // Uniform Random 2
		{1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0}, // Uniform Random 3
		{0.25,0.5,1.0,1.25,1.75,2.0,2.5,3.5,4.25,4.5,4.75,5,6.25,8.25,11.0,16.0}, // Early Reflection
		{7.0,7.25,9.0,9.25,10.25,12.5,13.0,13.75,14.0,15.0,15.25,15.5,15.75,16.0,16.0,16.0} // Late Reflection
	};

	const float minCutoff = 15.0;
	const float maxCutoff = 8400.0;

	int tapGroovePattern = 9;
	float grooveAmount = 1.0f;

	bool pingPong = false;
	bool tapMuted[NUM_TAPS+1];
	bool tapStacked[NUM_TAPS+1];
	int lastFilterType[NUM_TAPS+1];
	float lastTapFc[NUM_TAPS+1];
	float lastTapQ[NUM_TAPS+1];
	float tapPitchShift[NUM_TAPS+1];
	float tapDetune[NUM_TAPS+1];
	int tapFilterType[NUM_TAPS+1];
	int feedbackTap = NUM_TAPS-1;

	StateVariableFilterState<T> filterStates[NUM_TAPS][CHANNELS];
    StateVariableFilterParams<T> filterParams[NUM_TAPS];
	RCFilter lowpassFilter[CHANNELS];
	RCFilter highpassFilter[CHANNELS];


	//clouds::PitchShifter pitch_shifter_;
	SchmittTrigger clockTrigger,pingPongTrigger,mutingTrigger[NUM_TAPS],stackingTrigger[NUM_TAPS];
	float divisions[DIVISIONS] = {1/256.0,1/192.0,1/128.0,1/96.0,1/64.0,1/48.0,1/32.0,1/24.0,1/16.0,1/13.0,1/12.0,1/11.0,1/8.0,1/7.0,1/6.0,1/5.0,1/4.0,1/3.0,1/2.0,1/1.5,1};
	const char* divisionNames[DIVISIONS] = {"/256","/192","/128","/96","/64","/48","/32","/24","/16","/13","/12","/11","/8","/7","/6","/5","/4","/3","/2","/1.5","x 1"};
	int division;
	float time = 0.0;
	float duration = 0;
	bool secondClockReceived = false;


	MultiTapDoubleRingBuffer<float, HISTORY_SIZE,NUM_TAPS> historyBuffer[CHANNELS];
	DoubleRingBuffer<float,MAX_GRAIN_SIZE> pitchShiftBuffer[MAX_GRAINS][NUM_TAPS+1][CHANNELS];
	DoubleRingBuffer<float, 16> outBuffer[NUM_TAPS][CHANNELS];
	SampleRateConverter<1> src;
	float lastFeedback[CHANNELS] = {0.0f,0.0f};

	float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	PortlandWeather() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int i = 0; i <= NUM_TAPS; ++i) {
			tapMuted[i] = false;
			tapStacked[i] = false;
			filterParams[i].setMode(StateVariableFilterParams<T>::Mode::LowPass);
			filterParams[i].setQ(5); 	
	        filterParams[i].setFreq(T(800.0f / engineGetSampleRate()));
	    }
	}

/*
	json_t *toJson() override {
		json_t *rootJ = json_object();
		for(int i=0;i<NUM_TAPS;i++) {
			//json_object_set_new(rootJ, "muted"+(char)(i+65), json_integer((bool) tapMuted[i]));
			//json_object_set_new(rootJ, "stacked"+(char)(i+65), json_integer((bool) tapStacked[i]));
			char* keyName = strcat("muted",(char)(i+65));
			//keyName+=(char)(i+64);
			printf("json key = %s\n", keyName );
		}
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		for(int i=0;i<NUM_TAPS;i++) {
			json_t *sumJ = json_object_get(rootJ, "muted"+(char)(i+65));
			if (sumJ) {
				tapMuted[i] = json_integer_value(sumJ);			
			}
		}
		for(int i=0;i<NUM_TAPS;i++) {
			json_t *sumJ = json_object_get(rootJ, "stacked"+(char)(i+65));
			if (sumJ) {
				tapStacked[i] = json_integer_value(sumJ);
			}
		}
	}
*/

	void step() override;
};


void PortlandWeather::step() {

	tapGroovePattern = (int)clamp(params[GROOVE_TYPE_PARAM].value + (inputs[GROOVE_TYPE_CV_INPUT].value / 10.0f),0.0f,15.0);
	grooveAmount = clamp(params[GROOVE_AMOUNT_PARAM].value + (inputs[GROOVE_AMOUNT_CV_INPUT].value / 10.0f),0.0f,1.0f);

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

	if (pingPongTrigger.process(params[PING_PONG_PARAM].value)) {
		pingPong = !pingPong;
	}
	lights[PING_PONG_LIGHT].value = pingPong;

	for(int channel = 0;channel < CHANNELS;channel++) {
		// Get input to delay block
		float in = 0.0f;
		if(channel == 0) {
			in = inputs[IN_L_INPUT].value;
		} else {
			in = inputs[IN_R_INPUT].active ? in = inputs[IN_R_INPUT].value : in = inputs[IN_L_INPUT].value;			
		}
		feedbackTap = (int)clamp(params[FEEDBACK_TAP_L_PARAM+channel].value + (inputs[FEEDBACK_TAP_L_INPUT+channel].value / 10.0f),0.0f,15.0);
		float feedback = clamp(params[FEEDBACK_PARAM].value + inputs[FEEDBACK_INPUT].value / 10.0f, 0.0f, 1.0f);
		float dry = in + lastFeedback[channel] * feedback;

		// Push dry sample into history buffer
		if (!historyBuffer[channel].full(NUM_TAPS-1)) {
			historyBuffer[channel].push(dry);
		}

		float wet = 0.0f; // This is the mix of delays and input that is outputed
		float feedbackValue = 0.0f; // This is the output of a tap that gets sent back to input
		for(int tap = 0; tap < NUM_TAPS;tap++) { 

			// Stacking
			if (tap < NUM_TAPS -1 && stackingTrigger[tap].process(params[TAP_STACKED_PARAM+tap].value)) {
				tapStacked[tap] = !tapStacked[tap];
			}

			// Muting
			if (mutingTrigger[tap].process(params[TAP_MUTE_PARAM+tap].value)) {
				tapMuted[tap] = !tapMuted[tap];
			}


			int inFrames = min(historyBuffer[channel].size(tap), 16);
		

			//Normally the delay tap is the same as the tap itself, unless it is stacked, then it is its neighbor;
			int delayTap = tap;
			while(tapStacked[delayTap] && delayTap < NUM_TAPS) {
				delayTap++;			
			}
			// Compute delay time in seconds
			float delay;
			if(inputs[CLOCK_INPUT].active) {
				delay = duration / divisions[division];
			} else {
				//float delay = clamp(params[TIME_PARAM].value + inputs[TIME_INPUT].value, 0.001f, 10.0f);
				delay = clamp(params[TIME_PARAM].value, 0.001f, 10.0f);			
			}
			delay = delay * lerp(tapGroovePatterns[0][delayTap],tapGroovePatterns[tapGroovePattern][delayTap],grooveAmount); //Balance between straight time and groove
			float delayMod = 0.0f;
			if(inputs[TIME_CV_INPUT].active) { //The CV can change either clocked or set delay by 10MS
				delayMod = 0.001f * inputs[TIME_CV_INPUT].value; 
			}
			// Number of delay samples
			float index = (delay+delayMod) * engineGetSampleRate();


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
				float pan = 0.0f;
				if(channel == 0) {
					pan = clamp(1.0-(params[TAP_PAN_PARAM+tap].value + (inputs[TAP_PAN_CV_INPUT+tap].value / 10.0f)),0.0f,0.5f) * 2.0f;
				} else {
					pan = clamp(params[TAP_PAN_PARAM+tap].value + (inputs[TAP_PAN_CV_INPUT+tap].value / 10.0f),0.0f,0.5f) * 2.0f;
				}				
				wetTap = outBuffer[tap][channel].shift() * clamp(params[TAP_MIX_PARAM+tap].value + (inputs[TAP_MIX_CV_INPUT+tap].value / 10.0f),0.0f,1.0f) * pan;
			}

			//float size = 1.0f; //Need to determine permanent value
			//float pitch = -12.0f; //Just for testing
			//Apply Pitch Shifting
		    //pitch_shifter_.set_ratio(SemitonesToRatio(pitch));
		    //pitch_shifter_.set_size(size);
		    //pitch_shifter_.Process(out_, 32); //32 came from clouds - it is another "size"

			int tapFilterType = (int)params[TAP_FILTER_TYPE_PARAM+tap].value;
			// Apply Filter to tap wet output			
			if(tapFilterType != FILTER_NONE) {
				if(tapFilterType != lastFilterType[tap]) {
					switch(tapFilterType) {
						case FILTER_LOWPASS:
						filterParams[tap].setMode(StateVariableFilterParams<T>::Mode::LowPass);
						break;
						case FILTER_HIGHPASS:
						filterParams[tap].setMode(StateVariableFilterParams<T>::Mode::HiPass);
						break;
						case FILTER_BANDPASS:
						filterParams[tap].setMode(StateVariableFilterParams<T>::Mode::BandPass);
						break;
						case FILTER_NOTCH:
						filterParams[tap].setMode(StateVariableFilterParams<T>::Mode::Notch);
						break;
					}

				}
				
				float cutoffExp = clamp(params[TAP_FC_PARAM+tap].value + inputs[TAP_FC_CV_INPUT+tap].value / 10.0f,0.0f,1.0f); 
				float tapFc = minCutoff * powf(maxCutoff / minCutoff, cutoffExp) / engineGetSampleRate();
				if(lastTapFc[tap] != tapFc) {
					filterParams[tap].setFreq(T(tapFc));
					lastTapFc[tap] = tapFc;
				}
				float tapQ = clamp(params[TAP_Q_PARAM+tap].value + (inputs[TAP_Q_CV_INPUT+tap].value / 10.0f),0.01f,1.0f) * 50; 
				if(lastTapQ[tap] != tapQ) {
					filterParams[tap].setQ(tapQ); 
					lastTapQ[tap] = tapQ;
				}
				wetTap = StateVariableFilter<T>::run(wetTap, filterStates[tap][channel], filterParams[tap]);
			}

			//If tap is muted should it contribute to feedback? Thinking yes.
			if(tap == feedbackTap) {
				feedbackValue = wetTap;
			}

			if(tapMuted[tap]) {
				wetTap = 0.0f;
			}

			wet += wetTap;

			lights[TAP_STACKED_LIGHT+tap].value = tapStacked[tap];
			lights[TAP_MUTED_LIGHT+tap].value = (tapMuted[tap]);
		}
			

		//Apply global filtering
		// TODO Make it sound better
		float color = clamp(params[FEEDBACK_TONE_PARAM].value + inputs[FEEDBACK_TONE_INPUT].value / 10.0f, 0.0f, 1.0f);
		float lowpassFreq = 10000.0f * powf(10.0f, clamp(2.0f*color, 0.0f, 1.0f));
		lowpassFilter[channel].setCutoff(lowpassFreq / engineGetSampleRate());
		lowpassFilter[channel].process(feedbackValue);
		feedbackValue = lowpassFilter[channel].lowpass();
		float highpassFreq = 10.0f * powf(100.0f, clamp(2.0f*color - 1.0f, 0.0f, 1.0f));
		highpassFilter[channel].setCutoff(highpassFreq / engineGetSampleRate());
		highpassFilter[channel].process(feedbackValue);
		feedbackValue = highpassFilter[channel].highpass();

		int feedbackChannel = channel;
		if (pingPong) {
			feedbackChannel = 1 - feedbackChannel;
		}
		lastFeedback[feedbackChannel] = feedbackValue;

		float mix = clamp(params[MIX_PARAM].value + inputs[MIX_INPUT].value / 10.0f, 0.0f, 1.0f);
		float out = crossfade(in, wet, mix);
		
		outputs[OUT_L_OUTPUT + channel].value = out;
	}
}


struct PortlandWeatherWidget : ModuleWidget {
	PortlandWeatherWidget(PortlandWeather *module);
};

PortlandWeatherWidget::PortlandWeatherWidget(PortlandWeather *module) : ModuleWidget(module) {
	box.size = Vec(RACK_GRID_WIDTH*56, RACK_GRID_HEIGHT * 2);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/PortlandWeather.svg")));
		addChild(panel);
	}


	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 745)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 745)));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(57, 40), module, PortlandWeather::CLOCK_DIV_PARAM, 0, DIVISIONS-1, 0));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 40), module, PortlandWeather::TIME_PARAM, 0.001f, 1.0f, 0.350f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(257, 40), module, PortlandWeather::GRID_PARAM, 0.001f, 10.0f, 0.350f));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(57, 110), module, PortlandWeather::GROOVE_TYPE_PARAM, 0.0f, 15.0f, 0.0f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 110), module, PortlandWeather::GROOVE_AMOUNT_PARAM, 0.0f, 1.0f, 1.0f));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(57, 180), module, PortlandWeather::FEEDBACK_PARAM, 0.0f, 1.0f, 0.0f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(157, 180), module, PortlandWeather::FEEDBACK_TAP_L_PARAM, 0.0f, 15.0f, 15.0f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(257, 180), module, PortlandWeather::FEEDBACK_TAP_R_PARAM, 0.0f, 15.0f, 15.0f));
	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(357, 180), module, PortlandWeather::FEEDBACK_TONE_PARAM, 0.0f, 5, 0.0f));
	addParam( ParamWidget::create<LEDButton>(Vec(470,185), module, PortlandWeather::PING_PONG_PARAM, 0.0f, 1.0f, 0.0f));

	addChild(ModuleLightWidget::create<MediumLight<BlueLight>>(Vec(474, 189), module, PortlandWeather::PING_PONG_LIGHT));

	addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(220, 705), module, PortlandWeather::MIX_PARAM, 0.0f, 1.0f, 0.5f));

	//last tap isn't stacked
	for (int i = 0; i< NUM_TAPS-1; i++) {
		addParam( ParamWidget::create<LEDButton>(Vec(54 + 50*i,240), module, PortlandWeather::TAP_STACKED_PARAM + i, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<BlueLight>>(Vec(58 + 50*i, 244), module, PortlandWeather::TAP_STACKED_LIGHT+i));
	}

	for (int i = 0; i < NUM_TAPS; i++) {
		addParam( ParamWidget::create<LEDButton>(Vec(54 + 50*i,260), module, PortlandWeather::TAP_MUTE_PARAM + i, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(58 + 50*i, 264), module, PortlandWeather::TAP_MUTED_LIGHT+i));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 280), module, PortlandWeather::TAP_MIX_PARAM + i, 0.0f, 1.0f, 1.0f));
		addInput(Port::create<PJ301MPort>(Vec(51+ 50*i, 311), Port::INPUT, module, PortlandWeather::TAP_MIX_CV_INPUT+i));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 340), module, PortlandWeather::TAP_PAN_PARAM + i, 0.0f, 1.0f, 0.5f));
		addInput(Port::create<PJ301MPort>(Vec(51 + 50*i, 371), Port::INPUT, module, PortlandWeather::TAP_PAN_CV_INPUT+i));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 400), module, PortlandWeather::TAP_FILTER_TYPE_PARAM + i, 0, 5, 0));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 440), module, PortlandWeather::TAP_FC_PARAM + i, 0.0f, 1.0f, 0.5f));
		addInput(Port::create<PJ301MPort>(Vec(51 + 50*i, 471), Port::INPUT, module, PortlandWeather::TAP_FC_CV_INPUT+i));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 500), module, PortlandWeather::TAP_Q_PARAM + i, 0.01f, 1.0f, 0.5f));
		addInput(Port::create<PJ301MPort>(Vec(51 + 50*i, 531), Port::INPUT, module, PortlandWeather::TAP_Q_CV_INPUT+i));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 560), module, PortlandWeather::TAP_PITCH_SHIFT_PARAM + i, -24.0f, 24.0f, 0.0f));
		addInput(Port::create<PJ301MPort>(Vec(51 + 50*i, 591), Port::INPUT, module, PortlandWeather::TAP_PITCH_SHIFT_CV_INPUT+i));
		addParam( ParamWidget::create<RoundBlackKnob>(Vec(48 + 50*i, 620), module, PortlandWeather::TAP_DETUNE_PARAM + i, -100.0f, 100.0f, 0.0f));
		addInput(Port::create<PJ301MPort>(Vec(51 + 50*i, 651), Port::INPUT, module, PortlandWeather::TAP_DETUNE_CV_INPUT+i));
	}

	addInput(Port::create<PJ301MPort>(Vec(25, 50), Port::INPUT, module, PortlandWeather::CLOCK_INPUT));

	addInput(Port::create<PJ301MPort>(Vec(100, 45), Port::INPUT, module, PortlandWeather::CLOCK_DIVISION_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(200, 45), Port::INPUT, module, PortlandWeather::TIME_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(300, 45), Port::INPUT, module, PortlandWeather::GRID_CV_INPUT));

	addInput(Port::create<PJ301MPort>(Vec(100, 115), Port::INPUT, module, PortlandWeather::GROOVE_TYPE_CV_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(200, 115), Port::INPUT, module, PortlandWeather::GROOVE_AMOUNT_CV_INPUT));


	addInput(Port::create<PJ301MPort>(Vec(100, 185), Port::INPUT, module, PortlandWeather::FEEDBACK_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(200, 185), Port::INPUT, module, PortlandWeather::FEEDBACK_TAP_L_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(300, 185), Port::INPUT, module, PortlandWeather::FEEDBACK_TAP_R_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(400, 185), Port::INPUT, module, PortlandWeather::FEEDBACK_TONE_INPUT));

	addInput(Port::create<PJ301MPort>(Vec(260, 710), Port::INPUT, module, PortlandWeather::MIX_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(100, 710), Port::INPUT, module, PortlandWeather::IN_L_INPUT));
	addInput(Port::create<PJ301MPort>(Vec(130, 710), Port::INPUT, module, PortlandWeather::IN_R_INPUT));
	addOutput(Port::create<PJ301MPort>(Vec(400, 710), Port::OUTPUT, module, PortlandWeather::OUT_L_OUTPUT));
	addOutput(Port::create<PJ301MPort>(Vec(430, 710), Port::OUTPUT, module, PortlandWeather::OUT_R_OUTPUT));
}


Model *modelPortlandWeather = Model::create<PortlandWeather, PortlandWeatherWidget>("Frozen Wasteland", "PortlandWeather", "Portland Weather", DELAY_TAG);
