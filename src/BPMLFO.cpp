//#include <string.h>
#include "FrozenWasteland.hpp"
#include "dsp/digital.hpp"

#define DIVISIONS 27

struct LowFrequencyOscillator {
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
		pw = clampf(pw_, pwMin, 1.0 - pwMin);
	}

	void hardReset()
	{
		phase = 0.0;
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



struct BPMLFO : Module {
	enum ParamIds {
		DIVISION_PARAM,
		OFFSET_PARAM,		
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		DIVISION_INPUT,
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
		CLOCK_LIGHT,
		HOLD_LIGHT,
		NUM_LIGHTS
	};	
	


	LowFrequencyOscillator oscillator;
	SchmittTrigger clockTrigger,resetTrigger,holdTrigger;
	float divisions[DIVISIONS] = {1/64.0,1/32.0,1/16.0,1/13.0,1/11.0,1/8.0,1/7.0,1/6.0,1/5.0,1/4.0,1/3.0,1/2.0,1/1.5,1,1.5,2,3,4,5,6,7,8,11,13,16,32,64};
	const char* divisionNames[DIVISIONS] = {"/64","/32","/16","/13","/11","/8","/7","/6","/5","/4","/3","/2","/1.5","x 1","x 1.5","x 2","x 3","x 4","x 5","x 6","x 7","x 8","x 11","x 13","x 16","x 32","x 64"};
	int division;
	float time = 0.0;
	float duration = 0;
	bool holding = false;


	BPMLFO() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	void reset() override {
		division = 0;
	}

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void BPMLFO::step() {

    time += 1.0 / engineGetSampleRate();
	if(inputs[CLOCK_INPUT].active) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			duration = time;
			time = 0;
		}
		lights[CLOCK_LIGHT].value = time > (duration/2.0);
	}
	
	

	float divisionf = params[DIVISION_PARAM].value;
	if(inputs[DIVISION_INPUT].active) {
		divisionf +=(inputs[DIVISION_INPUT].value * (DIVISIONS / 10.0));
	}
	divisionf = clampf(divisionf,0.0,26.0);
	division = int(divisionf);
	
	if(duration != 0) {
		oscillator.setFrequency(1.0 / (duration / divisions[division]));
	}
	else {
		oscillator.setFrequency(0);
	}

	if(inputs[RESET_INPUT].active) {
		if(resetTrigger.process(inputs[RESET_INPUT].value)) {
			oscillator.hardReset();
		}		
	} 

	if(inputs[HOLD_INPUT].active) {
		if(holdTrigger.process(inputs[HOLD_INPUT].value)) {
			holding = !holding;
			lights[HOLD_LIGHT].value = holding;
		}		
	} 

	if(!holding) {
		oscillator.offset = (params[OFFSET_PARAM].value > 0.0);
		oscillator.step(1.0 / engineGetSampleRate());
	}


	outputs[SIN_OUTPUT].value = 5.0 * oscillator.sin();
	outputs[TRI_OUTPUT].value = 5.0 * oscillator.tri();
	outputs[SAW_OUTPUT].value = 5.0 * oscillator.saw();
	outputs[SQR_OUTPUT].value =  5.0 * oscillator.sqr();
		
}

struct BPMLFOProgressDisplay : TransparentWidget {
	BPMLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	BPMLFOProgressDisplay() {
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
			nvgArc(vg,75.8,184.5,35,startArc,endArc,NVG_CW);
			nvgLineTo(vg,75.8,184.5);
			nvgClosePath(vg);
		}
		nvgFill(vg);
	}

	void drawDivision(NVGcontext *vg, Vec pos, int division) {
		nvgFontSize(vg, 28);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->divisionNames[division]);
		nvgText(vg, pos.x + 52, pos.y, text, NULL);
	}

	void draw(NVGcontext *vg) override {
		
		drawProgress(vg,module->oscillator.progress());
		drawDivision(vg, Vec(0, box.size.y - 150), module->division);
	}
};

BPMLFOWidget::BPMLFOWidget() {
	BPMLFO *module = new BPMLFO();
	setModule(module);
	box.size = Vec(15*10, RACK_GRID_HEIGHT);
	
	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/BPMLFO.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	{
		BPMLFOProgressDisplay *display = new BPMLFOProgressDisplay();
		display->module = module;
		display->box.pos = Vec(0, 0);
		display->box.size = Vec(box.size.x, 220);
		addChild(display);
	}

	addParam(createParam<Davies1900hBlackKnob>(Vec(65, 85), module, BPMLFO::DIVISION_PARAM, 0.0, 26.5, 13.0));
	addParam(createParam<CKSS>(Vec(122, 210), module, BPMLFO::OFFSET_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ301MPort>(Vec(30, 95), module, BPMLFO::DIVISION_INPUT));
	addInput(createInput<PJ301MPort>(Vec(31, 270), module, BPMLFO::CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(62, 270), module, BPMLFO::RESET_INPUT));
	addInput(createInput<PJ301MPort>(Vec(94, 270), module, BPMLFO::HOLD_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, BPMLFO::SIN_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, BPMLFO::TRI_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, BPMLFO::SAW_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, BPMLFO::SQR_OUTPUT));

	addChild(createLight<LargeLight<BlueLight>>(Vec(12, 274), module, BPMLFO::CLOCK_LIGHT));
	addChild(createLight<LargeLight<RedLight>>(Vec(122, 274), module, BPMLFO::HOLD_LIGHT));
}
