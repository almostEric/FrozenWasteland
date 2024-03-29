#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"



struct SeriouslySlowLFO : Module {
	enum ParamIds {
		TIME_BASE_PARAM,
		DURATION_PARAM = TIME_BASE_PARAM + 5,
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

	void setBasePhase(double initialPhase) {
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
	float tri(double x) {
		return 4.0 * fabsf(x - roundf(x));
	}
	float tri() {
		if (offset)
			return tri(invert ? phase - 0.5 : phase);
		else
			return -1.0 + tri(invert ? phase - 0.25 : phase - 0.75);
	}
	float saw(double x) {
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
	dsp::SchmittTrigger timeBaseTrigger[5], quantizePhaseTrigger, resetTrigger;
	
	double duration = 0.0;
	double initialPhase = 0.0;
	int timeBase = 0;
	bool phase_quantized = false;


	float durationPercentage = 0;
	float phasePercentage = 0;
	

	SeriouslySlowLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(DURATION_PARAM, 1.0, 100.0, 1.0,"Duration Multiplier");
		configParam(FM_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Duration CV Attenuation","%",0,100);
		configParam(PHASE_PARAM, 0.0, 0.9999, 0.0,"Phase","°",0,360);
		configParam(PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Phase CV Attenuation","%",0,100);

		configButton(TIME_BASE_PARAM+0,"Time Base - Minutes");
		configButton(TIME_BASE_PARAM+1,"Time Base - Hours");
		configButton(TIME_BASE_PARAM+2,"Time Base - Days");
		configButton(TIME_BASE_PARAM+3,"Time Base - Weeks");
		configButton(TIME_BASE_PARAM+4,"Time Base - Months");

		configButton(QUANTIZE_PHASE_PARAM,"Quantize Phase");
		configButton(RESET_PARAM,"Reset");
		configSwitch(OFFSET_PARAM, 0.f,1.f,0.f, "Offset", {"-5v to 5v", "0v-10v"});

		configInput(FM_INPUT, "Duration Multiplier");
		configInput(PHASE_INPUT, "Phase");
		configInput(RESET_INPUT, "Reset");

		configOutput(SIN_OUTPUT, "Sine");
		configOutput(TRI_OUTPUT, "Triangle");
		configOutput(SAW_OUTPUT, "Sawtooth");
		configOutput(SQR_OUTPUT, "Square/Pulse");

	}

	void process(const ProcessArgs &args) override {

		// if (sumTrigger.process(params[TIME_BASE_PARAM].getValue())) {
		// 	timeBase = (timeBase + 1) % 5;
		// 	oscillator.hardReset();
		// }

		for(int timeIndex = 0;timeIndex<5;timeIndex++)
		{	
			if (timeBaseTrigger[timeIndex].process(params[TIME_BASE_PARAM+timeIndex].getValue())) {
				timeBase = timeIndex;
				oscillator.hardReset();
			}
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
				numberOfSeconds = 2592000; // Months
				break;
		}

		duration = params[DURATION_PARAM].getValue();
		if(inputs[FM_INPUT].isConnected()) {
			duration +=inputs[FM_INPUT].getVoltage() * params[FM_CV_ATTENUVERTER_PARAM].getValue();
		}
		duration = clamp(duration,1.0f,100.0f);
		durationPercentage = duration / 100.0;

		oscillator.setFrequency(1.0 / (duration * numberOfSeconds));

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
		phasePercentage = initialPhase; 			
		if(phase_quantized) // Limit to 90 degree increments
			initialPhase = std::round(initialPhase * 4.0f) / 4.0f;

		
		oscillator.offset = (params[OFFSET_PARAM].getValue() > 0.0);
		oscillator.setBasePhase(initialPhase);

		//sr= args.sampleRate / 1000; // Test Code
		oscillator.step(args.sampleTime);

		        //fprintf(stderr, "sample time %f \n",args.sampleTime);


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
	std::string fontPath;



	SSLFOProgressDisplay() {
		fontPath = asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf");
	}

	void drawProgress(const DrawArgs &args, float phase)
	{
		//float startArc = (-M_PI) / 2.0; we know this rotates 90 degrees to top
		//float endArc = (phase * M_PI) - startArc;
		const float rotate90 = (M_PI) / 2.0;
		float startArc = 0 - rotate90;
		float endArc = (phase * M_PI * 2) - rotate90;

		// Draw indicator
		// nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));

		NVGpaint paint = nvgRadialGradient(args.vg, 141, 194.5, 0, 35,
						   nvgRGBA(0xff, 0xff, 0x20, 0xff), nvgRGBA(0xff, 0xff, 0x20, 0x1f));
		nvgFillPaint(args.vg, paint);


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
		font = APP->window->loadFont(fontPath);

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

		ParamWidget* durationParam = createParam<RoundLargeFWKnob>(Vec(56, 80), module, SeriouslySlowLFO::DURATION_PARAM);
		if (module) {
			dynamic_cast<RoundLargeFWKnob*>(durationParam)->percentage = &module->durationPercentage;
		}
		addParam(durationParam);							
		addParam(createParam<RoundSmallFWKnob>(Vec(99, 111), module, SeriouslySlowLFO::FM_CV_ATTENUVERTER_PARAM));

		ParamWidget* phaseParam = createParam<RoundFWKnob>(Vec(72, 162), module, SeriouslySlowLFO::PHASE_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(phaseParam)->percentage = &module->phasePercentage;
		}
		addParam(phaseParam);							
		addParam(createParam<RoundSmallFWKnob>(Vec(75, 223), module, SeriouslySlowLFO::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(58, 184), module, SeriouslySlowLFO::QUANTIZE_PHASE_PARAM));
		
		addParam(createParam<CKSS>(Vec(58, 266), module, SeriouslySlowLFO::OFFSET_PARAM));
		addParam(createParam<TL1105>(Vec(116, 276), module, SeriouslySlowLFO::RESET_PARAM));
		
		addInput(createInput<PJ301MPort>(Vec(98, 83), module, SeriouslySlowLFO::FM_INPUT));
		addInput(createInput<PJ301MPort>(Vec(74, 195), module, SeriouslySlowLFO::PHASE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(90, 272), module, SeriouslySlowLFO::RESET_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(29, 320), module, SeriouslySlowLFO::SIN_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(63, 320), module, SeriouslySlowLFO::TRI_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(98, 320), module, SeriouslySlowLFO::SAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(132, 320), module, SeriouslySlowLFO::SQR_OUTPUT));


		for(int i = 0;i<5;i++) {
			addParam(createParam<LEDButton>(Vec(3.5, 166 + i*20.0), module, SeriouslySlowLFO::TIME_BASE_PARAM+i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(5, 167.5 + i*20.0), module, SeriouslySlowLFO::MINUTES_LIGHT + i));
		}

		// addChild(createLight<MediumLight<BlueLight>>(Vec(5, 183), module, SeriouslySlowLFO::HOURS_LIGHT));
		// addChild(createLight<MediumLight<BlueLight>>(Vec(5, 198), module, SeriouslySlowLFO::DAYS_LIGHT));
		// addChild(createLight<MediumLight<BlueLight>>(Vec(5, 213), module, SeriouslySlowLFO::WEEKS_LIGHT));
		// addChild(createLight<MediumLight<BlueLight>>(Vec(5, 228), module, SeriouslySlowLFO::MONTHS_LIGHT));

		addChild(createLight<LargeLight<BlueLight>>(Vec(59.5, 185.5), module, SeriouslySlowLFO::QUANTIZE_PHASE_LIGHT));
	}
};

Model *modelSeriouslySlowLFO = createModel<SeriouslySlowLFO, SeriouslySlowLFOWidget>("SeriouslySlowLFO");
