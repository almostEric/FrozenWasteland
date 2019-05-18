//#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"


#define DIVISIONS 27


struct BPMLFO : Module {
	enum ParamIds {
		DIVISION_PARAM,
		PHASE_PARAM,
		PHASE_CV_ATTENUVERTER_PARAM,
		QUANTIZE_PHASE_PARAM,
		OFFSET_PARAM,	
		HOLD_CLOCK_BEHAVIOR_PARAM,
		HOLD_MODE_PARAM,
		DIVISION_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		DIVISION_INPUT,
		PHASE_INPUT,
		RESET_INPUT,
		HOLD_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SIN_OUTPUT,
		TRI_OUTPUT,
		SAW_OUTPUT,
		SQR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		QUANTIZE_PHASE_LIGHT,
		CLOCK_LIGHT,
		HOLD_LIGHT,
		NUM_LIGHTS
	};	

struct LowFrequencyOscillator {
	float basePhase = 0.0;
	float phase = 0.0;
	float pw = 0.5;
	float freq = 1.0;
	bool offset = false;
	bool invert = false;

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
	float sin() {
		if (offset)
			return 1.0 - cosf(2*M_PI * phase) * (invert ? -1.0 : 1.0);
		else
			return sinf(2*M_PI * phase) * (invert ? -1.0 : 1.0);
	}
	float tri(float x) {
		return 4.0 * fabsf(x - roundf(x));
	}
	float tri() {
		if (offset)
			return tri(invert ? phase - 0.5 : phase);
		else
			return -1.0 + tri(invert ? phase - 0.25 : phase - 0.75);
	}
	float saw(float x) {
		return 2.0 * (x - roundf(x));
	}
	float saw() {
		if (offset)
			return invert ? 2.0 * (1.0 - phase) : 2.0 * phase;
		else
			return saw(phase) * (invert ? -1.0 : 1.0);
	}
	float sqr() {
		float sqr = (phase < pw) ^ invert ? 1.0 : -1.0;
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
	int division;
	float time = 0.0;
	float duration = 0;
	float initialPhase = 0.0;
	bool holding = false;
	bool secondClockReceived = false;
	bool phase_quantized = false;

	float sinOutputValue = 0.0;
	float triOutputValue = 0.0;
	float sawOutputValue = 0.0;
	float sqrOutputValue = 0.0;

	dsp::SchmittTrigger quantizePhaseTrigger;


	BPMLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DIVISION_PARAM, 0.0, 26.5, 13.0);
		configParam(DIVISION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0);
		configParam(PHASE_PARAM, 0.0, 0.9999, 0.0);
		configParam(PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0);
		configParam(QUANTIZE_PHASE_PARAM, 0.0, 1.0, 0.0);
		configParam(OFFSET_PARAM, 0.0, 1.0, 1.0);
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


void BPMLFO::process(const ProcessArgs &args) {

    time += 1.0 / args.sampleRate;
	if(inputs[CLOCK_INPUT].isConnected()) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(secondClockReceived) {
				duration = time;
			}
			time = 0;
			secondClockReceived = true;			
			//secondClockReceived = !secondClockReceived;			
		}
		lights[CLOCK_LIGHT].value = time > (duration/2.0);
	}
	
	

	float divisionf = params[DIVISION_PARAM].getValue();
	if(inputs[DIVISION_INPUT].isConnected()) {
		divisionf +=(inputs[DIVISION_INPUT].getVoltage() * params[DIVISION_CV_ATTENUVERTER_PARAM].getValue() * (DIVISIONS / 10.0));
	}
	divisionf = clamp(divisionf,0.0f,26.0f);
	division = int(divisionf);

	if(duration != 0) {
		oscillator.setFrequency(1.0 / (duration / divisions[division]));
	}
	else {
		oscillator.setFrequency(0);
	}

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
		sinOutputValue = 5.0 * oscillator.sin();
		triOutputValue = 5.0 * oscillator.tri();
		sawOutputValue = 5.0 * oscillator.saw();
		sqrOutputValue = 5.0 * oscillator.sqr();
	}


	outputs[SIN_OUTPUT].setVoltage(sinOutputValue);
	outputs[TRI_OUTPUT].setVoltage(triOutputValue);
	outputs[SAW_OUTPUT].setVoltage(sawOutputValue);
	outputs[SQR_OUTPUT].setVoltage( sqrOutputValue);
		
}

struct BPMLFOProgressDisplay : TransparentWidget {
	BPMLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	BPMLFOProgressDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}

	void drawProgress(const DrawArgs &args, float phase) 
	{
		const float rotate90 = (M_PI) / 2.0;
		float startArc = 0 - rotate90;
		float endArc = (phase * M_PI * 2) - rotate90;

		// Draw indicator
		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		{
			nvgBeginPath(args.vg);
			nvgArc(args.vg,100.8,170,35,startArc,endArc,NVG_CW);
			nvgLineTo(args.vg,100.8,170);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);
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

		drawProgress(args,module->oscillator.progress());
		drawDivision(args, Vec(0, box.size.y - 153), module->division);
	}
};

struct BPMLFOWidget : ModuleWidget {
	BPMLFOWidget(BPMLFO *module) {

		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BPMLFO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			BPMLFOProgressDisplay *display = new BPMLFOProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}

		addParam(createParam<RoundLargeFWKnob>(Vec(35, 78), module, BPMLFO::DIVISION_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 109), module, BPMLFO::DIVISION_CV_ATTENUVERTER_PARAM));
		
		addParam(createParam<RoundFWKnob>(Vec(27, 132), module, BPMLFO::PHASE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(30, 191), module, BPMLFO::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(13, 152), module, BPMLFO::QUANTIZE_PHASE_PARAM));

		addParam(createParam<CKSS>(Vec(12, 224), module, BPMLFO::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(70, 224), module, BPMLFO::HOLD_CLOCK_BEHAVIOR_PARAM));
		addParam(createParam<CKSS>(Vec(125, 224), module, BPMLFO::HOLD_MODE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(75, 81), module, BPMLFO::DIVISION_INPUT));
		addInput(createInput<PJ301MPort>(Vec(29, 163), module, BPMLFO::PHASE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(31, 275), module, BPMLFO::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(62, 275), module, BPMLFO::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(94, 275), module, BPMLFO::HOLD_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(11, 323), module, BPMLFO::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(45, 323), module, BPMLFO::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(80, 323), module, BPMLFO::SAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(114, 323), module, BPMLFO::SQR_OUTPUT));

		addChild(createLight<MediumLight<BlueLight>>(Vec(17, 156), module, BPMLFO::QUANTIZE_PHASE_LIGHT));
		addChild(createLight<LargeLight<BlueLight>>(Vec(12, 279), module, BPMLFO::CLOCK_LIGHT));
		addChild(createLight<LargeLight<RedLight>>(Vec(122, 279), module, BPMLFO::HOLD_LIGHT));
	}

};



Model *modelBPMLFO = createModel<BPMLFO, BPMLFOWidget>("BPMLFO");
