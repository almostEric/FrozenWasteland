#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "filters/biquad.h"

using namespace std;

struct LowFrequencyOscillator {
	float phase = 0.0;
	float pw = 0.5;
	float freq = 1.0;
	

	LowFrequencyOscillator() {}
	void setPitch(float pitch) {
		pitch = fminf(pitch, 8.0);
		freq = powf(2.0, pitch);
	}
	void setPulseWidth(float pw_) {
		const float pwMin = 0.01;
		pw = clamp(pw_, pwMin, 1.0f - pwMin);
	}
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5);
		phase += deltaPhase;
		if (phase >= 1.0)
			phase -= 1.0;
	}
	float sin(float phaseOffset) {
		float newPhase = fabsf(phase + phaseOffset);
		return sinf(2*M_PI * newPhase);
	}
	float tri(float phaseOffset) {
		float newPhase = 4.0 * fabsf(phase + phaseOffset -0.75 - roundf(phase + phaseOffset - 0.75));
		return -1.0 + newPhase;
	}
	float saw(float phaseOffset) {
		return 2.0 * (phase + phaseOffset - roundf(phase + phaseOffset));
	}
	float sqr(float phaseOffset) {
		float newPhase = fabsf(phase + phaseOffset);
		float sqr = (newPhase < pw);
		return sqr;
	}
	float light() {
		return sinf(2*M_PI * phase);
	}
};

#define NUM_STAGE_SETUPS 3
#define MAX_STAGES 12
#define MAX_CHANNELS 2

struct JustAPhaser : Module {
	enum ParamIds {
		NUMBER_STAGES_PARAM,
		FILTER_MODE_PARAM,
		WAVESHAPE_PARAM,
		FREQUENCY_PARAM,
		DEPTH_PARAM,
		FREQUENCY_MOD_TYPE_PARAM,
		FEEDBACK_AMOUNT_PARAM,
		CENTER_FREQUENCY_PARAM,
		FREQUENCY_SPAN_PARAM,
		RESONANCE_PARAM,
		STEREO_PHASE_PARAM,
		MIX_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN_L_IN,
		IN_R_IN,
		FB_IN_L_IN,
		FB_IN_R_IN,
		FREQUENCY_INPUT,
		DEPTH_INPUT,
		FEEDBACK_INPUT,
		CENTER_FREQUENCY_INPUT,
		FREQUENCY_SPAN_INPUT,
		RESONANCE_INPUT,
		STEREO_PHASE_INPUT,
		EXTERNAL_MOD_INPUT_L,		
		EXTERNAL_MOD_INPUT_R,	
		MIX_CV_INPUT,	
		NUM_INPUTS
	};
	enum OutputIds {
		FB_OUT_L_OUTPUT,
		FB_OUT_R_OUTPUT,
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		STAGES_4_LIGHT,
		STAGES_8_LIGHT,
		STAGES_12_LIGHT,
		SIN_LFO_LIGHT,
		TRI_LFO_LIGHT,
		SAW_LFO_LIGHT,
		SQR_LFO_LIGHT,
		FILTER_AP_LIGHT,
		FILTER_NR_LIGHT,
		FREQUENCY_MOD_TYPE_LOG_LIGHT,
		FREQUENCY_MOD_TYPE_CONSTANT_LIGHT,
		FREQUENCY_MOD_TYPE_ALT_LOG_LIGHT,
		FREQUENCY_MOD_TYPE_ALT_CONSTANT_LIGHT,
		NUM_LIGHTS
	};
	enum FilterModes {
		FILTER_ALLPASS,
		FILTER_NOTCH,
		NUM_FILTER_MODES
	};
	enum LFOWaveShapes {
		SIN_LFO,
		TRI_LFO,
		SAW_LFO,
		SQR_LFO,
		NUM_WAVE_SHAPES
	};
	enum FrequencyModTypes {
		LOGARITHMIC_FREQ_MOD,
		CONSTANT_FREQ_MOD,
		ALTERNATING_LOGARITHMIC_FREQ_MOD,
		ALTERNATING_CONSTANT_FREQ_MOD,
		NUM_FREQUENCY_MOD_TYPES
	};


	//Stages
	int stageCount[NUM_STAGE_SETUPS] = {4,8,12};
	
	float basefreq[NUM_STAGE_SETUPS][MAX_STAGES] = {{1.5 / 12, 19.5 / 12, 35 / 12, 50 / 12},
	                                                {0.6 / 12, 1.5 / 12, 7 / 12, 19.5 / 12, 27 / 12, 35 / 12, 43 / 12, 50 / 12},
													{0.6 / 12, 1.5 / 12, 2.2 / 12, 7 / 12, 19.5 / 12, 23 / 12, 27 / 12, 35 / 12, 43 / 12, 47 / 12, 50 / 12, 55 / 12}};
	float basespan[NUM_FREQUENCY_MOD_TYPES][NUM_STAGE_SETUPS][MAX_STAGES] = {
													//Logarithmic
													{{2.0, 1.5, 1.0, 0.5},
	                                                {2.0, 2.0, 1.75, 1.5, 1.25, 1.0, 0.75, 0.5},
													{2.0, 2.0, 1.8, 1.75, 1.5, 1.25, 1.1, 1.0, 0.75, 0.6, 0.5, 0.4}},
													//Constant
													{{2.0, 2.0, 2.0, 2.0},
	                                                {2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0},
													{2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0}},
													//Alternating Logarithmic
													{{2.0, -1.5, 1.0, -0.5},
	                                                {2.0, -2.0, 1.75, -1.5, 1.25, -1.0, 0.75, -0.5},
													{2.0, -2.0, 1.8, -1.75, 1.5, -1.25, 1.1, -1.0, 0.75, -0.6, 0.5, -0.4}},
													//Alternating Constant
													{{2.0, -2.0, 2.0, -2.0},
	                                                {2.0, -2.0, 2.0, -2.0, 2.0, -2.0, 2.0, -2.0},
													{2.0, -2.0, 2.0, -2.0, 2.0, -2.0, 2.0, -2.0, 2.0, -2.0, 2.0, -2.0}} };


	Biquad* pFilter[MAX_STAGES][MAX_CHANNELS];
	LowFrequencyOscillator lfo;

	int nunberOfStagesIndex = 0;
	int numberOfStages = 4;
	int filterMode = 0;
	int lastFilterMode = -1;

	int waveShape = 0;

	float directInput[MAX_CHANNELS] = {0.0,0.0};
	float feedbackOut[MAX_CHANNELS];
	float feedbackIn[MAX_CHANNELS] = {0.0,0.0};

	float lastFc[MAX_STAGES][MAX_CHANNELS];

	float lastResonance = 0.0;

	float centerFrequency = 0.0;
	float modDepth = 0.0;
	float resonance = 0.0;

	int frequencyModType = 0;

	dsp::SchmittTrigger freqModTypeTrigger,filterModeTrigger,waveShapeTrigger,numStagesTrigger;

    //percenatages
	float frequencyPercentage = 0;
	float stereoPhasePercentage = 0;
	float depthPercentage = 0;
	float feedbackAmountPercentage = 0;
	float centerFrequencyPercentage = 0;
	float frequencySpanPercentage = 0;
	float resonancePercentage = 0;
	float mixPercentage = 0;


	JustAPhaser() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(FREQUENCY_PARAM, -8.0, 3.0, 0.0,"Mod Frequency", " Hz", 2, 1);
	
		configParam(DEPTH_PARAM, 0, 1.0, 0.5,"Mod Depth", "%",0,100);
		configParam(FEEDBACK_AMOUNT_PARAM, -1.0, 1.0, 0.0,"Feedback","%",0,100);
		configParam(CENTER_FREQUENCY_PARAM, 4.0, 14.0, 8.0,"Center Frequency", " Hz", 2, 1);
		configParam(FREQUENCY_SPAN_PARAM, 0.01, 1.0, 1.0,"Frequency Span", "%",0,100);
		configParam(RESONANCE_PARAM, 0.5, 5, 0.707,"Resonance");
		configParam(STEREO_PHASE_PARAM, 0, 0.99999, 0.25,"Stereo Phase Separation","°",0,360);
		configParam(MIX_PARAM, 0, 1, 0.5,"Mix","%",0,100);

		configButton(NUMBER_STAGES_PARAM,"# of Stages");
		configButton(WAVESHAPE_PARAM,"LFO Wave");
		configButton(FREQUENCY_MOD_TYPE_PARAM,"Modulation Type");
		configButton(FILTER_MODE_PARAM,"Filter Type");

		configInput(IN_L_IN, "Left");
		configInput(IN_R_IN, "Right");
		configInput(FB_IN_L_IN, "Left Feedback Return");
		configInput(FB_IN_R_IN, "Right Feedback Return");
		configInput(FREQUENCY_INPUT, "LFO Frequency");
		configInput(DEPTH_INPUT, "LFO Depth");
		configInput(FEEDBACK_INPUT, "Feedback");
		configInput(CENTER_FREQUENCY_INPUT, "Center Frequency");
		configInput(FREQUENCY_SPAN_INPUT, "Frequency Span");
		configInput(RESONANCE_INPUT, "Resonance");
		configInput(STEREO_PHASE_INPUT, "Stereo Phase Separation");
		configInput(EXTERNAL_MOD_INPUT_L, "External Modulation L");
		configInput(EXTERNAL_MOD_INPUT_R, "External Modulation R");
		configInput(MIX_CV_INPUT, "Mix");

		configOutput(FB_OUT_L_OUTPUT, "Left Feedback Send");
		configOutput(FB_OUT_R_OUTPUT, "Right Feedback Send");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");



		for(int i=0; i<MAX_STAGES; i++) {
			for(int c=0;c<MAX_CHANNELS;c++) {
				pFilter[i][c] = new Biquad(bq_type_allpass, 0.5 , 0.707, 0);
			}
		};
	}

	double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

	//void process(const ProcessArgs &args) override;
	//};

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "nunberOfStagesIndex", json_integer(nunberOfStagesIndex));
		json_object_set_new(rootJ, "numberOfStages", json_integer(numberOfStages));
		json_object_set_new(rootJ, "filterMode", json_integer(filterMode));
		json_object_set_new(rootJ, "waveShape", json_integer(waveShape));
		json_object_set_new(rootJ, "frequencyModType", json_integer(frequencyModType));



		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

		json_t *ctNsi = json_object_get(rootJ, "nunberOfStagesIndex");
		if (ctNsi)
			nunberOfStagesIndex = json_integer_value(ctNsi);
		json_t *ctNs = json_object_get(rootJ, "numberOfStages");
		if (ctNs)
			numberOfStages = json_integer_value(ctNs);
		json_t *ctFm = json_object_get(rootJ, "filterMode");
		if (ctFm)
			filterMode = json_integer_value(ctFm);
		json_t *ctWs = json_object_get(rootJ, "waveShape");
		if (ctWs)
			waveShape = json_integer_value(ctWs);
		json_t *ctFmt = json_object_get(rootJ, "frequencyModType");
		if (ctFmt)
			frequencyModType = json_integer_value(ctFmt);

	}

	void process(const ProcessArgs &args) override {

		float sampleRate = args.sampleRate;

		if (numStagesTrigger.process(params[NUMBER_STAGES_PARAM].getValue())) {
			nunberOfStagesIndex = (nunberOfStagesIndex + 1) % NUM_STAGE_SETUPS;
			numberOfStages = stageCount[nunberOfStagesIndex];
		}
		lights[STAGES_4_LIGHT].value = numberOfStages == 4;
		lights[STAGES_8_LIGHT].value = numberOfStages == 8;
		lights[STAGES_12_LIGHT].value = numberOfStages == 12;

		if (filterModeTrigger.process(params[FILTER_MODE_PARAM].getValue())) {
			filterMode = (filterMode + 1) % NUM_FILTER_MODES;
		}
		lights[FILTER_AP_LIGHT].value = filterMode == FILTER_ALLPASS;
		lights[FILTER_NR_LIGHT].value = filterMode == FILTER_NOTCH;

		if (waveShapeTrigger.process(params[WAVESHAPE_PARAM].getValue())) {
			waveShape = (waveShape + 1) % NUM_WAVE_SHAPES;
		}
		lights[SIN_LFO_LIGHT].value = waveShape == SIN_LFO;
		lights[TRI_LFO_LIGHT].value = waveShape == TRI_LFO;
		lights[SAW_LFO_LIGHT].value = waveShape == SAW_LFO;
		lights[SQR_LFO_LIGHT].value = waveShape == SQR_LFO;


		if (freqModTypeTrigger.process(params[FREQUENCY_MOD_TYPE_PARAM].getValue())) {
			frequencyModType = (frequencyModType + 1) % NUM_FREQUENCY_MOD_TYPES;
		}
		lights[FREQUENCY_MOD_TYPE_LOG_LIGHT].value = frequencyModType == LOGARITHMIC_FREQ_MOD;
		lights[FREQUENCY_MOD_TYPE_CONSTANT_LIGHT].value = frequencyModType == CONSTANT_FREQ_MOD;
		lights[FREQUENCY_MOD_TYPE_ALT_LOG_LIGHT].value = frequencyModType == ALTERNATING_LOGARITHMIC_FREQ_MOD;
		lights[FREQUENCY_MOD_TYPE_ALT_CONSTANT_LIGHT].value = frequencyModType == ALTERNATING_CONSTANT_FREQ_MOD;

		float feedbackAmount = clamp(params[FEEDBACK_AMOUNT_PARAM].getValue() + (inputs[FEEDBACK_INPUT].getVoltage() / 10.0 ),-1.0f,1.0f);
		feedbackAmountPercentage = feedbackAmount;

		// directInput[0] = inputs[IN_L_IN].getVoltage()/5.0 + (feedbackIn[0] * feedbackAmount);
		// if(inputs[IN_R_IN].active) {
		// 	directInput[1] = inputs[IN_R_IN].getVoltage()/5.0 + (feedbackIn[1] * feedbackAmount);
		// } else {
		// 	directInput[1] = inputs[IN_L_IN].getVoltage()/5.0 + (feedbackIn[1] * feedbackAmount);
		// }

		directInput[0] = inputs[IN_L_IN].getVoltage()/5.0;
		if(inputs[IN_R_IN].active) {
			directInput[1] = inputs[IN_R_IN].getVoltage()/5.0;
		} else {
			directInput[1] = inputs[IN_L_IN].getVoltage()/5.0;
		}

		float lfoFrequency = clamp(params[FREQUENCY_PARAM].getValue() + (inputs[FREQUENCY_INPUT].getVoltage()),-8.0f,3.0f); 
		frequencyPercentage = (lfoFrequency + 8.0) / 11.0;
		lfo.setPitch(lfoFrequency);
		lfo.step(1.0/sampleRate);

		if(filterMode != lastFilterMode) {
			for(int i=0; i<numberOfStages; i++) {
				for(int c=0;c<MAX_CHANNELS;c++) {
					pFilter[i][c]->setType(filterMode ? bq_type_allpass : bq_type_notch );
				}
			};
			lastFilterMode = filterMode;
			lastResonance = -1.0; // force resonance to get reset as well
		}

		float resonance = clamp(params[RESONANCE_PARAM].getValue() + (inputs[RESONANCE_INPUT].getVoltage() / 2.0),0.5f,5.0f);
		if(resonance != lastResonance) {
			resonancePercentage = resonance / 5.0;
			for(int i=0; i<numberOfStages; i++) {
				for(int c=0;c<MAX_CHANNELS;c++) {
					pFilter[i][c]->setQ(resonance);
				}
			};
			lastResonance = resonance;
		}

		float centerFrequency = clamp(params[CENTER_FREQUENCY_PARAM].getValue() + (inputs[CENTER_FREQUENCY_INPUT].getVoltage()),4.0f,14.0f);
		centerFrequencyPercentage = (centerFrequency - 4.0)/ 10.0;
		float frequencySpan = clamp(params[FREQUENCY_SPAN_PARAM].getValue() + (inputs[FREQUENCY_SPAN_INPUT].getVoltage() / 10.0),0.0f,1.0f);
		frequencySpanPercentage = frequencySpan;

		float modValue = clamp(params[DEPTH_PARAM].getValue() + inputs[DEPTH_INPUT].getVoltage() / 10.0,0.0f,1.0f);
		depthPercentage = modValue;

		float steroPhase = clamp(params[STEREO_PHASE_PARAM].getValue() + (inputs[STEREO_PHASE_INPUT].getVoltage() / 10.0),0.0f,1.0f);
		stereoPhasePercentage = steroPhase;
		for(int c=0;c<MAX_CHANNELS;c++) {
			float lfoValue = 0.0;
			if (inputs[EXTERNAL_MOD_INPUT_L+c].active) {
				lfoValue = (inputs[EXTERNAL_MOD_INPUT_L+c].getVoltage() / 5.0) - 1.0;
			} else {
				switch(waveShape) {
					case SIN_LFO :			
						lfoValue = lfo.sin(c == 0 ? 0.0 : steroPhase);
						break;
					case TRI_LFO :			
						lfoValue = lfo.tri(c == 0 ? 0.0 : steroPhase);
						break;
					case SAW_LFO :			
						lfoValue = lfo.saw(c == 0 ? 0.0 : steroPhase);
						break;
					case SQR_LFO :			
						lfoValue = lfo.sqr(c == 0 ? 0.0 : steroPhase);
						break;
				}
			}
			for(int i=0; i<numberOfStages; i++) {

				//2.0 is temporary
				float Fc = centerFrequency - 2.0 + (basefreq[nunberOfStagesIndex][i] * frequencySpan) + (modValue * basespan[frequencyModType][nunberOfStagesIndex][i] * lfoValue);
				if(fabsf(lastFc[i][c] - Fc) > 1E-2) {
					pFilter[i][c]->setFc(clamp(pow(2,Fc),20.0f,15000.0f)/ sampleRate);
					lastFc[i][c] = Fc;
				}
			};
		}

		float mix = clamp(params[MIX_PARAM].getValue() + (inputs[MIX_CV_INPUT].getVoltage() / 10.0),0.0f,1.0f);
		mixPercentage = mix;
		
		for(int c=0; c<MAX_CHANNELS;c++) {
			float phaseOut = directInput[c]+ (feedbackIn[c] * feedbackAmount);			
			for(int i=0;i<numberOfStages;i++) {
				phaseOut = pFilter[i][c]->process(phaseOut);
			}
			feedbackOut[c] = phaseOut;
			outputs[FB_OUT_L_OUTPUT + c].setVoltage(phaseOut * 5);
			
			if(inputs[FB_IN_L_IN+c].active) 
				feedbackIn[c] = inputs[FB_IN_L_IN+c].getVoltage() / 5.0;
			else 
				feedbackIn[c] = feedbackOut[c];

			outputs[OUT_L_OUTPUT + c].setVoltage(lerp(directInput[c],phaseOut,mix) * 5.0);
		}
		
	}
};


struct JustAPhaserWidget : ModuleWidget {
	JustAPhaserWidget(JustAPhaser *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/JustAPhaser.svg")));
		


		addParam(createParam<TL1105>(Vec(15, 31), module, JustAPhaser::NUMBER_STAGES_PARAM));
		addChild(createLight<SmallLight<BlueLight>>(Vec(45, 35), module, JustAPhaser::STAGES_4_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(65, 35), module, JustAPhaser::STAGES_8_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(85, 35), module, JustAPhaser::STAGES_12_LIGHT));

		
		ParamWidget* frequencyParam = createParam<RoundSmallFWKnob>(Vec(10, 92), module, JustAPhaser::FREQUENCY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(frequencyParam)->percentage = &module->frequencyPercentage;
		}
		addParam(frequencyParam);							
		ParamWidget* stereoPhaseParam = createParam<RoundSmallFWKnob>(Vec(10, 132), module, JustAPhaser::STEREO_PHASE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(stereoPhaseParam)->percentage = &module->stereoPhasePercentage;
		}
		addParam(stereoPhaseParam);							
		ParamWidget* depthParam = createParam<RoundSmallFWKnob>(Vec(10, 172), module, JustAPhaser::DEPTH_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(depthParam)->percentage = &module->depthPercentage;
		}
		addParam(depthParam);							
		ParamWidget* feedbackAmountParam = createParam<RoundSmallFWKnob>(Vec(10, 212), module, JustAPhaser::FEEDBACK_AMOUNT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(feedbackAmountParam)->percentage = &module->feedbackAmountPercentage;
			dynamic_cast<RoundSmallFWKnob*>(feedbackAmountParam)->biDirectional = true;
		}
		addParam(feedbackAmountParam);							
		ParamWidget* centerFrequencyParam = createParam<RoundSmallFWKnob>(Vec(10, 252), module, JustAPhaser::CENTER_FREQUENCY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(centerFrequencyParam)->percentage = &module->centerFrequencyPercentage;
		}
		addParam(centerFrequencyParam);							
		ParamWidget* frequencySpanParam = createParam<RoundSmallFWKnob>(Vec(100, 252), module, JustAPhaser::FREQUENCY_SPAN_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(frequencySpanParam)->percentage = &module->frequencySpanPercentage;
		}
		addParam(frequencySpanParam);							
		ParamWidget* resonanceParam = createParam<RoundSmallFWKnob>(Vec(10, 292), module, JustAPhaser::RESONANCE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(resonanceParam)->percentage = &module->resonancePercentage;
		}
		addParam(resonanceParam);							

		ParamWidget* mixParam = createParam<RoundSmallFWKnob>(Vec(53, 338), module, JustAPhaser::MIX_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(mixParam)->percentage = &module->mixPercentage;
		}
		addParam(mixParam);							


		addParam(createParam<TL1105>(Vec(100, 294), module, JustAPhaser::FILTER_MODE_PARAM));
		addChild(createLight<SmallLight<BlueLight>>(Vec(120, 300), module, JustAPhaser::FILTER_AP_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(136, 300), module, JustAPhaser::FILTER_NR_LIGHT));

		addParam(createParam<TL1105>(Vec(15, 60), module, JustAPhaser::WAVESHAPE_PARAM));
		addChild(createLight<SmallLight<BlueLight>>(Vec(45, 64), module, JustAPhaser::SIN_LFO_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(65, 64), module, JustAPhaser::TRI_LFO_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(85, 64), module, JustAPhaser::SAW_LFO_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(105, 64), module, JustAPhaser::SQR_LFO_LIGHT));
	
		addParam(createParam<TL1105>(Vec(65, 174), module, JustAPhaser::FREQUENCY_MOD_TYPE_PARAM));
		addChild(createLight<SmallLight<BlueLight>>(Vec(85, 180), module, JustAPhaser::FREQUENCY_MOD_TYPE_LOG_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(100, 180), module, JustAPhaser::FREQUENCY_MOD_TYPE_CONSTANT_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(115, 180), module, JustAPhaser::FREQUENCY_MOD_TYPE_ALT_LOG_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(130, 180), module, JustAPhaser::FREQUENCY_MOD_TYPE_ALT_CONSTANT_LIGHT));


		addInput(createInput<FWPortInSmall>(Vec(38, 94), module, JustAPhaser::FREQUENCY_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 134), module, JustAPhaser::STEREO_PHASE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 174), module, JustAPhaser::DEPTH_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 214), module, JustAPhaser::FEEDBACK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 254), module, JustAPhaser::CENTER_FREQUENCY_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(128, 254), module, JustAPhaser::FREQUENCY_SPAN_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(38, 294), module, JustAPhaser::RESONANCE_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(85, 94), module, JustAPhaser::EXTERNAL_MOD_INPUT_L));
		addInput(createInput<FWPortInSmall>(Vec(105, 94), module, JustAPhaser::EXTERNAL_MOD_INPUT_R));

		addInput(createInput<FWPortInSmall>(Vec(110, 214), module, JustAPhaser::FB_IN_L_IN));
		addInput(createInput<FWPortInSmall>(Vec(130, 214), module, JustAPhaser::FB_IN_R_IN));

		addOutput(createOutput<FWPortOutSmall>(Vec(65, 214), module, JustAPhaser::FB_OUT_L_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(85, 214), module, JustAPhaser::FB_OUT_R_OUTPUT));

		addInput(createInput<FWPortInSmall>(Vec(82, 340), module, JustAPhaser::MIX_CV_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(8, 340), module, JustAPhaser::IN_L_IN));
		addInput(createInput<FWPortInSmall>(Vec(28, 340), module, JustAPhaser::IN_R_IN));

		addOutput(createOutput<FWPortOutSmall>(Vec(106, 340), module, JustAPhaser::OUT_L_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(126, 340), module, JustAPhaser::OUT_R_OUTPUT));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};

Model *modelJustAPhaser = createModel<JustAPhaser, JustAPhaserWidget>("JustAPhaser");
