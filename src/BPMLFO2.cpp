//NOTE Because I am tipsy
//BPM based modulation of phase. Not sure if it means anything other than 2 BPM LFOS :)

#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"


#define DIVISIONS 27


struct BPMLFO2 : Module {
	enum ParamIds {
		DIVISION_PARAM,
		DIVISION_CV_ATTENUVERTER_PARAM,
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
		DIVISION_INPUT,
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
		CLOCK_LIGHT,
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
	bool offset = false;


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
		float result;
        if (skew == 0 && x == 0) //Avoid /0 error
            return 2;
        if (x <= skew)
            result = 2.0 * (1- (-1 / skew * x + 1));
        else
            result = 2.0 * (1-(1 / inverseSkew * (x - skew)));
		return result;
	}

	float skewsaw() {
		if (offset)
			return skewsaw(phase);
		else
			return skewsaw(phase) - 1; //Going to keep same phase for now
			//return skewsaw(phase - .5) - 1;
	}

	float sqr() {
		float sqr = (phase < pw) ? 1.0 : -1.0;
		return offset ? sqr + 1.0 : sqr;
	}
	
	float progress() {
		return phase;
	}
};


	LowFrequencyOscillator oscillator;
	dsp::SchmittTrigger clockTrigger,resetTrigger,holdTrigger;
	float divisions[DIVISIONS] = {1/64.0,1/32.0,1/16.0,1/13.0,1/11.0,1/8.0,1/7.0,1/6.0,1/5.0,1/4.0,1/3.0,1/2.0,1/1.5,1,1.5,2,3,4,5,6,7,8,11,13,16,32,64};
	const char* divisionNames[DIVISIONS] = {"/64","/32","/16","/13","/11","/8","/7","/6","/5","/4","/3","/2","/1.5","x 1","x 1.5","x 2","x 3","x 4","x 5","x 6","x 7","x 8","x 11","x 13","x 16","x 32","x 64"};
	int division = 0;
	float time = 0.0;
	float duration = 0;
	float waveshape = 0;
	float skew = 0.5;
	float initialPhase = 0.0;
	bool holding = false;
	bool secondClockReceived = false;
	bool phase_quantized = false;

	float lfoOutputValue = 0.0;

	dsp::SchmittTrigger quantizePhaseTrigger;


	BPMLFO2() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DIVISION_PARAM, 0.0, 26.5, 13.0);
		configParam(DIVISION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0);
		configParam(SKEW_PARAM, 0.0, 1.0, 0.5);
		configParam(SKEW_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0);
		configParam(PHASE_PARAM, 0.0, 0.9999, 0.0);
		configParam(PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0);
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

    time += 1.0 / args.sampleRate;
	if(inputs[CLOCK_INPUT].isConnected()) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(secondClockReceived) {
				duration = time;
			}
			time = 0;
			secondClockReceived = true;			
		}
		lights[CLOCK_LIGHT].value = time > (duration/2.0);
	}
	
	

	float divisionf = params[DIVISION_PARAM].getValue();
	if(inputs[DIVISION_INPUT].isConnected()) {
		divisionf +=(inputs[DIVISION_INPUT].getVoltage() * params[DIVISION_CV_ATTENUVERTER_PARAM].getValue() * (DIVISIONS / 10.0));
	}
	divisionf = clamp(divisionf,0.0f,26.0f);
	division = int(divisionf);

	waveshape = params[WAVESHAPE_PARAM].getValue();

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
	oscillator.skew = skew;
	oscillator.setPulseWidth(skew);

	if(duration != 0) {
		oscillator.setFrequency(1.0 / (duration / divisions[division]));
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

	void drawProgress(const DrawArgs &args, int waveshape, float skew, float phase) 
	{
		float inverseSkew = 1 - skew;
		float y;
        if (skew == 0 && phase == 0) //Avoid /0 error
            y = 72.0;
        if (phase <= skew)
            y = 72.0 * (1- (-1 / skew * phase + 1));
        else
            y = 72.0 * (1-(1 / inverseSkew * (phase - skew)));

		// Draw indicator
		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		if(waveshape == BPMLFO2::SKEWSAW_WAV)
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,90,213);
			if(phase > skew) {
				nvgLineTo(args.vg,90 + (skew * 68),141);
			}
			nvgLineTo(args.vg,90 + (phase * 68),213-y);
			nvgLineTo(args.vg,90 + (phase * 68),213);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
		} else 
		{
			float endpoint = fminf(phase,skew);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,90,177);
			nvgRect(args.vg,90,177,endpoint*68,36.0);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
			if(phase > skew) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg,90 + (skew * 68),141);
				nvgRect(args.vg,90 + (skew * 68),141,(phase-skew)*68,36.0);
				nvgClosePath(args.vg);
				nvgFill(args.vg);
			}

		}
	}

	void drawWaveShape(const DrawArgs &args, int waveshape, float skew) 
	{
		// Draw wave shape
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));	
		nvgStrokeWidth(args.vg, 2.0);
		if(waveshape == BPMLFO2::SKEWSAW_WAV) {						
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,90,213);
			nvgLineTo(args.vg,90 + (skew * 68),141);
			nvgLineTo(args.vg,161,213);
			nvgClosePath(args.vg);			
		} else 
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,90,213);
			nvgLineTo(args.vg,90 + (skew * 68),213);
			nvgLineTo(args.vg,90 + (skew * 68),141);
			nvgLineTo(args.vg,161,141);
			//nvgClosePath(args.vg);			
		}
		nvgStroke(args.vg);		
	}

	void drawDivision(const DrawArgs &args, Vec pos, int division) {
		nvgFontSize(args.vg, 28);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->divisionNames[division]);
		nvgText(args.vg, pos.x + 52, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;	
		drawWaveShape(args,module->waveshape, module->skew);
		drawProgress(args,module->waveshape, module->skew, module->oscillator.progress());
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

		addParam(createParam<RoundLargeFWKnob>(Vec(50, 78), module, BPMLFO2::DIVISION_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(91, 109), module, BPMLFO2::DIVISION_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(5, 142), module, BPMLFO2::SKEW_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(8, 201), module, BPMLFO2::SKEW_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(52, 142), module, BPMLFO2::PHASE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(55, 201), module, BPMLFO2::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(38, 162), module, BPMLFO2::QUANTIZE_PHASE_PARAM));
		
		
		addParam(createParam<CKSS>(Vec(27, 240), module, BPMLFO2::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(57, 240), module, BPMLFO2::WAVESHAPE_PARAM));
		addParam(createParam<CKSS>(Vec(97, 240), module, BPMLFO2::HOLD_CLOCK_BEHAVIOR_PARAM));
		addParam(createParam<CKSS>(Vec(140, 240), module, BPMLFO2::HOLD_MODE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(90, 81), module, BPMLFO2::DIVISION_INPUT));
		addInput(createInput<PJ301MPort>(Vec(7, 173), module, BPMLFO2::SKEW_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 173), module, BPMLFO2::PHASE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(46, 290), module, BPMLFO2::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(77, 290), module, BPMLFO2::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(109, 290), module, BPMLFO2::HOLD_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(78, 336), module, BPMLFO2::LFO_OUTPUT));

		addChild(createLight<MediumLight<BlueLight>>(Vec(42, 166), module, BPMLFO2::QUANTIZE_PHASE_LIGHT));
		addChild(createLight<LargeLight<BlueLight>>(Vec(27, 294), module, BPMLFO2::CLOCK_LIGHT));
		addChild(createLight<LargeLight<RedLight>>(Vec(137, 294), module, BPMLFO2::HOLD_LIGHT));
	}
};



Model *modelBPMLFO2 = createModel<BPMLFO2, BPMLFO2Widget>("BPMLFO2");
