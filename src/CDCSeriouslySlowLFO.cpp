#include "FrozenWasteland.hpp"



struct CDCSeriouslySlowLFO : Module {
	enum ParamIds {
		TIME_BASE_PARAM,
		DURATION_PARAM,
		FM_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FM_INPUT,
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
		YEARS_LIGHT,
		CENTURIES_LIGHT,
		MILLENIUM_LIGHT,
		AGE_LIGHT,
		ERA_LIGHT,
		UNIVERSE_LIGHT,
		HEAT_DEATH_LIGHT,
		NUM_LIGHTS
	};

struct LowFrequencyOscillator {
	float phase = 0.0;
	float pw = 0.5;
	float freq = 1.0;
	bool offset = false;
	bool invert = false;
	dsp::SchmittTrigger resetTrigger;
	LowFrequencyOscillator() {}
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
	void setReset(float reset) {
		if (resetTrigger.process(reset)) {
			phase = 0.0;
		}
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

	LowFrequencyOscillator oscillator;
	dsp::SchmittTrigger sumTrigger;
	float duration = 0.0;
	int timeBase = 0;


	CDCSeriouslySlowLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(DURATION_PARAM, 1.0, 100.0, 1.0);
		configParam(FM_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0);
	}
	void process(const ProcessArgs &args) override;

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


void CDCSeriouslySlowLFO::process(const ProcessArgs &args) {

	if (sumTrigger.process(params[TIME_BASE_PARAM].getValue())) {
		timeBase = (timeBase + 1) % 7;
		oscillator.hardReset();
	}

	const float year = 31556925.97474; // seconds in tropical year for 1900 from http://www.journaloftheoretics.com/articles/3-3/uwe.pdf
	double numberOfSeconds = 0;
	switch(timeBase) {
		case 0 :
			numberOfSeconds = year; // Years
			break;
		case 1 :
			numberOfSeconds = year * 100; // Centuries
			break;
		case 2 :
			numberOfSeconds = year * 1000; // Millenium
			break;
		case 3 :
			numberOfSeconds = year * 1e+06; // Age
			break;
		case 4 :
			numberOfSeconds = year * 1e+08; // Era
			break;
		case 5 :
			numberOfSeconds = year * 13.772 * 1e+9; // Age of Universe
			break;
		case 6 :
			numberOfSeconds = year * 1e+100; // Heat Death of Universe from https://en.wikipedia.org/wiki/Graphical_timeline_from_Big_Bang_to_Heat_Death
			break;
	}

	duration = params[DURATION_PARAM].getValue();
	if(inputs[FM_INPUT].isConnected()) {
		duration +=inputs[FM_INPUT].getVoltage() * params[FM_CV_ATTENUVERTER_PARAM].getValue();
	}
	duration = clamp(duration,1.0f,100.0f);

	oscillator.setFrequency(1.0 / (duration * numberOfSeconds));
	oscillator.step(1.0 / args.sampleRate);
	if(inputs[RESET_INPUT].isConnected()) {
		oscillator.setReset(inputs[RESET_INPUT].getVoltage());
	}


	outputs[SIN_OUTPUT].setVoltage(5.0 * oscillator.sin());
	outputs[TRI_OUTPUT].setVoltage(5.0 * oscillator.tri());
	outputs[SAW_OUTPUT].setVoltage(5.0 * oscillator.saw());
	outputs[SQR_OUTPUT].setVoltage( 5.0 * oscillator.sqr());

	for(int lightIndex = 0;lightIndex < 7;lightIndex++)
	{
		lights[lightIndex].value = lightIndex != timeBase ? 0.0 : 1.0;
	}
}

struct CDCSSLFOProgressDisplay : TransparentWidget {
	CDCSeriouslySlowLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;



	CDCSSLFOProgressDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
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
			nvgArc(args.vg,109.8,184.5,35,startArc,endArc,NVG_CW);
			nvgLineTo(args.vg,109.8,184.5);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);
	}

	void drawDuration(const DrawArgs &args, Vec pos, float duration) {
		nvgFontSize(args.vg, 28);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % #6.1f", duration);
		nvgText(args.vg, pos.x + 22, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		drawProgress(args,module->oscillator.progress());
		drawDuration(args, Vec(0, box.size.y - 140), module->duration);
	}
};

struct CDCSeriouslySlowLFOWidget : ModuleWidget {
	CDCSeriouslySlowLFOWidget(CDCSeriouslySlowLFO *module) {

		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/CDCSeriouslySlowLFO.svg")));
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			CDCSSLFOProgressDisplay *display = new CDCSSLFOProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}

		addParam(createParam<CKD6>(Vec(10, 270), module, CDCSeriouslySlowLFO::TIME_BASE_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(75, 90), module, CDCSeriouslySlowLFO::DURATION_PARAM));
		addParam(createParam<RoundSmallBlackKnob>(Vec(44, 121), module, CDCSeriouslySlowLFO::FM_CV_ATTENUVERTER_PARAM));


		addInput(createInput<PJ301MPort>(Vec(43, 93), module, CDCSeriouslySlowLFO::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(95, 274), module, CDCSeriouslySlowLFO::RESET_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(11, 320), module, CDCSeriouslySlowLFO::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(45, 320), module, CDCSeriouslySlowLFO::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(80, 320), module, CDCSeriouslySlowLFO::SAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(114, 320), module, CDCSeriouslySlowLFO::SQR_OUTPUT));

		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 168), module, CDCSeriouslySlowLFO::YEARS_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 183), module, CDCSeriouslySlowLFO::CENTURIES_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 198), module, CDCSeriouslySlowLFO::MILLENIUM_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 213), module, CDCSeriouslySlowLFO::AGE_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 228), module, CDCSeriouslySlowLFO::ERA_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 243), module, CDCSeriouslySlowLFO::UNIVERSE_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(Vec(10, 258), module, CDCSeriouslySlowLFO::HEAT_DEATH_LIGHT));
	}
};


Model *modelCDCSeriouslySlowLFO = createModel<CDCSeriouslySlowLFO, CDCSeriouslySlowLFOWidget>("CDCSeriouslySlowLFO");
