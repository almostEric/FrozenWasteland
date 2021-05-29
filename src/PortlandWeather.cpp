#include "FrozenWasteland.hpp"
#include "dsp-delay/delayLine.cpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ringbuffer.hpp"
#include "StateVariableFilter.h"
#include "dsp-compressor/SimpleComp.h"
#include "dsp-compressor/SimpleGain.h"
#include <iostream>
#include <time.h>

#define HISTORY_SIZE (1<<24)
#define NUM_TAPS 16
#define MAX_GRAINS 4
#define CHANNELS 2
#define DIVISIONS 36
#define NUM_GROOVES 16
#define SMOOTHING 100 //# of samples

struct PortlandWeather : Module {
	
	enum ParamIds {
		CLOCK_DIV_PARAM,
		TIME_PARAM,
		GRID_PARAM,
		GROOVE_TYPE_PARAM,
		GROOVE_AMOUNT_PARAM,
		FEEDBACK_PARAM,
		FEEDBACK_TAP_L_PARAM,
		FEEDBACK_TAP_R_PARAM,
		FEEDBACK_L_SLIP_PARAM,
		FEEDBACK_R_SLIP_PARAM,
		FEEDBACK_TONE_PARAM,
		FEEDBACK_L_PITCH_SHIFT_PARAM,
		FEEDBACK_R_PITCH_SHIFT_PARAM,
		FEEDBACK_L_DETUNE_PARAM,
		FEEDBACK_R_DETUNE_PARAM,		
		PING_PONG_PARAM,
		REVERSE_PARAM,
		MIX_PARAM,
		TAP_MUTE_PARAM,
		TAP_STACKED_PARAM = TAP_MUTE_PARAM+NUM_TAPS,
		TAP_LEVEL_PARAM = TAP_STACKED_PARAM+NUM_TAPS,
		TAP_PAN_PARAM = TAP_LEVEL_PARAM+NUM_TAPS,
		TAP_FILTER_TYPE_PARAM = TAP_PAN_PARAM+NUM_TAPS,
		TAP_FC_PARAM = TAP_FILTER_TYPE_PARAM+NUM_TAPS,
		TAP_Q_PARAM = TAP_FC_PARAM+NUM_TAPS,
		TAP_PITCH_SHIFT_PARAM = TAP_Q_PARAM+NUM_TAPS,
		TAP_DETUNE_PARAM = TAP_PITCH_SHIFT_PARAM+NUM_TAPS,
		CLEAR_BUFFER_PARAM = TAP_DETUNE_PARAM+NUM_TAPS,
		REVERSE_TRIGGER_MODE_PARAM,
		PING_PONG_TRIGGER_MODE_PARAM,
		STACK_TRIGGER_MODE_PARAM,
		MUTE_TRIGGER_MODE_PARAM,
		COMPRESSION_MODE_PARAM,
		CLOCK_MULT_PARAM,
		DUCKING_AMOUNT_PARAM,
		DUCKING_FB_AMOUNT_PARAM,
		PHASE_REVERSE_L_PARAM,
		PHASE_REVERSE_R_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		CLOCK_DIVISION_CV_INPUT,
		TIME_CV_INPUT,
		EXTERNAL_DELAY_TIME_INPUT,
		GRID_CV_INPUT,
		GROOVE_TYPE_CV_INPUT,
		GROOVE_AMOUNT_CV_INPUT,
		FEEDBACK_INPUT,
		FEEDBACK_TAP_L_INPUT,
		FEEDBACK_TAP_R_INPUT,
		FEEDBACK_TONE_INPUT,
		FEEDBACK_L_SLIP_CV_INPUT,
		FEEDBACK_R_SLIP_CV_INPUT,
		FEEDBACK_L_PITCH_SHIFT_CV_INPUT,
		FEEDBACK_R_PITCH_SHIFT_CV_INPUT,
		FEEDBACK_L_DETUNE_CV_INPUT,
		FEEDBACK_R_DETUNE_CV_INPUT,
		FEEDBACK_L_RETURN,
		FEEDBACK_R_RETURN,
		PING_PONG_INPUT,
		REVERSE_INPUT,
		MIX_INPUT,
		TAP_MUTE_CV_INPUT,
		TAP_STACK_CV_INPUT = TAP_MUTE_CV_INPUT + NUM_TAPS,
		TAP_MIX_CV_INPUT = TAP_STACK_CV_INPUT + NUM_TAPS,
		TAP_PAN_CV_INPUT = TAP_MIX_CV_INPUT + NUM_TAPS,
		TAP_FC_CV_INPUT = TAP_PAN_CV_INPUT + NUM_TAPS,
		TAP_Q_CV_INPUT = TAP_FC_CV_INPUT + NUM_TAPS,
		TAP_PITCH_SHIFT_CV_INPUT = TAP_Q_CV_INPUT + NUM_TAPS,
		TAP_DETUNE_CV_INPUT = TAP_PITCH_SHIFT_CV_INPUT + NUM_TAPS,
		IN_L_INPUT = TAP_DETUNE_CV_INPUT+NUM_TAPS,
		IN_R_INPUT,
		CLOCK_MULT_CV_INPUT,
		DUCKING_AMOUNT_CV_INPUT,
		DUCKING_FB_AMOUNT_CV_INPUT,
		PHASE_REVERSE_L_INPUT,
		PHASE_REVERSE_R_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		FEEDBACK_L_OUTPUT,
		FEEDBACK_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PING_PONG_LIGHT,
		REVERSE_LIGHT,
		COMPRESSION_MODE_LIGHT,
		TAP_MUTED_LIGHT = COMPRESSION_MODE_LIGHT+3,
		TAP_STACKED_LIGHT = TAP_MUTED_LIGHT+NUM_TAPS,
		FREQ_LIGHT = TAP_STACKED_LIGHT+NUM_TAPS,	
		DUCKING_LIGHT,	
		PHASE_REVERSE_L_LIGHT,
		PHASE_REVERSE_R_LIGHT,
		NUM_LIGHTS
	};
	enum FilterModes {
		FILTER_NONE,
		FILTER_LOWPASS,
		FILTER_HIGHPASS,
		FILTER_BANDPASS,
		FILTER_NOTCH
	};
	enum TriggerModes {
		TRIGGER_TRIGGER_MODE,
		GATE_TRIGGE_MODE
	};
	enum CompressionModes {
		COMPRESSION_NONE,
		COMPRESSION_CLAMP,
		COMPRESSION_HARD,
		COMPRESSION_ADAPTIVE,
		NUM_COMPRESSION_MODES
	};
	enum ParameterGroups {
		STACKING_AND_MUTING_GROUP,
		LEVELS_AND_PANNING_GROUP,
		FILTERING_GROUP,
		PITCH_SHIFTING_GROUP,
		NUM_PARAMETER_GROUPS
	};

	// Expander
	float rightMessages[2][NUM_TAPS * 15] = {};// this module must read from here

	bool usingAlgorithm = false;
	
	chunkware_simple::SimpleComp compressor[CHANNELS + 1]; //2 = ducking

	uint8_t compressionMode = COMPRESSION_NONE;

	void changeCompressionMode (uint8_t newMode) {
		if (newMode == compressionMode) {
			return;
		}

		if (newMode == COMPRESSION_HARD) {
			// reset to hard compression
			for (uint8_t i = 0; i < CHANNELS; i++) {
				compressor[i].setRatio(20.0);
			}
		}

		// compressionMode = newMode;
	}

	
	inline float limit (float in, uint8_t which) {
		compressor[which].process(in);
		if (compressionMode == COMPRESSION_NONE) {
			return in;
		} else if (compressionMode == COMPRESSION_CLAMP) {
			return clamp(in, -6.0f, 6.0f);
		} else if (compressionMode == COMPRESSION_HARD) {
			float gainReduction = compressor[which].getGainReduction();
			double finalGainLin = chunkware_simple::dB2lin(-gainReduction);
			return in * finalGainLin;
		} else {
			// adaptive compression
			// float ratio = fabs(in) / 4.0f;
			float ratio = 5; //hack
			compressor[which].setRatio(ratio);
			float gainReduction = compressor[which].getGainReduction();
			double finalGainLin = chunkware_simple::dB2lin(-gainReduction);
			return in * finalGainLin;
		}
	}

	float sampleRate;

	const char* grooveNames[NUM_GROOVES+1] = {"Straight","Swing","Hard Swing","Reverse Swing","Alternate Swing","Accelerando","Ritardando","Waltz Time","Half Swing","Roller Coaster","Quintuple","Random 1","Random 2","Random 3","Early Reflection","Late Reflection"};
	const float tapGroovePatterns[NUM_GROOVES][NUM_TAPS] = {
		{1.0f,2.0f,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0f}, // Straight time
		{1.25,2.0,3.25,4.0,5.25,6.0,7.25,8.0,9.25,10.0,11.25,12.0,13.25,14.0,15.25,16.0}, // Swing
		{1.75,2.0,3.75,4.0,5.75,6.0,7.75,8.0,9.75,10.0,11.75,12.0,13.75,14.0,15.75,16.0}, // Hard Swing
		{0.75,2.0,2.75,4.0,4.75,6.0,6.75,8.0,8.75,10.0,10.75,12.0,12.75,14.0,14.75,16.0}, // Reverse Swing
		{1.25,2.0,3.0,4.0,5.25,6.0,7.0,8.0,9.25,10.0,11.0,12.0,13.25,14.0,15.0,16.0}, // Alternate Swing
		{3.0,5.0,7.0,9.0,10.0,11.0,12.0,13.0,13.5,14.0,14.5,15.0,15.25,15.5,15.75,16.0}, // Accelerando
		{0.25,0.5,0.75,1.0,1.5,2.0,2.5,3.0,4.0,5.0,6.0,7.0,9.0,11.0,13.0,16.0}, // Ritardando
		{1.25,2.75,3.25,4.0,5.25,6.75,7.25,8.0,9.25,10.75,11.25,12.0,13.25,14.75,15.25,16.0}, // Waltz Time
		{1.5,2.0,3.5,4.0,5.0,6.0,7.0,8.0,9.5,10.0,11.5,12.0,13.0,14.0,15.0,16.0}, // Half Swing
		{1.0,2.0,4.0,5.0,6.0,8.0,10.0,12.0,12.5,13.0,13.5,14.0,14.5,15.0,15.5,16.0}, // Roller Coaster
		{1.75,2.5,3.25,4.0,4.75,6.5,7.25,8.0,9.75,10.5,11.25,12.0,12.75,14.5,15.25,16.0}, // Quintuple
		{0.25,0.75,1.0,1.25,4.0,5.5,7.25,7.5,8.0,8.25,10.0,11.0,13.5,15.0,15.75,16.0}, // Uniform Random 1
		{0.25,4.75,5.25,5.5,7.0,8.0,8.5,8.75,9.0,9.25,11.75,12.75,13.0,13.25,14.75,15.5}, // Uniform Random 2
		{0.75,2.0,2.25,5.75,7.25,7.5,7.75,8.5,8.75,12.5,12.75,13.0,13.75,14.0,14.5,16.0}, // Uniform Random 3
		{0.25,0.5,1.0,1.25,1.75,2.0,2.5,3.5,4.25,4.5,4.75,5,6.25,8.25,11.0,16.0}, // Early Reflection
		{7.0,7.25,9.0,9.25,10.25,12.5,13.0,13.75,14.0,15.0,15.25,15.5,15.75,16.0,16.0,16.0} // Late Reflection
	};

	const float minCutoff = 15.0;
	const float maxCutoff = 8400.0;

	int tapGroovePattern = 0;
	float grooveAmount = 1.0f;

	bool pingPong = false;
	bool reverse = false;
	bool phaseReverseL = false;
	bool phaseReverseR = false;
	int grainCount = 3; //NOTE Should be 3
	float grainSize = 0.5f; //Can be between 0 and 1			
	bool tapMuted[NUM_TAPS];
	bool lastTapMutedStatus[NUM_TAPS] = {0};
	float lastTapSmoothPosition[NUM_TAPS] = {0};
	bool tapStacked[NUM_TAPS];
	int lastFilterType[NUM_TAPS];
	float lastTapFc[NUM_TAPS];
	float lastTapQ[NUM_TAPS];
	float tapPitchShift[NUM_TAPS];
	float tapDetune[NUM_TAPS];
	int tapFilterType[NUM_TAPS];
	int feedbackTap[CHANNELS] = {NUM_TAPS-1,NUM_TAPS-1};
	float feedbackSlip[CHANNELS] = {0.0f,0.0f};
	float feedbackPitch[CHANNELS] = {0.0f,0.0f};
	float feedbackDetune[CHANNELS] = {0.0f,0.0f};
	float delayTime[NUM_TAPS+CHANNELS];
	float lastDelayTime[NUM_TAPS+CHANNELS];

	float *tapLConnections;// index into correct page of messages from expander (avoid having separate buffers)
	float *tapLReturns;// index into correct page of messages from expander (avoid having separate buffers)
	float *tapRConnections;// index into correct page of messages from expander (avoid having separate buffers)
	float *tapRReturns;// index into correct page of messages from expander (avoid having separate buffers)
	float tapLSends[NUM_TAPS] = {0};
	float tapRSends[NUM_TAPS] = {0};

	float expanderDelayTime[NUM_TAPS] = {0.0f};
	bool expanderMuteTaps[NUM_TAPS] = {false};
	
	float *expanderLevels;
	float *expanderPans;
	float *expanderFcs;
	float *expanderQs;
	float *expanderPitches;
	float *expanderDetunes;

	bool hasExpanderLevels = false;
	bool hasExpanderPans = false;
	bool hasExpanderFcs = false;
	bool hasExpanderQs = false;
	bool hasExpanderPitches = false;
	bool hasExpanderDetunes = false;


	//percentages
	float clockMultiplyPercentage = 0;
	float clockDividePercentage = 0;
	float timePercentage = 0;
	float grooveTypePercentage = 0;
	float grooveAmountPercentage = 0;
	float mixPercentage = 0;
	float duckingPercentage = 0;
	float feedbackPercentage = 0;
	float duckingFBPercentage = 0;
	float feedbackTonePercentage = 0;
	float feedbackTapPercentage[CHANNELS] = {0};
	float feedbackSlipPercentage[CHANNELS] = {0};
	float feedbackPitchShiftPercentage[CHANNELS] = {0};
	float feedbackDetunePercentage[CHANNELS] = {0};

	float levelPercentage[NUM_TAPS] = {0};
	float panPercentage[NUM_TAPS] = {0};
	float fcPercentage[NUM_TAPS] = {0};
	float qPercentage[NUM_TAPS] = {0};
	float pShiftPercentage[NUM_TAPS] = {0};
	float pDetunePercentage[NUM_TAPS] = {0};

	
	StateVariableFilterState<T> filterStates[NUM_TAPS][CHANNELS];
    StateVariableFilterParams<T> filterParams[NUM_TAPS];
	dsp::RCFilter lowpassFilter[CHANNELS];
	dsp::RCFilter highpassFilter[CHANNELS];
	float lastColor = 0.0f;

	const char* filterNames[5] = {"OFF","LP","HP","BP","NOTCH"};

	const char* tapNames[NUM_TAPS+2] {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","ALL","EXT"};
	

	
	dsp::SchmittTrigger clockTrigger,pingPongTrigger,reverseTrigger,clearBufferTrigger,compressionModeTrigger,phaseReverseLTrigger,phaseReverseRTrigger,mutingTrigger[NUM_TAPS],stackingTrigger[NUM_TAPS];
	//double divisions[DIVISIONS] = {1/256.0,1/192.0,1/128.0,1/96.0,1/64.0,1/48.0,1/32.0,1/24.0,1/16.0,1/13.0,1/12.0,1/11.0,1/8.0,1/7.0,1/6.0,1/5.0,1/4.0,1/3.0,1/2.0,1/1.5,1,1/1.5,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,16.0,24.0};
	//const char* divisionNames[DIVISIONS] = {"/256","/192","/128","/96","/64","/48","/32","/24","/16","/13","/12","/11","/8","/7","/6","/5","/4","/3","/2","/1.5","x 1","x 1.5","x 2","x 3","x 4","x 5","x 6","x 7","x 8","x 9","x 10","x 11","x 12","x 13","x 16","x 24"};
	double division = 1;
	double multiplier = 1;
	double timeElapsed = 0.0;
	double duration = 0;
	double baseDelay = 0.0;
	
	bool firstClockReceived = false;
	bool secondClockReceived = false;

	
	MultiTapDelayLine<FloatFrame, NUM_TAPS+CHANNELS> delayLine;
	FrozenWasteland::ReverseRingBuffer<float, HISTORY_SIZE> reverseHistoryBuffer[CHANNELS];
	GranularPitchShifter granularPitchShift[NUM_TAPS + CHANNELS]; // Each tap, plus each channel 
	
	FloatFrame lastFeedback = {0.0f,0.0f};

	float lerp(float v0, float v1, float t) {
	  return (1 - t) * v0 + t * v1;
	}

	float SemitonesToRatio(float semiTone) {
		return powf(2,semiTone/12.0f);
	}

	PortlandWeather() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CLOCK_DIV_PARAM, 1, 256, 1,"Clock Division");
		configParam(CLOCK_MULT_PARAM, 1, 256, 1,"Clock Multiply");
		configParam(TIME_PARAM, 0.0f, 10.0f, 0.350f,"Time"," ms",0,1000);
		

		configParam(GROOVE_TYPE_PARAM, 0.0f, 15.0f, 0.0f,"Groove Type");
		configParam(GROOVE_AMOUNT_PARAM, 0.0f, 1.0f, 1.0f,"Groove Amount","%",0,100);

		configParam(FEEDBACK_PARAM, 0.0f, 1.0f, 0.0f,"Feedback","%",0,100);
		configParam(FEEDBACK_TONE_PARAM, 0.0f, 1.0f, 0.5f,"Feedback Tone","%",0,100);

		configParam(FEEDBACK_TAP_L_PARAM, 0.0f, 17.0f, 15.0f,"Feedback L Tap");
		configParam(FEEDBACK_TAP_R_PARAM, 0.0f, 17.0f, 15.0f,"Feedback R Tap");
		configParam(FEEDBACK_L_SLIP_PARAM, -0.5f, 0.5f, 0.0f,"Feedback L Slip","%",0,100);
		configParam(FEEDBACK_R_SLIP_PARAM, -0.5f, 0.5f, 0.0f,"Feedback R Slip","%",0,100);
		configParam(FEEDBACK_L_PITCH_SHIFT_PARAM, -24.0f, 24.0f, 0.0f,"Feedback L Pitch Shift", " semitones");
		configParam(FEEDBACK_R_PITCH_SHIFT_PARAM, -24.0f, 24.0f, 0.0f,"Feedback R Pitch Shift", " semitones");
		configParam(FEEDBACK_L_DETUNE_PARAM, -99.0f, 99.0f, 0.0f,"Feedback L Detune", " cents");
		configParam(FEEDBACK_R_DETUNE_PARAM, -99.0f, 99.0f, 0.0f,"Feedback R Detune", " cents");

	
		configParam(CLEAR_BUFFER_PARAM, 0.0f, 1.0f, 0.0f,"Clear Buffer");

		configParam(REVERSE_PARAM, 0.0f, 1.0f, 0.0f);		
		configParam(PING_PONG_PARAM, 0.0f, 1.0f, 0.0f);

		configParam(COMPRESSION_MODE_PARAM, 0.0f, 1.0f, 0.0f);

		//last tap isn't stacked
		for (int i = 0; i< NUM_TAPS-1; i++) {
			configParam(TAP_STACKED_PARAM + i, 0.0f, 1.0f, 0.0f);
		}

		for (int i = 0; i < NUM_TAPS; i++) {
			configParam(TAP_MUTE_PARAM + i, 0.0f, 1.0f, 0.0f);		
			configParam(TAP_LEVEL_PARAM + i, 0.0f, 1.0f, 0.5f,"Tap " + std::to_string(i+1) + " mix","%",0,100);
			configParam(TAP_PAN_PARAM + i, -1.0f, 1.0f, 0.0f,"Tap " + std::to_string(i+1) + " pan","%",0,100);
			configParam(TAP_FILTER_TYPE_PARAM + i, 0, 4, 0,"Tap " + std::to_string(i+1) + " filter type");
			configParam(TAP_FC_PARAM + i, 0.0f, 1.0f, 0.5f,"Tap " + std::to_string(i+1) + " Fc"," Hz",560,15);
			configParam(TAP_Q_PARAM + i, 0.01f, 1.0f, 0.1f,"Tap " + std::to_string(i+1) + " Q","",0,100);
			configParam(TAP_PITCH_SHIFT_PARAM + i, -24.0f, 24.0f, 0.0f,"Tap " + std::to_string(i+1) + " pitch shift"," semitones");
			configParam(TAP_DETUNE_PARAM + i, -99.0f, 99.0f, 0.0f,"Tap " + std::to_string(i+1) + " detune"," cents");

		}

		configParam(MIX_PARAM, 0.0f, 1.0f, 0.5f,"Mix","%",0,100);
		configParam(DUCKING_AMOUNT_PARAM, 0.0f, 1.0f, 0.0f,"Ducking","%",0,100);

		configParam(REVERSE_TRIGGER_MODE_PARAM, 0.0f, 1.0f, 0.0f);
		configParam(PING_PONG_TRIGGER_MODE_PARAM, 0.0f, 1.0f, 0.0f);
		configParam(STACK_TRIGGER_MODE_PARAM, 0.0f, 1.0f, 0.0f);
		configParam(MUTE_TRIGGER_MODE_PARAM, 0.0f, 1.0f, 0.0f);


		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		sampleRate = APP->engine->getSampleRate();
		
		for (uint8_t i = 0; i < CHANNELS + 1; i++) {
			compressor[i].initRuntime();
			compressor[i].setRatio(20.0);
			compressor[i].setThresh(i < CHANNELS ? 45.0 : 5.0);
			compressor[i].setKnee(0.0);
			compressor[i].setAttack(1.0);
			compressor[i].setRelease(30.0);
			compressor[i].setAttackCurve(0.0);
			compressor[i].setReleaseCurve(0.0);
		}


		for (int i = 0; i < NUM_TAPS; ++i) {
			tapMuted[i] = false;
			tapStacked[i] = false;
			tapPitchShift[i] = 0.0f;
			tapDetune[i] = 0.0f;
			lastFilterType[i] = FILTER_NONE;
			lastTapFc[i] = 800.0f / sampleRate;
			lastTapQ[i] = 5.0f;
			filterParams[i].setMode(StateVariableFilterParams<T>::Mode::LowPass);
			filterParams[i].setQ(5); 	
	        filterParams[i].setFreq(T(800.0f / sampleRate));
			delayTime[i] = 0.0f;

	    }	


		srand(time(NULL));
	}



	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "pingPong", json_integer((int) pingPong));
		json_object_set_new(rootJ, "reverse", json_integer((int) reverse));
		json_object_set_new(rootJ, "phaseReverseL", json_boolean(phaseReverseL));
		json_object_set_new(rootJ, "phaseReverseR", json_boolean(phaseReverseR));

		json_object_set_new(rootJ, "compressionMode", json_integer((int) compressionMode));

		json_object_set_new(rootJ, "grainSize", json_real((float) grainSize));

		for(int i=0;i<NUM_TAPS;i++) {
			//This is so stupid!!! why did he not use strings?
			char buf[100];
			strcpy(buf, "muted");
			strcat(buf, tapNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) tapMuted[i]));

			strcpy(buf, "stacked");
			strcat(buf, tapNames[i]);
			json_object_set_new(rootJ, buf, json_integer((int) tapStacked[i]));
		}
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		json_t *sumJ = json_object_get(rootJ, "pingPong");
		if (sumJ) {
			pingPong = json_integer_value(sumJ);			
		}

		json_t *sumR = json_object_get(rootJ, "reverse");
		if (sumR) {
			reverse = json_integer_value(sumR);			
		}

		json_t *sumPL = json_object_get(rootJ, "phaseReverseL");
		if (sumPL) {
			phaseReverseL = json_boolean_value(sumPL);			
		}

		json_t *sumPR = json_object_get(rootJ, "phaseReverseR");
		if (sumPR) {
			phaseReverseR = json_boolean_value(sumPR);			
		}

		json_t *sumCM = json_object_get(rootJ, "compressionMode");
		if (sumCM) {
			compressionMode = json_integer_value(sumCM);
			changeCompressionMode(compressionMode);
		}

		json_t *sumGs = json_object_get(rootJ, "grainSize");
		if (sumGs) {
			grainSize = json_real_value(sumGs);			
		}
		
		char buf[100];			
		for(int i=0;i<NUM_TAPS;i++) {
			strcpy(buf, "muted");
			strcat(buf, tapNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				tapMuted[i] = json_integer_value(sumJ);			
			}
		}

		for(int i=0;i<NUM_TAPS;i++) {
			strcpy(buf, "stacked");
			strcat(buf, tapNames[i]);

			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				tapStacked[i] = json_integer_value(sumJ);
			}
		}
		
	}


	void process(const ProcessArgs &args) override {

		if (clearBufferTrigger.process(params[CLEAR_BUFFER_PARAM].getValue())) {
			delayLine.clear();
		}
 
		tapGroovePattern = (int)clamp(params[GROOVE_TYPE_PARAM].getValue() + (inputs[GROOVE_TYPE_CV_INPUT].isConnected() ?  inputs[GROOVE_TYPE_CV_INPUT].getVoltage() / 10.0f : 0.0f),0.0f,15.0);
		grooveTypePercentage = tapGroovePattern / 15.0;
		grooveAmount = clamp(params[GROOVE_AMOUNT_PARAM].getValue() + (inputs[GROOVE_AMOUNT_CV_INPUT].isConnected() ? inputs[GROOVE_AMOUNT_CV_INPUT].getVoltage() / 10.0f : 0.0f),0.0f,1.0f);
		grooveAmountPercentage = grooveAmount;

		division = params[CLOCK_DIV_PARAM].getValue();
		if(inputs[CLOCK_DIVISION_CV_INPUT].isConnected()) {
			division +=(inputs[CLOCK_DIVISION_CV_INPUT].getVoltage() * 25.6);
		}
		division = clamp(std::floor(division),1.0f,256.0f);
		clockDividePercentage = division / 256.0;

		multiplier = params[CLOCK_MULT_PARAM].getValue();
		if(inputs[CLOCK_MULT_CV_INPUT].isConnected()) {
			multiplier +=(inputs[CLOCK_MULT_CV_INPUT].getVoltage() * 25.6);
		}
		multiplier = clamp(std::floor(multiplier),1.0f,256.0f);
		clockMultiplyPercentage = multiplier / 256.0;

		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPWTapBreakoutExpander || rightExpander.module->model == modelPWAlgorithmicExpander || rightExpander.module->model == modelPWGridControlExpander));
		bool algorithmExpanderPresent = false;
		bool tapBreakoutPresent = false;	
		bool hasExpanderLevels = false;
		bool hasExpanderPans = false;
		bool hasExpanderFcs = false;
		bool hasExpanderQs = false;
		bool hasExpanderPitches = false;
		bool hasExpanderDetunes = false;
		if(rightExpanderPresent) {
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			tapLConnections = &messagesFromExpander[NUM_TAPS * 3]; // contains 8 values of the returns from the aux panel
			tapLReturns = &messagesFromExpander[NUM_TAPS * 4]; // contains 8 values of the returns from the aux panel
			tapRConnections = &messagesFromExpander[NUM_TAPS * 5]; // contains 8 values of the returns from the aux panel
			tapRReturns = &messagesFromExpander[NUM_TAPS * 6]; // contains 8 values of the returns from the aux panel

			tapBreakoutPresent = (bool)messagesFromExpander[2];
			algorithmExpanderPresent = (bool)messagesFromExpander[3];

			uint8_t nbrGridControlExpanders = messagesFromExpander[4];
			//fprintf(stderr, "%hu %i %i \n", nbrGridControlExpanders,tapBreakoutPresent,algorithmExpanderPresent);
			for(int i=0;i<nbrGridControlExpanders;i++) {
				uint8_t paramDestination = messagesFromExpander[5+i];
				switch(paramDestination) {
					case 0 :
						//Nothing connected
						break;
					case 1 :
						expanderLevels = &messagesFromExpander[NUM_TAPS * (8+i)];
						hasExpanderLevels = true;
						break;
					case 2 :
						expanderPans = &messagesFromExpander[NUM_TAPS * (8+i)];
						hasExpanderPans = true;
						break;
					case 3 :
						expanderFcs = &messagesFromExpander[NUM_TAPS * (8+i)];
						hasExpanderFcs = true;
						break;
					case 4 :
						expanderQs = &messagesFromExpander[NUM_TAPS * (8+i)];
						hasExpanderQs = true;
						break;
					case 5 :
						expanderPitches = &messagesFromExpander[NUM_TAPS * (8+i)];
						hasExpanderPitches = true;
						break;
					case 6 :
						expanderDetunes = &messagesFromExpander[NUM_TAPS * (8+i)];
						hasExpanderDetunes = true;
						break;
				}
			}
		}
		if(algorithmExpanderPresent)
		{			
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			usingAlgorithm = true;
			//Clock for algorithn
			messageToExpander[0] = inputs[CLOCK_INPUT].getVoltage();
			messageToExpander[1] = division;

			//Get Delay Times
			for(int tap=0;tap<NUM_TAPS;tap++) {

				// fprintf(stderr, "Tap:%i delay:%f \n",tap,expanderDelayTime[tap]);
				expanderDelayTime[tap] = messagesFromExpander[NUM_TAPS * 7 + tap];
				expanderMuteTaps[tap] = (expanderDelayTime[tap] < 0);
			}

		} else { // make more sophisticated
			usingAlgorithm = false;
			for(int tap=0;tap<NUM_TAPS;tap++) {
				expanderDelayTime[tap] = 0;
				expanderMuteTaps[tap] = false;
			}
		}


		timeElapsed += 1.0 / sampleRate;
		if(inputs[CLOCK_INPUT].isConnected()) {
			if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				if(firstClockReceived) {
					duration = timeElapsed;	
					secondClockReceived = true;
				}
				timeElapsed = 0;
				firstClockReceived = true;								
			} else if(secondClockReceived && timeElapsed > duration) {  //allow absense of second clock to affect duration
				//duration = timeElapsed;
				//duration = 1;
			}	
			//baseDelay = duration / division;
			baseDelay = clamp(duration / multiplier * division,0.001f,99.0f);
			// if(baseDelay > HISTORY_SIZE / sampleRate) {
			// 	baseDelay = HISTORY_SIZE / sampleRate - 1.0;
			// }
				
		} else {
			baseDelay = clamp(params[TIME_PARAM].getValue() + inputs[TIME_CV_INPUT].getVoltage(), 0.001f, 99.0f);	
			timePercentage = baseDelay / 10.0;
			duration = 0.0f;
			firstClockReceived = false;
			secondClockReceived = false;			
		}

		float delayMod = 0.0f;
		if(inputs[TIME_CV_INPUT].isConnected() && inputs[CLOCK_INPUT].isConnected()) { //The CV can change either clocked or set delay by 10MS
			delayMod = (0.001f * inputs[TIME_CV_INPUT].getVoltage()); 
		}


		// Ping Pong
		if(params[PING_PONG_TRIGGER_MODE_PARAM].getValue() == GATE_TRIGGE_MODE && inputs[PING_PONG_INPUT].isConnected()) {
			pingPong = inputs[PING_PONG_INPUT].getVoltage() > 0.0f;
		}
		//Button (or trigger) can override input	
		if (pingPongTrigger.process(params[PING_PONG_PARAM].getValue() + (inputs[PING_PONG_INPUT].isConnected() && params[PING_PONG_TRIGGER_MODE_PARAM].getValue() == TRIGGER_TRIGGER_MODE ? inputs[PING_PONG_INPUT].getVoltage() : 0))) {
			pingPong = !pingPong;
		}
		lights[PING_PONG_LIGHT].value = pingPong;

		// Reverse
		bool reversePrevious = reverse;
		if(params[REVERSE_TRIGGER_MODE_PARAM].getValue() == GATE_TRIGGE_MODE && inputs[REVERSE_INPUT].isConnected()) {
			reverse = inputs[REVERSE_INPUT].getVoltage() > 0.0f;
		}		
		if (reverseTrigger.process(params[REVERSE_PARAM].getValue() + (inputs[REVERSE_INPUT].isConnected() && params[REVERSE_TRIGGER_MODE_PARAM].getValue() == TRIGGER_TRIGGER_MODE ? inputs[REVERSE_INPUT].getVoltage() : 0))) {
			reverse = !reverse;
		}
		lights[REVERSE_LIGHT].value = reverse;
		if(reverse && reverse != reversePrevious) {
			reverseHistoryBuffer[0].clear();
			reverseHistoryBuffer[1].clear();		
		}

		// Phase Reverse L
		if (phaseReverseLTrigger.process(params[PHASE_REVERSE_L_PARAM].getValue())) {
			phaseReverseL = !phaseReverseL;
		}
		lights[PHASE_REVERSE_L_LIGHT].value = phaseReverseL;

		// Phase Reverse R
		if (phaseReverseRTrigger.process(params[PHASE_REVERSE_R_PARAM].getValue())) {
			phaseReverseR = !phaseReverseR;
		}
		lights[PHASE_REVERSE_R_LIGHT].value = phaseReverseR;


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


		FloatFrame dryFrame = {0.0, 0.0};
		FloatFrame inFrame = {0.0, 0.0};
		for(int channel = 0;channel < CHANNELS;channel++) {
			compressor[channel].setSampleRate(double(args.sampleRate));
			
			// Get input to delay block
			feedbackTap[channel] = (int)clamp(params[FEEDBACK_TAP_L_PARAM+channel].getValue() + (inputs[FEEDBACK_TAP_L_INPUT+channel].isConnected() ? (inputs[FEEDBACK_TAP_L_INPUT+channel].getVoltage() / 10.0f) : 0),0.0f,17.0);
			feedbackSlip[channel] = clamp(params[FEEDBACK_L_SLIP_PARAM+channel].getValue() + (inputs[FEEDBACK_L_SLIP_CV_INPUT+channel].isConnected() ? (inputs[FEEDBACK_L_SLIP_CV_INPUT+channel].getVoltage() / 10.0f) : 0),-0.5f,0.5);
			float feedbackAmount = clamp(params[FEEDBACK_PARAM].getValue() + (inputs[FEEDBACK_INPUT].isConnected() ? (inputs[FEEDBACK_INPUT].getVoltage() / 10.0f) : 0), 0.0f, 1.0f);
			feedbackPercentage = feedbackAmount;
			float in = 0.0f;
					
			if(channel == 0) {
				in = inputs[IN_L_INPUT].getVoltageSum();	
				inFrame.l = in;
				dryFrame.l = in + lastFeedback.l * feedbackAmount;
			} else {
				in = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltageSum() : inputs[IN_L_INPUT].getVoltageSum();	
				inFrame.r = in;
				dryFrame.r = in + lastFeedback.r * feedbackAmount;
			}				

			feedbackPitch[channel] = floor(params[FEEDBACK_L_PITCH_SHIFT_PARAM+channel].getValue() + (inputs[FEEDBACK_L_PITCH_SHIFT_CV_INPUT+channel].isConnected() ? (inputs[FEEDBACK_L_PITCH_SHIFT_CV_INPUT+channel].getVoltage()*2.4f) : 0));
			feedbackDetune[channel] = floor(params[FEEDBACK_L_DETUNE_PARAM+channel].getValue() + (inputs[FEEDBACK_L_DETUNE_CV_INPUT+channel].isConnected() ? (inputs[FEEDBACK_L_DETUNE_CV_INPUT+channel].getVoltage()*10.0f) : 0));		

			feedbackTapPercentage[channel] = feedbackTap[channel] / 17.0;
			feedbackSlipPercentage[channel] = feedbackSlip[channel] * 2.0;
			feedbackPitchShiftPercentage[channel] = feedbackPitch[channel] / 24.0;
			feedbackDetunePercentage[channel] = feedbackDetune[channel] / 100.0;

		}
		float inLeval = std::max(abs(inFrame.l),abs(inFrame.r));		
		compressor[2].setSampleRate(double(args.sampleRate));
		compressor[2].process(inLeval);
		double duckingGainReduction = compressor[2].getGainReduction();

		FloatFrame dryToUse = dryFrame; //Normally the same as dry unless in reverse mode

		// Push dry sample into reverse history buffers
		reverseHistoryBuffer[0].push(dryFrame.l);
		reverseHistoryBuffer[1].push(dryFrame.r);
		if(reverse) {
			FloatFrame reverseDry;
			reverseDry.l = reverseHistoryBuffer[0].shift();
			reverseDry.r = reverseHistoryBuffer[1].shift();
			dryToUse = reverseDry;
		}	

		// Push dry sample into history buffer
		delayLine.write(dryToUse);

		FloatFrame wet = {0.0f, 0.0f}; // This is the mix of delays and input that is outputed
		FloatFrame feedbackValue = {0.0f, 0.0f}; // This is the output of a tap that gets sent back to input
				
		for(int tap = 0; tap < NUM_TAPS;tap++) { 

			// Stacking
			if(params[STACK_TRIGGER_MODE_PARAM].getValue() == GATE_TRIGGE_MODE && inputs[TAP_STACK_CV_INPUT+tap].isConnected()) {
				tapStacked[tap] = inputs[TAP_STACK_CV_INPUT+tap].getVoltage() > 0.0f;
			}
			//Button (or trigger) can override input
			if (tap < NUM_TAPS -1 && stackingTrigger[tap].process(params[TAP_STACKED_PARAM+tap].getValue() + (params[STACK_TRIGGER_MODE_PARAM].getValue() == TRIGGER_TRIGGER_MODE ? inputs[TAP_STACK_CV_INPUT+tap].getVoltage() : 0.0f))) {
				tapStacked[tap] = !tapStacked[tap];
			}

			float pitch,detune;


			pitch = floor((hasExpanderPitches ? expanderPitches[tap] * 24.0f : params[TAP_PITCH_SHIFT_PARAM+tap].getValue()) + (inputs[TAP_PITCH_SHIFT_CV_INPUT+tap].isConnected() ? (inputs[TAP_PITCH_SHIFT_CV_INPUT+tap].getVoltage()*2.4f) : 0));
			pShiftPercentage[tap] = pitch / 24.0;

			detune = floor((hasExpanderDetunes ? expanderDetunes[tap] * 99.0f : params[TAP_DETUNE_PARAM+tap].getValue()) + (inputs[TAP_DETUNE_CV_INPUT+tap].isConnected() ? (inputs[TAP_DETUNE_CV_INPUT+tap].getVoltage()*10.0f) : 0));
			pDetunePercentage[tap] = detune / 100.0;

			tapPitchShift[tap] = pitch;
			tapDetune[tap] = detune;
			pitch += detune/100.0f; 

			
			//Normally the delay tap is the same as the tap itself, unless it is stacked, then it is its neighbor;
			int delayTap = tap;
			while(delayTap < NUM_TAPS && tapStacked[delayTap]) {
				delayTap++;			
			}
			
			// Compute delay from base and groove
			float delay = baseDelay / NUM_TAPS * lerp(tapGroovePatterns[0][delayTap],tapGroovePatterns[tapGroovePattern][delayTap],grooveAmount); //Balance between straight time and groove
			if(expanderDelayTime[delayTap] > 0) {
				delay = expanderDelayTime[delayTap];
			}
						
			delayTime[tap] = (delay + delayMod); 
			if(delayTime[tap] != lastDelayTime[tap]) {
				float index = delayTime[tap] * sampleRate;
				delayLine.setDelayTime(tap,index);
				lastDelayTime[tap] = delayTime[tap];
			}
			
			FloatFrame wetTap = {0.0f, 0.0f};

			//Get Delay Tap Output
			FloatFrame initialOutput = delayLine.getTap(tap);
		
			granularPitchShift[tap].setRatio(SemitonesToRatio(pitch));
			granularPitchShift[tap].setSize(grainSize);

			FloatFrame pitchShiftOut = granularPitchShift[tap].process(initialOutput,true); 
				
			wetTap.l +=pitchShiftOut.l;
			wetTap.r +=pitchShiftOut.r;
					
			// Muting			
			if(params[MUTE_TRIGGER_MODE_PARAM].getValue() == GATE_TRIGGE_MODE && inputs[TAP_MUTE_CV_INPUT+tap].isConnected()) {
				tapMuted[tap] = inputs[TAP_MUTE_CV_INPUT+tap].getVoltage() > 0.0f;
			}
			//Button (or trigger) can override input
			if (mutingTrigger[tap].process(params[TAP_MUTE_PARAM+tap].getValue() + (inputs[TAP_MUTE_CV_INPUT+tap].isConnected() && params[MUTE_TRIGGER_MODE_PARAM].getValue() == TRIGGER_TRIGGER_MODE ? inputs[TAP_MUTE_CV_INPUT+tap].getVoltage() : 0))) {
				tapMuted[tap] = !tapMuted[tap];
			}			

			//Each tap - channel has its own filter
			int tapFilterType = (int)params[TAP_FILTER_TYPE_PARAM+tap].getValue();
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

				float cutoffExp = clamp((hasExpanderFcs ? expanderFcs[tap] : params[TAP_FC_PARAM+tap].getValue()) + inputs[TAP_FC_CV_INPUT+tap].getVoltage() / 10.0f,0.0f,1.0f);
				fcPercentage[tap] = cutoffExp;

				float tapQ = clamp((hasExpanderQs ? expanderQs[tap] : params[TAP_Q_PARAM+tap].getValue()) + (inputs[TAP_Q_CV_INPUT+tap].getVoltage() / 10.0f),0.01f,1.0f) * 50; 
				qPercentage[tap] = tapQ / 50.0;

				float tapFc = minCutoff * powf(maxCutoff / minCutoff, cutoffExp) / sampleRate;
				if(lastTapFc[tap] != tapFc) {
					filterParams[tap].setFreq(T(tapFc));
					lastTapFc[tap] = tapFc;
				}
				if(lastTapQ[tap] != tapQ) {
					filterParams[tap].setQ(tapQ); 
					lastTapQ[tap] = tapQ;
				}
				for(int channel=0;channel <CHANNELS;channel++) {		
					if(channel == 0) {
						wetTap.l = StateVariableFilter<T>::run(wetTap.l, filterStates[tap][channel], filterParams[tap]);
					} else {
						wetTap.r = StateVariableFilter<T>::run(wetTap.r, filterStates[tap][channel], filterParams[tap]);
					}
				}
			} else {
				fcPercentage[tap] = 0.0;
				qPercentage[tap] = 0.0;
			}
			lastFilterType[tap] = tapFilterType;

			
			bool tapCurrentlyMuted = tapMuted[tap] || expanderMuteTaps[tap];
			if(tapCurrentlyMuted != lastTapMutedStatus[tap]) {
				lastTapSmoothPosition[tap] = 0;
				lastTapMutedStatus[tap] = tapCurrentlyMuted;
			}
			
			float muteSmoothing = 1.0;
			if(lastTapSmoothPosition[tap] < SMOOTHING) {
				muteSmoothing = tapCurrentlyMuted ? lerp(1.0,0.0,lastTapSmoothPosition[tap]/SMOOTHING) : lerp(0.0,1.0,lastTapSmoothPosition[tap]/SMOOTHING);				
				lastTapSmoothPosition[tap]++;
			} else {
				muteSmoothing = tapCurrentlyMuted ? 0.0 : 1.0;
			}
			

			float level = clamp(((hasExpanderLevels ? expanderLevels[tap] : params[TAP_LEVEL_PARAM+tap].getValue()) + (inputs[TAP_PAN_CV_INPUT+tap].isConnected() ? (inputs[TAP_PAN_CV_INPUT+tap].getVoltage() / 10.0f) : 0)),0.0f,1.0f);
			levelPercentage[tap] = level;
			 
			float pan = clamp(((hasExpanderPans ? expanderPans[tap] : params[TAP_PAN_PARAM+tap].getValue()) + (inputs[TAP_PAN_CV_INPUT+tap].isConnected() ? (inputs[TAP_PAN_CV_INPUT+tap].getVoltage() / 5.0f) : 0)),-1.0f,1.0f);
			panPercentage[tap] = pan;
			wetTap.l = wetTap.l * muteSmoothing * level * std::max(1.0 - pan,0.0);  
			wetTap.r = wetTap.r * muteSmoothing * level * std::max(pan+1.0,1.0);   


			if(tapBreakoutPresent)
			{	
				//For Breakout
				tapLSends[tap] = wetTap.l;
				tapRSends[tap] = wetTap.r;
				bool isExpanderPortConnectedL =  (bool)tapLConnections[tap]; 
				if (isExpanderPortConnectedL) {
					wetTap.l =  tapLReturns[tap];  
				}
				bool isExpanderPortConnectedR = (bool)tapRConnections[tap]; 
				if (isExpanderPortConnectedR) {
					wetTap.r = tapRReturns[tap]; \
				}				
			}


			wet.l += wetTap.l;
			wet.r += wetTap.r;
		

			lights[TAP_STACKED_LIGHT+tap].value = tapStacked[tap];
			lights[TAP_MUTED_LIGHT+tap].value = (tapMuted[tap] || expanderMuteTaps[tap]);	

		}

				
		//Process Feedback delays and pitch shifting
		for(int channel = 0;channel < CHANNELS;channel ++) {
			float delay = 0.0f;
			FloatFrame initialFBOutput = {0.0f, 0.0f};

			//Pull feedback time off of normal tap time
			if(feedbackTap[channel] != NUM_TAPS ) {
				
				if(feedbackTap[channel] == NUM_TAPS+1) {
					//External feedback time
					delay = clamp(inputs[EXTERNAL_DELAY_TIME_INPUT].getVoltage(), 0.001f, 10.0f); //Need to process this same as other size...
				} else { 
					// Use tap as basis of delay time
					int delayTap = feedbackTap[channel];				
					while(delayTap < NUM_TAPS && tapStacked[delayTap]) {
						delayTap++;			
					}

					float slip = feedbackSlip[channel] * baseDelay / NUM_TAPS;
					delay = delayTime[delayTap] + slip; 
				}
			
				float index = delay * sampleRate;
				if(index > 0  && index != lastDelayTime[NUM_TAPS+channel] ) {
					delayLine.setDelayTime(NUM_TAPS+channel,index);
				}

				FloatFrame tempOutput = delayLine.getTap(NUM_TAPS+channel);
				if(channel == 0) {
					initialFBOutput.l = tempOutput.l; 
				} else {
					initialFBOutput.r = tempOutput.r;
				}								
			}

			if(feedbackTap[channel] == NUM_TAPS) { //This would be the All Taps setting
				delay = delayTime[NUM_TAPS-1]; // last tap
				if(channel == 0) {
					initialFBOutput.l = wet.l; 
				} else {
					initialFBOutput.r = wet.r;
				}
			}
					
			//Set reverse size = delay of feedback
			reverseHistoryBuffer[channel].setDelaySize((delay) * sampleRate);			
			
			float pitch,detune;
			pitch = feedbackPitch[channel];
			detune = feedbackDetune[channel];
			pitch += detune/100.0f; 

			FloatFrame pitchShiftedFB = {0.0f,0.0f};			
			granularPitchShift[NUM_TAPS+channel].setRatio(SemitonesToRatio(pitch));
			granularPitchShift[NUM_TAPS+channel].setSize(grainSize);

			FloatFrame pitchShiftOut = granularPitchShift[NUM_TAPS+channel].process(initialFBOutput,true); 
				
			pitchShiftedFB.l +=pitchShiftOut.l; 
			pitchShiftedFB.r +=pitchShiftOut.r;
			
			if(channel == 0) {
				feedbackValue.l = pitchShiftedFB.l; 				
			} else {
				feedbackValue.r = pitchShiftedFB.r;				
			}
		}
	
		

		//Apply global filtering
		float color = clamp(params[FEEDBACK_TONE_PARAM].getValue() + inputs[FEEDBACK_TONE_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
		feedbackTonePercentage = color;

		if(color != lastColor) {
			float lowpassFreq = 10000.0f * powf(10.0f, clamp(2.0f*color, 0.0f, 1.0f));
			lowpassFilter[0].setCutoff(lowpassFreq / sampleRate);
			lowpassFilter[1].setCutoff(lowpassFreq / sampleRate);
		
			float highpassFreq = 10.0f * powf(10.0f, clamp(2.0f*color - 1.0f, 0.0f, 1.0f));
			highpassFilter[0].setCutoff(highpassFreq / sampleRate);
			highpassFilter[1].setCutoff(highpassFreq / sampleRate);
		
			lastColor = color;
		}

		lowpassFilter[0].process(feedbackValue.l);
		feedbackValue.l = lowpassFilter[0].lowpass();
		lowpassFilter[1].process(feedbackValue.r);
		feedbackValue.r = lowpassFilter[1].lowpass();
		
		highpassFilter[0].process(feedbackValue.l);
		feedbackValue.l = highpassFilter[0].highpass();
		highpassFilter[1].process(feedbackValue.r);
		feedbackValue.r = highpassFilter[1].highpass();

		outputs[FEEDBACK_L_OUTPUT].setVoltage(feedbackValue.l);
		outputs[FEEDBACK_R_OUTPUT].setVoltage(feedbackValue.r);

		if(inputs[FEEDBACK_L_RETURN].isConnected()) {
			feedbackValue.l = inputs[FEEDBACK_L_RETURN].getVoltage();
		}
		if(inputs[FEEDBACK_R_RETURN].isConnected()) {
			feedbackValue.r = inputs[FEEDBACK_R_RETURN].getVoltage();
		}

		float fbDuckAmount = clamp(params[DUCKING_FB_AMOUNT_PARAM].getValue() + inputs[DUCKING_FB_AMOUNT_CV_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
		duckingFBPercentage = fbDuckAmount;
		double fbdDucking = chunkware_simple::dB2lin(-duckingGainReduction * fbDuckAmount );

		feedbackValue.l *= fbdDucking;
		feedbackValue.r *= fbdDucking;
		
		if (pingPong) {
			lastFeedback.l = limit(feedbackValue.r,1);
			lastFeedback.r = limit(feedbackValue.l,0);
		} else
		{
			lastFeedback.l = limit(feedbackValue.l,0);
			lastFeedback.r = limit(feedbackValue.r,1);
		}
		
		float mix = clamp(params[MIX_PARAM].getValue() + inputs[MIX_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
		mixPercentage = mix;
		float duckAmount = clamp(params[DUCKING_AMOUNT_PARAM].getValue() + inputs[DUCKING_AMOUNT_CV_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);	
		duckingPercentage = duckAmount;		
		double ducking = chunkware_simple::dB2lin(-duckingGainReduction * duckAmount );
		float phaseL = phaseReverseL ? -1.0 : 1.0;
		float phaseR = phaseReverseR ? -1.0 : 1.0;

		float outL = crossfade(inFrame.l, wet.l * ducking * phaseL, mix);  // Not sure this should be wet
		float outR = crossfade(inFrame.r, wet.r * ducking * phaseR, mix);  // Not sure this should be wet
		
		outputs[OUT_L_OUTPUT].setVoltage(outL);
		outputs[OUT_R_OUTPUT].setVoltage(outR);

		if(tapBreakoutPresent)
		{	
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			memcpy(&messageToExpander[NUM_TAPS], &tapLSends, sizeof(float) * NUM_TAPS);
			memcpy(&messageToExpander[NUM_TAPS*2], &tapRSends, sizeof(float) * NUM_TAPS);
		}


		//if(tapBreakoutPresent || algorithmExpanderPresent) {
		if(rightExpanderPresent) {
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
	}


	void randomizeParameters(int parameterGroup) {
		float rnd;
		switch(parameterGroup) {
			case STACKING_AND_MUTING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					rnd = ((float) rand()/RAND_MAX);
					tapMuted[tap] = rnd > 0.5;
					rnd = ((float) rand()/RAND_MAX);
					tapStacked[tap] = rnd > 0.5;
				}
				break;
			case LEVELS_AND_PANNING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					rnd = ((float) rand()/RAND_MAX);
					params[TAP_LEVEL_PARAM+tap].setValue(rnd);
					rnd = ((float) rand()/RAND_MAX) * 2 - 1;
					params[TAP_PAN_PARAM+tap].setValue(rnd);
				}
				break;
			case FILTERING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					rnd = ((float) rand()/RAND_MAX) * 4.5;
					params[TAP_FILTER_TYPE_PARAM+tap].setValue((int)rnd);
					rnd = ((float) rand()/RAND_MAX);
					params[TAP_FC_PARAM+tap].setValue(rnd);
					rnd = ((float) rand()/RAND_MAX);
					params[TAP_Q_PARAM+tap].setValue(rnd);
				}
				break;
			case PITCH_SHIFTING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					rnd = ((float) rand()/RAND_MAX) * 48 - 24;
					params[TAP_PITCH_SHIFT_PARAM+tap].setValue(rnd);
					rnd = ((float) rand()/RAND_MAX) * 198 - 99;
					params[TAP_DETUNE_PARAM+tap].setValue(rnd);
				}
				break;
		}
	}


	void reinitializeParameters(int parameterGroup) {
		switch(parameterGroup) {
			case STACKING_AND_MUTING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					tapMuted[tap] = false;
					tapStacked[tap] = false;
				}
				break;
			case LEVELS_AND_PANNING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					params[TAP_LEVEL_PARAM+tap].setValue(0.5f);
					params[TAP_PAN_PARAM+tap].setValue(0.0f);
				}
				break;
			case FILTERING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					params[TAP_FILTER_TYPE_PARAM+tap].setValue(0);
					params[TAP_FC_PARAM+tap].setValue(0.5f);
					params[TAP_Q_PARAM+tap].setValue(0.1f);
				}
				break;
			case PITCH_SHIFTING_GROUP :
				for(int tap = 0; tap < NUM_TAPS;tap++) {
					params[TAP_PITCH_SHIFT_PARAM+tap].setValue(0);
					params[TAP_DETUNE_PARAM+tap].setValue(0);
				}
				break;
		}
	}


	void onReset() override {
		reverse = false;
		pingPong = false;
		compressionMode = COMPRESSION_NONE;
		for(int tap = 0; tap < NUM_TAPS;tap++) {
			tapMuted[tap] = false;
			tapStacked[tap] = false;
			expanderDelayTime[tap] = 0;
		}
	}

};




struct PWStatusDisplay : TransparentWidget {
	PortlandWeather *module;
	int frame = 0;
	std::shared_ptr<Font> fontNumbers,fontText;

	

	PWStatusDisplay() {
		fontNumbers = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
		fontText = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}

	void drawDivisionMultiplier(const DrawArgs &args, Vec pos, double dm) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, fontNumbers->handle);
		nvgTextLetterSpacing(args.vg, -2);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		
		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%3.0f", dm);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawDelayTime(const DrawArgs &args, Vec pos, float delayTime) {
		nvgFontSize(args.vg, 26);
		nvgFontFaceId(args.vg, fontNumbers->handle);
		nvgTextLetterSpacing(args.vg, -2);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		
		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%6.0f", delayTime*1000);	
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawGrooveType(const DrawArgs &args, Vec pos, int grooveType,bool usingAlgorithm) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
		

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", !usingAlgorithm ? module->grooveNames[grooveType] : "Algorithm");
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawFeedbackTaps(const DrawArgs &args, Vec pos, int *feedbackTaps) {
		nvgFontSize(args.vg, 12);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		for(int i=0;i<CHANNELS;i++) {
			char text[128];
			snprintf(text, sizeof(text), "%s", module->tapNames[feedbackTaps[i]]);
			nvgText(args.vg, pos.x + i*42, pos.y, text, NULL);
		}
	}

	void drawFeedbackPitch(const DrawArgs &args, Vec pos, float *feedbackPitch) {
		nvgFontSize(args.vg, 12);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%-2.0f", feedbackPitch[0]);
		nvgText(args.vg, pos.x , pos.y, text, NULL);
		snprintf(text, sizeof(text), "%2.0f", feedbackPitch[1]);
		nvgText(args.vg, pos.x + 42, pos.y, text, NULL);
	}

	void drawFeedbackDetune(const DrawArgs &args, Vec pos, float *feedbackDetune) {
		nvgFontSize(args.vg, 12);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%-2.0f", feedbackDetune[0]);
		nvgText(args.vg, pos.x , pos.y, text, NULL);
		snprintf(text, sizeof(text), "%2.0f", feedbackDetune[1]);
		nvgText(args.vg, pos.x + 42, pos.y, text, NULL);
	}


	void drawFilterTypes(const DrawArgs &args, Vec pos, int *filterType) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
		for(int i=0;i<NUM_TAPS;i++) {
			char text[128];
			snprintf(text, sizeof(text), "%s", module->filterNames[filterType[i]]);
			nvgText(args.vg, pos.x + i*30, pos.y, text, NULL);
		}
	}

	void drawTapPitchShift(const DrawArgs &args, Vec pos, float *pitchShift) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
		for(int i=0;i<NUM_TAPS;i++) {
			char text[128];
			snprintf(text, sizeof(text), "%-2.0f", pitchShift[i]);
			nvgText(args.vg, pos.x + i*30, pos.y, text, NULL);
		}
	}

	void drawTapDetune(const DrawArgs &args, Vec pos, float *detune) {
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, fontText->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
		for(int i=0;i<NUM_TAPS;i++) {
			char text[128];
			snprintf(text, sizeof(text), "%-3.0f", detune[i]);
			nvgText(args.vg, pos.x + i*30, pos.y, text, NULL);
		}
	}

	

	void draw(const DrawArgs &args) override {

		if (!module)
			return;
		
		drawDivisionMultiplier(args, Vec(105,56), module->multiplier);
		drawDivisionMultiplier(args, Vec(156,56), module->division);
		drawDelayTime(args, Vec(178,130), module->baseDelay);
		drawGrooveType(args, Vec(88,203), module->tapGroovePattern, module->usingAlgorithm);
		drawFeedbackTaps(args, Vec(310,174), module->feedbackTap);
		drawFeedbackPitch(args, Vec(310,254), module->feedbackPitch);
		drawFeedbackDetune(args, Vec(310,295), module->feedbackDetune);
		
		drawFilterTypes(args, Vec(490,210), module->lastFilterType);
		//drawTapPitchShift(args, Vec(513,320), module->tapPitchShift);
		//drawTapDetune(args, Vec(513,360), module->tapDetune);
	}
};


struct PortlandWeatherWidget : ModuleWidget {

	

	PortlandWeatherWidget(PortlandWeather *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PortlandWeather.svg")));


		
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			PWStatusDisplay *display = new PWStatusDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 385);
			addChild(display);
		}

		ParamWidget* clockMultiplyParam = createParam<RoundLargeFWSnapKnob>(Vec(12, 40), module, PortlandWeather::CLOCK_MULT_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWSnapKnob*>(clockMultiplyParam)->percentage = &module->clockMultiplyPercentage;
		}
		addParam(clockMultiplyParam);							

		ParamWidget* clockDivideParam = createParam<RoundLargeFWSnapKnob>(Vec(170, 40), module, PortlandWeather::CLOCK_DIV_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWSnapKnob*>(clockDivideParam)->percentage = &module->clockDividePercentage;
		}
		addParam(clockDivideParam);							
		ParamWidget* timeParam = createParam<RoundLargeFWKnob>(Vec(12, 105), module, PortlandWeather::TIME_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(timeParam)->percentage = &module->timePercentage;
		}
		addParam(timeParam);							


		ParamWidget* grooveTypeParam = createParam<RoundLargeFWSnapKnob>(Vec(12, 175), module, PortlandWeather::GROOVE_TYPE_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWSnapKnob*>(grooveTypeParam)->percentage = &module->grooveTypePercentage;
		}
		addParam(grooveTypeParam);							

		ParamWidget* grooveAmountParam = createParam<RoundLargeFWKnob>(Vec(12, 230), module, PortlandWeather::GROOVE_AMOUNT_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(grooveAmountParam)->percentage = &module->grooveAmountPercentage;
		}
		addParam(grooveAmountParam);							


		ParamWidget* feedbackParam = createParam<RoundLargeFWKnob>(Vec(217, 55), module, PortlandWeather::FEEDBACK_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(feedbackParam)->percentage = &module->feedbackPercentage;
		}
		addParam(feedbackParam);							
		ParamWidget* duckingFBParam = createParam<RoundLargeFWKnob>(Vec(287, 55), module, PortlandWeather::DUCKING_FB_AMOUNT_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(duckingFBParam)->percentage = &module->duckingFBPercentage;
		}
		addParam(duckingFBParam);							
		ParamWidget* feedbackToneParam = createParam<RoundLargeFWKnob>(Vec(349, 55), module, PortlandWeather::FEEDBACK_TONE_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(feedbackToneParam)->percentage = &module->feedbackTonePercentage;
		}
		addParam(feedbackToneParam);							


		ParamWidget* feedbackTapLParam = createParam<RoundFWSnapKnob>(Vec(223, 153), module, PortlandWeather::FEEDBACK_TAP_L_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(feedbackTapLParam)->percentage = &module->feedbackTapPercentage[0];
		}
		addParam(feedbackTapLParam);							
		ParamWidget* feedbackTapRParam = createParam<RoundFWSnapKnob>(Vec(364, 153), module, PortlandWeather::FEEDBACK_TAP_R_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(feedbackTapRParam)->percentage = &module->feedbackTapPercentage[1];
		}
		addParam(feedbackTapRParam);							
		ParamWidget* feedbackSlipLParam = createParam<RoundFWKnob>(Vec(223, 193), module, PortlandWeather::FEEDBACK_L_SLIP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(feedbackSlipLParam)->percentage = &module->feedbackSlipPercentage[0];
			dynamic_cast<RoundFWKnob*>(feedbackSlipLParam)->biDirectional = true;
		}
		addParam(feedbackSlipLParam);							
		ParamWidget* feedbackSlipRParam = createParam<RoundFWKnob>(Vec(364, 193), module, PortlandWeather::FEEDBACK_R_SLIP_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(feedbackSlipRParam)->percentage = &module->feedbackSlipPercentage[1];
			dynamic_cast<RoundFWKnob*>(feedbackSlipRParam)->biDirectional = true;
		}
		addParam(feedbackSlipRParam);							
		ParamWidget* feedbackPitchShiftLParam = createParam<RoundFWSnapKnob>(Vec(223, 233), module, PortlandWeather::FEEDBACK_L_PITCH_SHIFT_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(feedbackPitchShiftLParam)->percentage = &module->feedbackPitchShiftPercentage[0];
			dynamic_cast<RoundFWSnapKnob*>(feedbackPitchShiftLParam)->biDirectional = true;
		}
		addParam(feedbackPitchShiftLParam);							
		ParamWidget* feedbackPitchShiftRParam = createParam<RoundFWSnapKnob>(Vec(364, 233), module, PortlandWeather::FEEDBACK_R_PITCH_SHIFT_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(feedbackPitchShiftRParam)->percentage = &module->feedbackPitchShiftPercentage[1];
			dynamic_cast<RoundFWSnapKnob*>(feedbackPitchShiftRParam)->biDirectional = true;
		}
		addParam(feedbackPitchShiftRParam);							
		ParamWidget* feedbackDetuneLParam = createParam<RoundFWKnob>(Vec(223, 273), module, PortlandWeather::FEEDBACK_L_DETUNE_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(feedbackDetuneLParam)->percentage = &module->feedbackDetunePercentage[0];
			dynamic_cast<RoundFWKnob*>(feedbackDetuneLParam)->biDirectional = true;
		}
		addParam(feedbackDetuneLParam);							
		ParamWidget* feedbackDetuneRParam = createParam<RoundFWKnob>(Vec(364, 273), module, PortlandWeather::FEEDBACK_R_DETUNE_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(feedbackDetuneRParam)->percentage = &module->feedbackDetunePercentage[1];
			dynamic_cast<RoundFWKnob*>(feedbackDetuneRParam)->biDirectional = true;
		}
		addParam(feedbackDetuneRParam);							

		
		addParam(createParam<LEDButton>(Vec(236,116), module, PortlandWeather::REVERSE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(237.5, 117.5), module, PortlandWeather::REVERSE_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(260, 112), module, PortlandWeather::REVERSE_INPUT));

		addParam( createParam<LEDButton>(Vec(371,116), module, PortlandWeather::PING_PONG_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(372.5, 117.5), module, PortlandWeather::PING_PONG_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(395, 112), module, PortlandWeather::PING_PONG_INPUT));

		addParam(createParam<LEDButton>(Vec(332, 330), module, PortlandWeather::COMPRESSION_MODE_PARAM));
		addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(333.5, 331.5), module, PortlandWeather::COMPRESSION_MODE_LIGHT));

		addParam( createParam<LEDButton>(Vec(127, 340), module, PortlandWeather::PHASE_REVERSE_L_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(128.5, 341.5), module, PortlandWeather::PHASE_REVERSE_L_LIGHT));
		addParam( createParam<LEDButton>(Vec(200, 340), module, PortlandWeather::PHASE_REVERSE_R_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(201.5, 341.5), module, PortlandWeather::PHASE_REVERSE_R_LIGHT));


		//last tap isn't stacked
		for (int i = 0; i< NUM_TAPS-1; i++) {
			addParam(createParam<LEDButton>(Vec(437 + 52 + 30*i,17), module, PortlandWeather::TAP_STACKED_PARAM + i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(437 + 53.5 + 30*i, 18.5), module, PortlandWeather::TAP_STACKED_LIGHT+i));
			addInput(createInput<FWPortInSmall>(Vec(437 + 52+ 30*i, 37), module, PortlandWeather::TAP_STACK_CV_INPUT+i));
		}

		for (int i = 0; i < NUM_TAPS; i++) {
			addParam( createParam<LEDButton>(Vec(437 + 52 + 30*i,57), module, PortlandWeather::TAP_MUTE_PARAM + i));
			addChild(createLight<LargeLight<RedLight>>(Vec(437 + 53.5 + 30*i, 58.5), module, PortlandWeather::TAP_MUTED_LIGHT+i));
			addInput(createInput<FWPortInSmall>(Vec(437 + 52+ 30*i, 77), module, PortlandWeather::TAP_MUTE_CV_INPUT+i));

			ParamWidget* tapLevelParam = createParam<RoundReallySmallFWKnob>(Vec(437 + 50 + 30*i, 98), module, PortlandWeather::TAP_LEVEL_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(tapLevelParam)->percentage = &module->levelPercentage[i];
			}
			addParam(tapLevelParam);							
			addInput(createInput<FWPortInSmall>(Vec(437 + 52 + 30*i, 119), module, PortlandWeather::TAP_MIX_CV_INPUT+i));

			ParamWidget* tapPanParam = createParam<RoundReallySmallFWKnob>(Vec(437 + 50 + 30*i, 140), module, PortlandWeather::TAP_PAN_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(tapPanParam)->percentage = &module->panPercentage[i];
				dynamic_cast<RoundReallySmallFWKnob*>(tapPanParam)->biDirectional = true;
			}
			addParam(tapPanParam);							
			addInput(createInput<FWPortInSmall>(Vec(437 + 52 + 30*i, 161), module, PortlandWeather::TAP_PAN_CV_INPUT+i));

			addParam( createParam<RoundReallySmallFWSnapKnob>(Vec(437 + 50 + 30*i, 182), module, PortlandWeather::TAP_FILTER_TYPE_PARAM + i));

			ParamWidget* tapFcCutoffParam = createParam<RoundReallySmallFWKnob>(Vec(437 + 50 + 30*i, 213), module, PortlandWeather::TAP_FC_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(tapFcCutoffParam)->percentage = &module->fcPercentage[i];
			}
			addParam(tapFcCutoffParam);							
			addInput(createInput<FWPortInSmall>(Vec(437 + 52 + 30*i, 234), module, PortlandWeather::TAP_FC_CV_INPUT+i));

			ParamWidget* tapQParam = createParam<RoundReallySmallFWKnob>(Vec(437 + 50 + 30*i, 255), module, PortlandWeather::TAP_Q_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(tapQParam)->percentage = &module->qPercentage[i];
			}
			addParam(tapQParam);							
			addInput(createInput<FWPortInSmall>(Vec(437 + 52 + 30*i, 276), module, PortlandWeather::TAP_Q_CV_INPUT+i));

			ParamWidget* tapPitchShiftParam = createParam<RoundReallySmallFWSnapKnob>(Vec(437 + 50 + 30*i, 297), module, PortlandWeather::TAP_PITCH_SHIFT_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWSnapKnob*>(tapPitchShiftParam)->percentage = &module->pShiftPercentage[i];
				dynamic_cast<RoundReallySmallFWSnapKnob*>(tapPitchShiftParam)->biDirectional = true;
			}
			addParam(tapPitchShiftParam);							
			addInput(createInput<FWPortInSmall>(Vec(437 + 52 + 30*i, 318), module, PortlandWeather::TAP_PITCH_SHIFT_CV_INPUT+i));

			ParamWidget* tapPitchDetuneParam = createParam<RoundReallySmallFWKnob>(Vec(437 + 50 + 30*i, 339), module, PortlandWeather::TAP_DETUNE_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(tapPitchDetuneParam)->percentage = &module->pDetunePercentage[i];
				dynamic_cast<RoundReallySmallFWKnob*>(tapPitchDetuneParam)->biDirectional = true;
			}
			addParam(tapPitchDetuneParam);							
			addInput(createInput<FWPortInSmall>(Vec(437 + 52 + 30*i, 360), module, PortlandWeather::TAP_DETUNE_CV_INPUT+i));
		}

		addParam( createParam<CKSS>(Vec(290, 114), module, PortlandWeather::REVERSE_TRIGGER_MODE_PARAM));
		addParam( createParam<CKSS>(Vec(353, 114), module, PortlandWeather::PING_PONG_TRIGGER_MODE_PARAM));
		addParam( createParam<HCKSS>(Vec(437 + 14, 37), module, PortlandWeather::STACK_TRIGGER_MODE_PARAM));
		addParam( createParam<HCKSS>(Vec(437 + 14, 77), module, PortlandWeather::MUTE_TRIGGER_MODE_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(55, 66), module, PortlandWeather::CLOCK_MULT_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(146, 66), module, PortlandWeather::CLOCK_DIVISION_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(55, 110), module, PortlandWeather::TIME_CV_INPUT));
		
		addInput(createInput<FWPortInSmall>(Vec(55, 180), module, PortlandWeather::GROOVE_TYPE_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(55, 235), module, PortlandWeather::GROOVE_AMOUNT_CV_INPUT));


		addInput(createInput<FWPortInSmall>(Vec(260, 60), module, PortlandWeather::FEEDBACK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(325, 60), module, PortlandWeather::DUCKING_FB_AMOUNT_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(395, 60), module, PortlandWeather::FEEDBACK_TONE_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(260, 157), module, PortlandWeather::FEEDBACK_TAP_L_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(401, 157), module, PortlandWeather::FEEDBACK_TAP_R_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(260, 197), module, PortlandWeather::FEEDBACK_L_SLIP_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(401, 197), module, PortlandWeather::FEEDBACK_R_SLIP_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(260, 237), module, PortlandWeather::FEEDBACK_L_PITCH_SHIFT_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(401, 237), module, PortlandWeather::FEEDBACK_R_PITCH_SHIFT_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(260, 277), module, PortlandWeather::FEEDBACK_L_DETUNE_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(401, 277), module, PortlandWeather::FEEDBACK_R_DETUNE_CV_INPUT));


		addParam(createParam<CKD6>(Vec(25, 282), module, PortlandWeather::CLEAR_BUFFER_PARAM));


		ParamWidget* mixParam = createParam<RoundLargeFWKnob>(Vec(75, 273), module, PortlandWeather::MIX_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(mixParam)->percentage = &module->mixPercentage;
		}
		addParam(mixParam);							
		addInput(createInput<FWPortInSmall>(Vec(120, 280), module, PortlandWeather::MIX_INPUT));

		ParamWidget* duckingParam = createParam<RoundLargeFWKnob>(Vec(140, 273), module, PortlandWeather::DUCKING_AMOUNT_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(duckingParam)->percentage = &module->duckingPercentage;
		}
		addParam(duckingParam);
		addInput(createInput<FWPortInSmall>(Vec(185, 280), module, PortlandWeather::DUCKING_AMOUNT_CV_INPUT));


		addInput(createInput<PJ301MPort>(Vec(15, 330), module, PortlandWeather::CLOCK_INPUT));

		addInput(createInput<PJ301MPort>(Vec(65, 330), module, PortlandWeather::IN_L_INPUT));
		addInput(createInput<PJ301MPort>(Vec(95, 330), module, PortlandWeather::IN_R_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(145, 330), module, PortlandWeather::OUT_L_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(175, 330), module, PortlandWeather::OUT_R_OUTPUT));

		addInput(createInput<PJ301MPort>(Vec(227, 330), module, PortlandWeather::EXTERNAL_DELAY_TIME_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(262, 330), module, PortlandWeather::FEEDBACK_L_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(292, 330), module, PortlandWeather::FEEDBACK_R_OUTPUT));
		addInput(createInput<PJ301MPort>(Vec(365, 330), module, PortlandWeather::FEEDBACK_L_RETURN));
		addInput(createInput<PJ301MPort>(Vec(395, 330), module, PortlandWeather::FEEDBACK_R_RETURN));

	}

	struct GrainCountItem : MenuItem {
		PortlandWeather *module;
		int grainCount;
		void onAction(const event::Action &e) override {
			module->grainCount = grainCount;
		}
		void step() override {
			rightText = (module->grainCount == grainCount) ? "✔" : "";
		}
	};

	struct GrainSizeItem : MenuItem {
		PortlandWeather *module;
		float grainSize;
		void onAction(const event::Action &e) override {
			module->grainSize = grainSize;
		}
		void step() override {
			rightText = (module->grainSize == grainSize) ? "✔" : "";
		}
	};

	struct RandomizeParamsItem : MenuItem {
		PortlandWeather *module;
		int parameterGroup;
		void onAction(const event::Action &e) override {		
			module->randomizeParameters(parameterGroup);
		}		
	};

	struct ReinitializeParamsItem : MenuItem {
		PortlandWeather *module;
		int parameterGroup;
		void onAction(const event::Action &e) override {		
			module->reinitializeParameters(parameterGroup);
		}		
	};
	
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		PortlandWeather *module = dynamic_cast<PortlandWeather*>(this->module);
		assert(module);

		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Grain Size";
		menu->addChild(settingsLabel);

		GrainSizeItem *grainSize1Item = new GrainSizeItem();
		grainSize1Item->text = "2048";// 
		grainSize1Item->module = module;
		grainSize1Item->grainSize= .125;
		menu->addChild(grainSize1Item);

		GrainSizeItem *grainSize2Item = new GrainSizeItem();
		grainSize2Item->text = "1024";// 
		grainSize2Item->module = module;
		grainSize2Item->grainSize= .25;
		menu->addChild(grainSize2Item);

		GrainSizeItem *grainSize3Item = new GrainSizeItem();
		grainSize3Item->text = "512";// 
		grainSize3Item->module = module;
		grainSize3Item->grainSize= 0.5f;
		menu->addChild(grainSize3Item);


		GrainSizeItem *grainSize4Item = new GrainSizeItem();
		grainSize4Item->text = "128";// 
		grainSize4Item->module = module;
		grainSize4Item->grainSize= 1.0f;
		menu->addChild(grainSize4Item);

		menu->addChild(new MenuLabel());// empty line

		MenuLabel *randomLabel = new MenuLabel();
		randomLabel->text = "Randomize";
		menu->addChild(randomLabel);

		RandomizeParamsItem *randomizeStackMuteItem = new RandomizeParamsItem();
		randomizeStackMuteItem->text = "Stacking & Muting";
		randomizeStackMuteItem->parameterGroup = module->STACKING_AND_MUTING_GROUP;
		randomizeStackMuteItem->module = module;
		menu->addChild(randomizeStackMuteItem);

		RandomizeParamsItem *randomizeVolPanItem = new RandomizeParamsItem();
		randomizeVolPanItem->text = "Level & Panning";
		randomizeVolPanItem->parameterGroup = module->LEVELS_AND_PANNING_GROUP;
		randomizeVolPanItem->module = module;
		menu->addChild(randomizeVolPanItem);

		RandomizeParamsItem *randomizeFilteringItem = new RandomizeParamsItem();
		randomizeFilteringItem->text = "Filtering";
		randomizeFilteringItem->parameterGroup = module->FILTERING_GROUP;
		randomizeFilteringItem->module = module;
		menu->addChild(randomizeFilteringItem);

		RandomizeParamsItem *randomizePitchItem = new RandomizeParamsItem();
		randomizePitchItem->text = "Pitch Shifting"; 
		randomizePitchItem->parameterGroup = module->PITCH_SHIFTING_GROUP;
		randomizePitchItem->module = module;
		menu->addChild(randomizePitchItem);	

		MenuLabel *initializeLabel = new MenuLabel();
		initializeLabel->text = "Initialize";
		menu->addChild(initializeLabel);

		ReinitializeParamsItem *reinitializeStackMuteItem = new ReinitializeParamsItem();
		reinitializeStackMuteItem->text = "Stacking & Muting";
		reinitializeStackMuteItem->parameterGroup = module->STACKING_AND_MUTING_GROUP;
		reinitializeStackMuteItem->module = module;
		menu->addChild(reinitializeStackMuteItem);

		ReinitializeParamsItem *reinitializeVolPanItem = new ReinitializeParamsItem();
		reinitializeVolPanItem->text = "Level & Panning";
		reinitializeVolPanItem->parameterGroup = module->LEVELS_AND_PANNING_GROUP;
		reinitializeVolPanItem->module = module;
		menu->addChild(reinitializeVolPanItem);

		ReinitializeParamsItem *reinitializeFilteringItem = new ReinitializeParamsItem();
		reinitializeFilteringItem->text = "Filtering";
		reinitializeFilteringItem->parameterGroup = module->FILTERING_GROUP;
		reinitializeFilteringItem->module = module;
		menu->addChild(reinitializeFilteringItem);

		ReinitializeParamsItem *reinitializePitchItem = new ReinitializeParamsItem();
		reinitializePitchItem->text = "Pitch Shifting"; 
		reinitializePitchItem->parameterGroup = module->PITCH_SHIFTING_GROUP;
		reinitializePitchItem->module = module;
		menu->addChild(reinitializePitchItem);	

	}

};


Model *modelPortlandWeather = createModel<PortlandWeather, PortlandWeatherWidget>("PortlandWeather");
