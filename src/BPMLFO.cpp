//#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"



struct BPMLFO : Module {
	enum ParamIds {
		MULTIPLIER_PARAM,
		MULTIPLIER_CV_ATTENUVERTER_PARAM,
		DIVISION_PARAM,
		DIVISION_CV_ATTENUVERTER_PARAM,
		PHASE_PARAM,
		PHASE_CV_ATTENUVERTER_PARAM,
		QUANTIZE_PHASE_PARAM,
		OFFSET_PARAM,	
		HOLD_CLOCK_BEHAVIOR_PARAM,
		HOLD_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		MULTIPLIER_INPUT,
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
		HOLD_LIGHT,
		NUM_LIGHTS
	};	

	

	struct LowFrequencyOscillator {
		float basePhase = 0.0;
		double phase = 0.0;
		float pw = 0.5;
		double freq = 1.0;
		bool offset = false;

		void setPitch(float pitch) {
			pitch = fminf(pitch, 8.0);
			freq = powf(2.0, pitch);
		}
		void setFrequency(double frequency) {
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

		void step(double dt) {
			double deltaPhase = fmin(freq * dt, 0.5);
			phase += deltaPhase;
			if (phase >= 1.0)
				phase -= 1.0;
		}
		float sin() {
			if (offset)
				return 1.0 - cosf(2*M_PI * phase);
			else
				return sinf(2*M_PI * phase);
		}
		float tri(float x) {
			return 4.0 * fabsf(x - roundf(x));
		}
		float tri() {
			if (offset)
				return tri(phase);
			else
				return -1.0 + tri(phase - 0.75);
		}
		float saw(float x) {
			return 2.0 * (x - roundf(x));
		}
		float saw() {
			if (offset)
				return 2.0 * phase;
			else
				return saw(phase);
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
	dsp::SchmittTrigger clockTrigger,resetTrigger,holdTrigger,quantizePhaseTrigger;
	
	float multiplier = 1;
	float division = 1;
	float timeElapsed = 0.0;
	double duration = 0;
	float initialPhase = 0.0;
	bool holding = false;
	bool firstClockReceived = false;
	bool secondClockReceived = false;
	bool phase_quantized = false;

	float sinOutputValue = 0.0;
	float triOutputValue = 0.0;
	float sawOutputValue = 0.0;
	float sqrOutputValue = 0.0;

	


	BPMLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MULTIPLIER_PARAM, 1.0, 128, 1.0,"Multiplier");
		configParam(MULTIPLIER_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Multiplier CV Attenuation","%",0,100);
		configParam(DIVISION_PARAM, 1.0, 128, 1.0,"Divisor");
		configParam(DIVISION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Division CV Attenuation","%",0,100);
		configParam(PHASE_PARAM, 0.0, 0.9999, 0.0,"Phase","°",0,360);
		configParam(PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Phase CV Attenuation","%",0,100);
		configParam(QUANTIZE_PHASE_PARAM, 0.0, 1.0, 0.0);
		configParam(OFFSET_PARAM, 0.0, 1.0, 1.0);
		configParam(HOLD_CLOCK_BEHAVIOR_PARAM, 0.0, 1.0, 1.0);
		configParam(HOLD_MODE_PARAM, 0.0, 1.0, 1.0);
	}

	void process(const ProcessArgs &args) override {

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

		if(duration != 0) {
			oscillator.setFrequency(1.0 / (duration / multiplier * division));
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

	// void reset() override {
	// 	division = 0;
	// }

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};



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
			nvgArc(args.vg,60,124,25,startArc,endArc,NVG_CW);
			nvgLineTo(args.vg,60,124);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);
	}

	void drawMultiplier(const DrawArgs &args, Vec pos, float multiplier) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 3.0f", multiplier);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawDivision(const DrawArgs &args, Vec pos, float division) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 3.0f", division);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

		drawProgress(args,module->oscillator.progress());
		drawMultiplier(args, Vec(2, 48), module->multiplier);
		drawDivision(args, Vec(68, 48), module->division);
	}
};

struct BPMLFOWidget : ModuleWidget {
	BPMLFOWidget(BPMLFO *module) {

		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BPMLFO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			BPMLFOProgressDisplay *display = new BPMLFOProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}


		addParam(createParam<RoundSmallFWSnapKnob>(Vec(4, 52), module, BPMLFO::MULTIPLIER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(29, 74), module, BPMLFO::MULTIPLIER_CV_ATTENUVERTER_PARAM));

		addParam(createParam<RoundSmallFWSnapKnob>(Vec(67, 52), module, BPMLFO::DIVISION_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(92, 74), module, BPMLFO::DIVISION_CV_ATTENUVERTER_PARAM));
		
		
		addParam(createParam<RoundSmallFWKnob>(Vec(47, 171), module, BPMLFO::PHASE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(48, 222), module, BPMLFO::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(31, 192), module, BPMLFO::QUANTIZE_PHASE_PARAM));

		addParam(createParam<CKSS>(Vec(10, 262), module, BPMLFO::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(50, 262), module, BPMLFO::HOLD_CLOCK_BEHAVIOR_PARAM));
		addParam(createParam<CKSS>(Vec(90, 262), module, BPMLFO::HOLD_MODE_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(30, 54), module, BPMLFO::MULTIPLIER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(93, 54), module, BPMLFO::DIVISION_INPUT));

		
		addInput(createInput<FWPortInSmall>(Vec(49, 202), module, BPMLFO::PHASE_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(7, 312), module, BPMLFO::CLOCK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(48, 312), module, BPMLFO::RESET_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(80, 312), module, BPMLFO::HOLD_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(5, 345), module, BPMLFO::SIN_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(35, 345), module, BPMLFO::TRI_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(65, 345), module, BPMLFO::SAW_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(95, 345), module, BPMLFO::SQR_OUTPUT));

		addChild(createLight<LargeLight<BlueLight>>(Vec(32.5, 193.5), module, BPMLFO::QUANTIZE_PHASE_LIGHT));		
		addChild(createLight<LargeLight<RedLight>>(Vec(100, 312), module, BPMLFO::HOLD_LIGHT));
	}

};



Model *modelBPMLFO = createModel<BPMLFO, BPMLFOWidget>("BPMLFO");
