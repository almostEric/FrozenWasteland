//NOTE Because I am tipsy
//BPM based modulation of phase. Not sure if it means anything other than 2 BPM LFOS :)

#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define DISPLAY_SIZE 68

struct BPMLFO2 : Module {
	enum ParamIds {
		MULTIPLIER_PARAM,
		MULTIPLIER_CV_ATTENUVERTER_PARAM,
		DIVISION_PARAM,
		DIVISION_CV_ATTENUVERTER_PARAM,
		WAVESLOPE_PARAM,
		WAVESLOPE_CV_ATTENUVERTER_PARAM,
		SKEW_PARAM,
		SKEW_CV_ATTENUVERTER_PARAM,
		PHASE_PARAM,
		PHASE_CV_ATTENUVERTER_PARAM,
		QUANTIZE_PHASE_PARAM,
		OFFSET_PARAM,	
		WAVESHAPE_PARAM,
		HOLD_CLOCK_BEHAVIOR_PARAM,
		HOLD_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		MULTIPLIER_INPUT,
		DIVISION_INPUT,
		WAVESLOPE_INPUT,
		SKEW_INPUT,
		PHASE_INPUT,
		RESET_INPUT,
		HOLD_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LFO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		QUANTIZE_PHASE_LIGHT,
		HOLD_LIGHT,
		NUM_LIGHTS
	};	
	enum Waveshapes {
		SKEWSAW_WAV,
		SQUARE_WAV
	};

	

	struct LowFrequencyOscillator {
		float basePhase = 0.0;
		float phase = 0.0;
		float freq = 1.0;
		float pw = 0.5;
		float skew = 0.5; // Triangle
		float waveSlope = 0.0; //Original (1 is sin)
		bool offset = false;

		float lerp(float v0, float v1, float t) {
			return (1 - t) * v0 + t * v1;
		}

		void setPitch(float pitch) {
			pitch = fminf(pitch, 8.0);
			freq = powf(2.0, pitch);
		}
		void setFrequency(float frequency) {
			freq = frequency;
		}
		void setPulseWidth(float pw_) {
			const float pwMin = 0.01;
			pw = clamp(pw_, pwMin, 1.0f - pwMin);
		}
		
		void setBasePhase(float initialPhase) {
			//Apply change, then remember
			phase += initialPhase - basePhase;
			if (phase >= 1.0)
				phase -= 1.0;
			else if (phase < 0)
				phase += 1.0;	
			basePhase = initialPhase;
		}

		void hardReset()
		{
			phase = basePhase;
		}

		void step(float dt) {
			float deltaPhase = fminf(freq * dt, 0.5);
			phase += deltaPhase;
			if (phase >= 1.0)
				phase -= 1.0;
		}

		float skewsaw(float x) {
			//x = dsp::eucmod(x,1.0);
			x = std::fabs(std::fmod(x ,1.0f));

			float inverseSkew = 1 - skew;
			float originalWave;
			if (skew == 0 && x == 0) //Avoid /0 error
				originalWave = 2;
			if (x <= skew)
				originalWave = 2.0 * (1- (-1 / skew * x + 1));
			else
				originalWave = 2.0 * (1-(1 / inverseSkew * (x - skew)));
			return originalWave;
		}
		
		float skewsaw() {
			if (offset)
				return lerp(skewsaw(phase),sin(),waveSlope);
			
			return lerp(skewsaw(phase) - 1 ,sin(),waveSlope);skewsaw(phase) ; //Going to keep same phase for now				
		}


		float sin() {
			if (offset)
				return 1.0 - cosf(2*M_PI * (phase-skew*0));
			
			return sinf(2*M_PI * (phase-skew*0));
		}

		float sqr() {
			float sqr = (phase < pw) ? 1.0 : -1.0;
			sqr = offset ? sqr + 1.0 : sqr;

			float result = lerp(sqr,sin(),waveSlope);
			return result;
		}
		
		float progress() {
			return phase;
		}
	};


	LowFrequencyOscillator oscillator,displayOscillator;
	dsp::SchmittTrigger clockTrigger,resetTrigger,holdTrigger;
	float multiplier = 1;
	float division = 1;
	float timeElapsed = 0.0;
	float duration = 0;
	float waveshape = 0;
	float waveSlope = 0.0;
	float skew = 0.5;
	float initialPhase = 0.0;
	bool holding = false;
	bool firstClockReceived = false;
	bool secondClockReceived = false;
	bool phase_quantized = false;
	float lfoOutputValue = 0.0;

	float lastWaveShape = -1;
	float lastWaveSlope = -1;
	float lastSkew = -1;
	float waveValues[DISPLAY_SIZE] = {};

	dsp::SchmittTrigger quantizePhaseTrigger;


	BPMLFO2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MULTIPLIER_PARAM, 1.0, 128, 1.0,"Multiplier");
		configParam(MULTIPLIER_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Multiplier CV Attenuation","%",0,100);
		configParam(DIVISION_PARAM, 1.0, 128, 1.0,"Divisor");
		configParam(DIVISION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Division CV Attenuation","%",0,100);
		configParam(WAVESLOPE_PARAM, 0.0, 1.0, 0.0,"Wave Slope","%",0,100);
		configParam(WAVESLOPE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Wave Slope CV Attenuation","%",0,100);
		configParam(SKEW_PARAM, 0.0, 1.0, 0.5,"Skew","%",0,100);
		configParam(SKEW_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Skew CV Attenuation","%",0,100);
		configParam(PHASE_PARAM, 0.0, 0.9999, 0.0,"Phase","°",0,360);
		configParam(PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Phase CV Attenuation","%",0,100);
		configParam(QUANTIZE_PHASE_PARAM, 0.0, 1.0, 0.0);
		configParam(OFFSET_PARAM, 0.0, 1.0, 1.0);
		configParam(WAVESHAPE_PARAM, 0.0, 1.0, 0.0);
		configParam(HOLD_CLOCK_BEHAVIOR_PARAM, 0.0, 1.0, 1.0);
		configParam(HOLD_MODE_PARAM, 0.0, 1.0, 1.0);
	}
	void process(const ProcessArgs &args) override;

	// void reset() override {
	// 	division = 0;
	// }

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void BPMLFO2::process(const ProcessArgs &args) {

    timeElapsed += 1.0 / args.sampleRate;
	if(inputs[CLOCK_INPUT].isConnected()) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(firstClockReceived) {
				duration = timeElapsed;
				secondClockReceived = true;
			}
			timeElapsed = 0;
			firstClockReceived = true;			
		} else if(secondClockReceived && timeElapsed > duration) {  //allow absense of second clock to affect duration
			duration = timeElapsed;				
		}	
	} else {
		duration = 0.0f;
		firstClockReceived = false;
		secondClockReceived = false;
	}
	
	
	
	multiplier = params[MULTIPLIER_PARAM].getValue();
	if(inputs[MULTIPLIER_INPUT].isConnected()) {
		multiplier +=(inputs[MULTIPLIER_INPUT].getVoltage() * params[MULTIPLIER_CV_ATTENUVERTER_PARAM].getValue() * 12.8);
	}
	multiplier = clamp(multiplier,1.0f,128.0f);

	division = params[DIVISION_PARAM].getValue();
	if(inputs[DIVISION_INPUT].isConnected()) {
		division +=(inputs[DIVISION_INPUT].getVoltage() * params[DIVISION_CV_ATTENUVERTER_PARAM].getValue() * 12.8);
	}
	division = clamp(division,1.0f,128.0f);

	waveshape = params[WAVESHAPE_PARAM].getValue();

	waveSlope = params[WAVESLOPE_PARAM].getValue();
	if(inputs[WAVESLOPE_INPUT].isConnected()) {
		waveSlope +=inputs[WAVESLOPE_INPUT].getVoltage() / 10 * params[WAVESLOPE_CV_ATTENUVERTER_PARAM].getValue();
	}
	waveSlope = clamp(waveSlope,0.0f,1.0f);

	skew = params[SKEW_PARAM].getValue();
	if(inputs[SKEW_INPUT].isConnected()) {
		skew +=inputs[SKEW_INPUT].getVoltage() / 10 * params[SKEW_CV_ATTENUVERTER_PARAM].getValue();
	}
	skew = clamp(skew,0.0f,1.0f);

	if (quantizePhaseTrigger.process(params[QUANTIZE_PHASE_PARAM].getValue())) {
		phase_quantized = !phase_quantized;
	}
	lights[QUANTIZE_PHASE_LIGHT].value = phase_quantized;

	initialPhase = params[PHASE_PARAM].getValue();
	if(inputs[PHASE_INPUT].isConnected()) {
		initialPhase += (inputs[PHASE_INPUT].getVoltage() / 10 * params[PHASE_CV_ATTENUVERTER_PARAM].getValue());
	}
	if (initialPhase >= 1.0)
		initialPhase -= 1.0;
	else if (initialPhase < 0)
		initialPhase += 1.0;	
	if(phase_quantized) // Limit to 90 degree increments
		initialPhase = std::round(initialPhase * 4.0f) / 4.0f;
	
	
	oscillator.offset = (params[OFFSET_PARAM].getValue() > 0.0);
	oscillator.setBasePhase(initialPhase);
	oscillator.waveSlope = waveSlope;
	oscillator.skew = skew;
	oscillator.setPulseWidth(skew);

	//Recalcluate display waveform if something changed
	if(lastWaveShape != waveshape || lastWaveSlope != waveSlope || lastSkew != skew) {
		displayOscillator.waveSlope = waveSlope;
		displayOscillator.skew = skew;
		displayOscillator.setPulseWidth(skew);
		displayOscillator.setFrequency(1.0);
		displayOscillator.phase = 0;
		for(int i=0;i<DISPLAY_SIZE;i++) {
			waveValues[i] = (waveshape == SKEWSAW_WAV ? displayOscillator.skewsaw() : displayOscillator.sqr()) * DISPLAY_SIZE / 2;
			displayOscillator.step(1.0 / DISPLAY_SIZE);			
		}
		lastWaveShape = waveshape;
		lastWaveSlope = waveSlope;
		lastSkew = skew;
	}

	if(duration != 0) {
		oscillator.setFrequency(1.0 / (duration / multiplier * division));
	}
	else {
		oscillator.setFrequency(0);
	}

	if(inputs[RESET_INPUT].isConnected()) {
		if(resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			oscillator.hardReset();
		}		
	} 

	if(inputs[HOLD_INPUT].isConnected()) {
		if(params[HOLD_MODE_PARAM].getValue() == 1.0) { //Latched is default		
			if(holdTrigger.process(inputs[HOLD_INPUT].getVoltage())) {
				holding = !holding;
			}		
		} else {
			holding = inputs[HOLD_INPUT].getVoltage() >= 1; 			
		}
		lights[HOLD_LIGHT].value = holding;
	} 

    if(!holding || (holding && params[HOLD_CLOCK_BEHAVIOR_PARAM].getValue() == 0.0)) {
    	oscillator.step(1.0 / args.sampleRate);
    }

	if(!holding) {
		if(waveshape == SKEWSAW_WAV)
			lfoOutputValue = 5.0 * oscillator.skewsaw();
		else
			lfoOutputValue = 5.0 * oscillator.sqr();
	}


	outputs[LFO_OUTPUT].setVoltage(lfoOutputValue);
}

struct BPMLFO2ProgressDisplay : TransparentWidget {
	BPMLFO2 *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	BPMLFO2ProgressDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}

	void drawProgress(const DrawArgs &args, int waveshape, float skew, float waveslope, float phase) 
	{
		// float inverseSkew = 1 - skew;
		// float y;
        // if (skew == 0 && phase == 0) //Avoid /0 error
        //     y = 72.0;
        // if (phase <= skew)
        //     y = 72.0 * (1- (-1 / skew * phase + 1));
        // else
        //     y = 72.0 * (1-(1 / inverseSkew * (phase - skew)));

		// Draw indicator
		//nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));	
		nvgStrokeWidth(args.vg, 1.5);

		nvgBeginPath(args.vg);
		//nvgMoveTo(args.vg,140,177 - module->waveValues[0]);
		for(int i=0;i<phase * DISPLAY_SIZE;i++) {
			nvgMoveTo(args.vg,140 + i,177 - module->waveValues[i]);
			nvgLineTo(args.vg,140 + i, module->waveValues[i] > 0 ? 211 : 211 );
			
		}
		//nvgLineTo(args.vg,140 + phase * 68,177 - module->waveValues[0]);
		//nvgClosePath(args.vg);
		nvgStroke(args.vg);		
		//nvgFill(args.vg);

		// if(waveshape == BPMLFO2::SKEWSAW_WAV)
		// {
		// 	nvgBeginPath(args.vg);
		// 	nvgMoveTo(args.vg,140,213);
		// 	if(phase > skew) {
		// 		nvgLineTo(args.vg,140 + (skew * 68),141);
		// 	}
		// 	nvgLineTo(args.vg,140 + (phase * 68),213-y);
		// 	nvgLineTo(args.vg,140 + (phase * 68),213);
		// 	nvgClosePath(args.vg);
		// 	nvgFill(args.vg);
		// } else 
		// {
		// 	float endpoint = fminf(phase,skew);
		// 	nvgBeginPath(args.vg);
		// 	nvgMoveTo(args.vg,140,177);
		// 	nvgRect(args.vg,140,177,endpoint*68,36.0);
		// 	nvgClosePath(args.vg);
		// 	nvgFill(args.vg);
		// 	if(phase > skew) {
		// 		nvgBeginPath(args.vg);
		// 		nvgMoveTo(args.vg,140 + (skew * 68),141);
		// 		nvgRect(args.vg,140 + (skew * 68),141,(phase-skew)*68,36.0);
		// 		nvgClosePath(args.vg);
		// 		nvgFill(args.vg);
		// 	}

		// }
	}

	void drawWaveShape(const DrawArgs &args, int waveshape, float skew, float waveslope) 
	{
		// Draw wave shape
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));	
		nvgStrokeWidth(args.vg, 2.0);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,140,177 - module->waveValues[0]);
		for(int i=1;i<DISPLAY_SIZE;i++) {
			nvgLineTo(args.vg,140 + i,177 - module->waveValues[i]);
		}
		//nvgLineTo(args.vg,208,213);
		//nvgClosePath(args.vg);	

		// if(waveshape == BPMLFO2::SKEWSAW_WAV) {						
		// 	nvgBeginPath(args.vg);
		// 	nvgMoveTo(args.vg,140,213);
		// 	nvgLineTo(args.vg,140 + (skew * 68),141);
		// 	nvgLineTo(args.vg,208,213);
		// 	nvgClosePath(args.vg);			
		// } else 
		// {
		// 	nvgBeginPath(args.vg);
		// 	nvgMoveTo(args.vg,140,213);
		// 	nvgLineTo(args.vg,140 + (skew * 68),213);
		// 	nvgLineTo(args.vg,140 + (skew * 68),141);
		// 	nvgLineTo(args.vg,208,141);
		// 	//nvgClosePath(args.vg);			
		// }
		nvgStroke(args.vg);		
	}

	void drawMultiplier(const DrawArgs &args, Vec pos, float multiplier) {
		nvgFontSize(args.vg, 28);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 3.0f", multiplier);
		nvgText(args.vg, pos.x + 15, pos.y, text, NULL);
	}

	void drawDivision(const DrawArgs &args, Vec pos, float division) {
		nvgFontSize(args.vg, 28);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 3.0f", division);
		nvgText(args.vg, pos.x + 130, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;	
		drawWaveShape(args,module->waveshape, module->skew, module->waveSlope);
		drawProgress(args,module->waveshape, module->skew, module->waveSlope, module->oscillator.progress());
		drawMultiplier(args, Vec(0, box.size.y - 153), module->multiplier);
		drawDivision(args, Vec(0, box.size.y - 153), module->division);
	}
};

struct BPMLFO2Widget : ModuleWidget {
	BPMLFO2Widget(BPMLFO2 *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BPMLFO2.svg")));
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			BPMLFO2ProgressDisplay *display = new BPMLFO2ProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}

		addParam(createParam<RoundLargeFWSnapKnob>(Vec(22, 78), module, BPMLFO2::MULTIPLIER_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(62, 108), module, BPMLFO2::MULTIPLIER_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundLargeFWSnapKnob>(Vec(140, 78), module, BPMLFO2::DIVISION_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(181, 108), module, BPMLFO2::DIVISION_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(5, 144), module, BPMLFO2::WAVESLOPE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(7, 203), module, BPMLFO2::WAVESLOPE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(50, 144), module, BPMLFO2::SKEW_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(53, 203), module, BPMLFO2::SKEW_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(100, 144), module, BPMLFO2::PHASE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(103, 203), module, BPMLFO2::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(83, 164), module, BPMLFO2::QUANTIZE_PHASE_PARAM));
		
		
		addParam(createParam<CKSS>(Vec(30, 242), module, BPMLFO2::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(73, 242), module, BPMLFO2::WAVESHAPE_PARAM));
		addParam(createParam<CKSS>(Vec(119, 242), module, BPMLFO2::HOLD_CLOCK_BEHAVIOR_PARAM));
		addParam(createParam<CKSS>(Vec(169, 242), module, BPMLFO2::HOLD_MODE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(62, 81), module, BPMLFO2::MULTIPLIER_INPUT));
		addInput(createInput<PJ301MPort>(Vec(181, 81), module, BPMLFO2::DIVISION_INPUT));
		addInput(createInput<PJ301MPort>(Vec(7, 175), module, BPMLFO2::WAVESLOPE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(53, 175), module, BPMLFO2::SKEW_INPUT));
		addInput(createInput<PJ301MPort>(Vec(103, 175), module, BPMLFO2::PHASE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(45, 292), module, BPMLFO2::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(100, 292), module, BPMLFO2::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(149, 292), module, BPMLFO2::HOLD_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(100, 336), module, BPMLFO2::LFO_OUTPUT));

		addChild(createLight<MediumLight<BlueLight>>(Vec(87, 168), module, BPMLFO2::QUANTIZE_PHASE_LIGHT));
		addChild(createLight<LargeLight<RedLight>>(Vec(177, 296), module, BPMLFO2::HOLD_LIGHT));
	}
};



Model *modelBPMLFO2 = createModel<BPMLFO2, BPMLFO2Widget>("BPMLFO2");
