#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"


//Not sure why this is necessary, but SSLFO is running twice as fast as sample rate says it should
#define SampleRateCompensation 2



struct SeriouslySlowLFO : Module {
	enum ParamIds {
		TIME_BASE_PARAM,
		DURATION_PARAM,
		FM_CV_ATTENUVERTER_PARAM,
		PHASE_PARAM,
		PHASE_CV_ATTENUVERTER_PARAM,
		QUANTIZE_PHASE_PARAM,
		OFFSET_PARAM,
		RESET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
		PHASE_INPUT,
		RESET_INPUT,
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
		MINUTES_LIGHT,
		HOURS_LIGHT,
		DAYS_LIGHT,
		WEEKS_LIGHT,
		MONTHS_LIGHT,
		QUANTIZE_PHASE_LIGHT,
		NUM_LIGHTS
	};

struct LowFrequencyOscillator {
	double basePhase = 0.0;
	double phase = 0.0;
	float pw = 0.5;
	double freq = 1.0;
	bool offset = false;
	bool invert = false;
	
	LowFrequencyOscillator() {}
	void setPitch(double pitch) {
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

	void step(float dt) {
		//float deltaPhase = fminf(freq * dt, 0.5);
		double deltaPhase = freq * dt;
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
	double progress() {
		return phase;
	}
};




	LowFrequencyOscillator oscillator;
	dsp::SchmittTrigger sumTrigger, quantizePhaseTrigger, resetTrigger;
	
	double duration = 0.0;
	double initialPhase = 0.0;
	int timeBase = 0;
	bool phase_quantized = false;
	

	SeriouslySlowLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(DURATION_PARAM, 1.0, 100.0, 1.0,"Duration Multiplier");
		configParam(FM_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"FM CV Attenuation","%",0,100);
		configParam(PHASE_PARAM, 0.0, 0.9999, 0.0,"Phase","°",0,360);
		configParam(PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Phase CV Attenuation","%",0,100);
		configParam(QUANTIZE_PHASE_PARAM, 0.0, 1.0, 0.0);
		configParam(OFFSET_PARAM, 0.0, 1.0, 1.0);
		configParam(RESET_PARAM, 0.0, 1.0, 0.0);
	}

	void process(const ProcessArgs &args) override {

		if (sumTrigger.process(params[TIME_BASE_PARAM].getValue())) {
			timeBase = (timeBase + 1) % 5;
			oscillator.hardReset();
		}

		if(resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			oscillator.hardReset();
		}

		double numberOfSeconds = 0;
		switch(timeBase) {
			case 0 :
				numberOfSeconds = 60; // Minutes
				break;
			case 1 :
				numberOfSeconds = 3600; // Hours
				break;
			case 2 :
				numberOfSeconds = 86400; // Days
				break;
			case 3 :
				numberOfSeconds = 604800; // Weeks
				break;
			case 4 :
				numberOfSeconds = 259200; // Months
				break;
		}

		duration = params[DURATION_PARAM].getValue();
		if(inputs[FM_INPUT].isConnected()) {
			duration +=inputs[FM_INPUT].getVoltage() * params[FM_CV_ATTENUVERTER_PARAM].getValue();
		}
		duration = clamp(duration,1.0f,100.0f);

		oscillator.setFrequency(1.0 / (duration * SampleRateCompensation * numberOfSeconds));

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

		//sr= args.sampleRate / 1000; // Test Code
		oscillator.step(1.0 / args.sampleRate);


		outputs[SIN_OUTPUT].setVoltage(5.0 * oscillator.sin());
		outputs[TRI_OUTPUT].setVoltage(5.0 * oscillator.tri());
		outputs[SAW_OUTPUT].setVoltage(5.0 * oscillator.saw());
		outputs[SQR_OUTPUT].setVoltage( 5.0 * oscillator.sqr());

		for(int lightIndex = 0;lightIndex<5;lightIndex++)
		{
			lights[lightIndex].value = lightIndex != timeBase ? 0.0 : 1.0;
		}
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "timeBase", json_integer((int) timeBase));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "timeBase");
		if (sumJ)
			timeBase = json_integer_value(sumJ);
	}

	// void reset() override {
	// 	timeBase = 0;
	// }

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

struct SSLFOProgressDisplay : TransparentWidget {
	SeriouslySlowLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;



	SSLFOProgressDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

	void drawProgress(const DrawArgs &args, float phase)
	{
		//float startArc = (-M_PI) / 2.0; we know this rotates 90 degrees to top
		//float endArc = (phase * M_PI) - startArc;
		const float rotate90 = (M_PI) / 2.0;
		float startArc = 0 - rotate90;
		float endArc = (phase * M_PI * 2) - rotate90;

		// Draw indicator
		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		{
			nvgBeginPath(args.vg);
			nvgArc(args.vg,141,194.5,35,startArc,endArc,NVG_CW);
			nvgLineTo(args.vg,141,194.5);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);
	}

	void drawDuration(const DrawArgs &args, Vec pos, float duration) {
		nvgFontSize(args.vg, 28);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % #6.1f", duration);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x + 135, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		drawProgress(args,module->oscillator.progress());
		drawDuration(args, Vec(0, box.size.y - 155), module->duration);
		
	}
};

struct SeriouslySlowLFOWidget : ModuleWidget {
	SeriouslySlowLFOWidget(SeriouslySlowLFO *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SeriouslySlowLFO.svg")));
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			SSLFOProgressDisplay *display = new SSLFOProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}

		addParam(createParam<CKD6>(Vec(8, 240), module, SeriouslySlowLFO::TIME_BASE_PARAM));
		addParam(createParam<RoundLargeFWKnob>(Vec(56, 80), module, SeriouslySlowLFO::DURATION_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(99, 111), module, SeriouslySlowLFO::FM_CV_ATTENUVERTER_PARAM));

		addParam(createParam<RoundFWKnob>(Vec(72, 162), module, SeriouslySlowLFO::PHASE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 223), module, SeriouslySlowLFO::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(58, 184), module, SeriouslySlowLFO::QUANTIZE_PHASE_PARAM));
		
		addParam(createParam<CKSS>(Vec(48, 266), module, SeriouslySlowLFO::OFFSET_PARAM));
		addParam(createParam<TL1105>(Vec(106, 276), module, SeriouslySlowLFO::RESET_PARAM));
		
		addInput(createInput<PJ301MPort>(Vec(98, 83), module, SeriouslySlowLFO::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(74, 195), module, SeriouslySlowLFO::PHASE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(80, 272), module, SeriouslySlowLFO::RESET_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(29, 320), module, SeriouslySlowLFO::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(63, 320), module, SeriouslySlowLFO::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(98, 320), module, SeriouslySlowLFO::SAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(132, 320), module, SeriouslySlowLFO::SQR_OUTPUT));

		addChild(createLight<MediumLight<BlueLight>>(Vec(5, 168), module, SeriouslySlowLFO::MINUTES_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(5, 183), module, SeriouslySlowLFO::HOURS_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(5, 198), module, SeriouslySlowLFO::DAYS_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(5, 213), module, SeriouslySlowLFO::WEEKS_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(5, 228), module, SeriouslySlowLFO::MONTHS_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(62, 188), module, SeriouslySlowLFO::QUANTIZE_PHASE_LIGHT));
	}
};

Model *modelSeriouslySlowLFO = createModel<SeriouslySlowLFO, SeriouslySlowLFOWidget>("SeriouslySlowLFO");
