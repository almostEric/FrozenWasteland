//NOTE Because I am tipsy
//BPM based modulation of phase. Not sure if it means anything other than 2 BPM LFOS :)

#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define DISPLAY_SIZE 50
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 13

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
		CLOCK_MODE_PARAM,
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
		LFO_45_OUTPUT,
		LFO_90_OUTPUT,
		LFO_180_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		QUANTIZE_PHASE_LIGHT,
		HOLD_LIGHT,
		CLOCK_MODE_LIGHT,
		NUM_LIGHTS
	};	
	enum Waveshapes {
		SKEWSAW_WAV,
		SQUARE_WAV
	};

	

	struct LowFrequencyOscillator {
		float basePhase = 0.0;
		double phase = 0.0;
		double freq = 1.0;
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

			//Regular Bounds Checking
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

		float skewsawInternal(float x) {
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
		
		float skewsaw(double phaseOffset) {
			double phaseToUse = phase + phaseOffset;
			if (phaseToUse >= 1.0)
				phaseToUse -= 1.0;

			if (offset)
				return lerp(skewsawInternal(phaseToUse),sin(phaseOffset),waveSlope);
			
			return lerp(skewsawInternal(phaseToUse) - 1 ,sin(phaseOffset),waveSlope); //Going to keep same phase for now				
		}


		float sin(double phaseOffset) {
			double phaseToUse = phase + phaseOffset - 0.25; //Sin is out of ohase of other waveforms
			if (phaseToUse >= 1.0)
				phaseToUse -= 1.0;

			if (offset)
				return 1.0 - cosf(2*M_PI * (phaseToUse-skew*0));
			
			return sinf(2*M_PI * (phaseToUse-skew*0));
		}

		float sqr(double phaseOffset) {
			double phaseToUse = phase + phaseOffset;
			if (phaseToUse >= 1.0)
				phaseToUse -= 1.0;

			float sqr = (phaseToUse < pw) ? 1.0 : -1.0;
			sqr = offset ? sqr + 1.0 : sqr;

			float result = lerp(sqr,sin(phaseOffset),waveSlope);
			return result;
		}
		
		float progress() {
			return phase;
		}
	};

	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here



	LowFrequencyOscillator oscillator,displayOscillator;
	dsp::SchmittTrigger clockTrigger,resetTrigger,holdTrigger;
	float multiplier = 1;
	float division = 1;
	double timeElapsed = 0.0;
	double duration = 0;
	float waveshape = 0;
	float waveSlope = 0.0;
	float skew = 0.5;
	float initialPhase = 0.0;
	bool holding = false;
	bool firstClockReceived = false;
	bool secondClockReceived = false;
	bool phaseQuantized = false;
	bool clockMode = false;

	float lfoOutputValue = 0.0;
	float lfo45OutputValue = 0.0;
	float lfo90OutputValue = 0.0;
	float lfo180OutputValue = 0.0;

	float lastWaveShape = -1;
	float lastWaveSlope = -1;
	float lastSkew = -1;
	float waveValues[DISPLAY_SIZE] = {};

	dsp::SchmittTrigger quantizePhaseTrigger,clockModeTrigger;

	//percentages
	float multiplierPercentage = 0;
	float divisionPercentage = 0;
	float phasePercentage = 0;
	float waveSlopePercentage = 0;
	float waveSkewPercentage = 0;

	int slowParamCount = 0;
	int paramLimiter = 20;


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
		configParam(WAVESHAPE_PARAM, 0.0, 1.0, 0.0,"TRI/SQR");
		configParam(HOLD_CLOCK_BEHAVIOR_PARAM, 0.0, 1.0, 1.0);
		configParam(HOLD_MODE_PARAM, 0.0, 1.0, 1.0);
		configParam(CLOCK_MODE_PARAM, 0.0, 1.0, 1.0,"Clock Mode");

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}
	json_t *dataToJson() override;
	void dataFromJson(json_t *rootJ) override;
	void process(const ProcessArgs &args) override;

	// void reset() override {
	// 	division = 0;
	// }

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

json_t *BPMLFO2::dataToJson()  {
	json_t *rootJ = json_object();
			
	json_object_set_new(rootJ, "phaseQuantized", json_boolean(phaseQuantized));
	json_object_set_new(rootJ, "clockMode", json_boolean(clockMode));

	return rootJ;
}

void BPMLFO2::dataFromJson(json_t *rootJ) {
	json_t *pqj = json_object_get(rootJ, "phaseQuantized");
	if (pqj)
		phaseQuantized = json_boolean_value(pqj);

	json_t *cmj = json_object_get(rootJ, "clockMode");
	if (cmj)
		clockMode = json_boolean_value(cmj);
}

void BPMLFO2::process(const ProcessArgs &args) {

    timeElapsed += 1.0 / args.sampleRate;
	if(inputs[CLOCK_INPUT].isConnected()) {
		if(!params[CLOCK_MODE_PARAM].getValue()) {
			double bpm = powf(2.0,clamp(inputs[CLOCK_INPUT].getVoltage(),-10.0f,10.0f)) * 120.0;
			duration = 60.0 / bpm;
		} else {
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
		}
			// fprintf(stderr, "duration  %f\n", duration);
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
	multiplierPercentage = multiplier / 128.0;

	division = params[DIVISION_PARAM].getValue();
	if(inputs[DIVISION_INPUT].isConnected()) {
		division +=(inputs[DIVISION_INPUT].getVoltage() * params[DIVISION_CV_ATTENUVERTER_PARAM].getValue() * 12.8);
	}
	division = clamp(division,1.0f,128.0f);
	divisionPercentage = division / 128.0;

	if(slowParamCount % paramLimiter == 0) {
		waveshape = params[WAVESHAPE_PARAM].getValue();

		waveSlope = params[WAVESLOPE_PARAM].getValue();
		if(inputs[WAVESLOPE_INPUT].isConnected()) {
			waveSlope +=inputs[WAVESLOPE_INPUT].getVoltage() / 10 * params[WAVESLOPE_CV_ATTENUVERTER_PARAM].getValue();
		}
		waveSlope = clamp(waveSlope,0.0f,1.0f);
		waveSlopePercentage = waveSlope;

		skew = params[SKEW_PARAM].getValue();
		if(inputs[SKEW_INPUT].isConnected()) {
			skew +=inputs[SKEW_INPUT].getVoltage() / 10 * params[SKEW_CV_ATTENUVERTER_PARAM].getValue();
		}
		skew = clamp(skew,0.0f,1.0f);
		waveSkewPercentage = skew;

		slowParamCount = 0;
	}
	slowParamCount++;

	if (clockModeTrigger.process(params[CLOCK_MODE_PARAM].getValue())) {
		clockMode = !clockMode;
	}
	lights[CLOCK_MODE_LIGHT].value = clockMode;

	if (quantizePhaseTrigger.process(params[QUANTIZE_PHASE_PARAM].getValue())) {
		phaseQuantized = !phaseQuantized;
	}
	lights[QUANTIZE_PHASE_LIGHT].value = phaseQuantized;

	initialPhase = params[PHASE_PARAM].getValue();
	if(inputs[PHASE_INPUT].isConnected()) {
		initialPhase += (inputs[PHASE_INPUT].getVoltage() / 10 * params[PHASE_CV_ATTENUVERTER_PARAM].getValue());
	}
	if (initialPhase >= 1.0)
		initialPhase -= 1.0;
	else if (initialPhase < 0)
		initialPhase += 1.0;	
	phasePercentage = initialPhase;
	if(phaseQuantized) // Limit to 90 degree increments
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
			waveValues[i] = (waveshape == SKEWSAW_WAV ? displayOscillator.skewsaw(0.0) : displayOscillator.sqr(0.0)) * DISPLAY_SIZE / 2;
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
    	oscillator.step(args.sampleTime);
    }

	if(!holding) {
		if(waveshape == SKEWSAW_WAV) {
			lfoOutputValue = 5.0 * oscillator.skewsaw(0.0);
			lfo45OutputValue = 5.0 * oscillator.skewsaw(0.125);
			lfo90OutputValue = 5.0 * oscillator.skewsaw(0.25);
			lfo180OutputValue = 5.0 * oscillator.skewsaw(0.5);
		}
		else {
			lfoOutputValue = 5.0 * oscillator.sqr(0.0);
			lfo45OutputValue = 5.0 * oscillator.sqr(0.125);
			lfo90OutputValue = 5.0 * oscillator.sqr(0.25);
			lfo180OutputValue = 5.0 * oscillator.sqr(0.5);
		}		
	}
	

	outputs[LFO_OUTPUT].setVoltage(lfoOutputValue);
	outputs[LFO_45_OUTPUT].setVoltage(lfo45OutputValue);
	outputs[LFO_90_OUTPUT].setVoltage(lfo90OutputValue);
	outputs[LFO_180_OUTPUT].setVoltage(lfo180OutputValue);

	bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelBPMLFOPhaseExpander));
	if(rightExpanderPresent) {
		float *messageToSlave = (float*)(rightExpander.module->leftExpander.producerMessage);	
		messageToSlave[0] = inputs[CLOCK_INPUT].isConnected(); 	
		messageToSlave[1] = inputs[CLOCK_INPUT].getVoltage();
		messageToSlave[2] = inputs[RESET_INPUT].getVoltage();
		messageToSlave[3] = inputs[HOLD_INPUT].getVoltage();
		messageToSlave[4] = multiplier;
		messageToSlave[5] = division;
		messageToSlave[6] = initialPhase;
		messageToSlave[7] = params[OFFSET_PARAM].getValue();
		messageToSlave[8] = params[HOLD_MODE_PARAM].getValue();
		messageToSlave[9] = params[HOLD_CLOCK_BEHAVIOR_PARAM].getValue();
		messageToSlave[10] = waveshape;
		messageToSlave[11] = waveSlope;
		messageToSlave[12] = skew;
	}

}

struct BPMLFO2ProgressDisplay : TransparentWidget {
	BPMLFO2 *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	BPMLFO2ProgressDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}


	void drawWaveShape(const DrawArgs &args, int waveshape, float skew, float waveslope) 
	{
		// Draw wave shape
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		nvgStrokeWidth(args.vg, 1.0);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,34,125 - module->waveValues[0]);
		for(int i=1;i<DISPLAY_SIZE;i++) {
			nvgLineTo(args.vg,34 + i,125 - module->waveValues[i]);
		}
		
		nvgStroke(args.vg);		
	}

	void drawProgress(const DrawArgs &args, int waveshape, float skew, float waveslope, float phase) 
	{		
		// Draw indicator
		// nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));	
		nvgStrokeWidth(args.vg, 1.0);
		NVGpaint paint = nvgLinearGradient(args.vg,0,0,0,151,nvgRGBA(0xff, 0xff, 0x20, 0xff), nvgRGBA(0xff, 0xff, 0x20, 0x1f));
        nvgStrokePaint(args.vg, paint);

		nvgBeginPath(args.vg);
		//nvgMoveTo(args.vg,140,177 - module->waveValues[0]);
		for(int i=0;i<phase * DISPLAY_SIZE;i++) {
			nvgMoveTo(args.vg,34 + i,125-module->waveValues[i]);
			nvgLineTo(args.vg,34 + i, 151 );
			
		}
		nvgStroke(args.vg);		
		
	}


	void drawMultiplier(const DrawArgs &args, Vec pos, float multiplier) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 3.0f", multiplier);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawDivision(const DrawArgs &args, Vec pos, float division) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 3.0f", division);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;	
		drawWaveShape(args,module->waveshape, module->skew, module->waveSlope);
		drawProgress(args,module->waveshape, module->skew, module->waveSlope, module->oscillator.progress());
		drawMultiplier(args, Vec(38, 47), module->multiplier);
		drawDivision(args, Vec(104, 47), module->division);
	}
};

struct BPMLFO2Widget : ModuleWidget {
	BPMLFO2Widget(BPMLFO2 *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BPMLFO2.svg")));
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			BPMLFO2ProgressDisplay *display = new BPMLFO2ProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}

		ParamWidget* multiplierParam = createParam<RoundSmallFWSnapKnob>(Vec(4, 52), module, BPMLFO2::MULTIPLIER_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(multiplierParam)->percentage = &module->multiplierPercentage;
		}
		addParam(multiplierParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(29, 74), module, BPMLFO2::MULTIPLIER_CV_ATTENUVERTER_PARAM));

		ParamWidget* divisionParam = createParam<RoundSmallFWSnapKnob>(Vec(67, 52), module, BPMLFO2::DIVISION_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(divisionParam)->percentage = &module->divisionPercentage;
		}
		addParam(divisionParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(92, 74), module, BPMLFO2::DIVISION_CV_ATTENUVERTER_PARAM));

		ParamWidget* waveSlopeParam = createParam<RoundSmallFWKnob>(Vec(5, 171), module, BPMLFO2::WAVESLOPE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(waveSlopeParam)->percentage = &module->waveSlopePercentage;
		}
		addParam(waveSlopeParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(7, 223), module, BPMLFO2::WAVESLOPE_CV_ATTENUVERTER_PARAM));

		ParamWidget* waveSkewParam = createParam<RoundSmallFWKnob>(Vec(45, 171), module, BPMLFO2::SKEW_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(waveSkewParam)->percentage = &module->waveSkewPercentage;
		}
		addParam(waveSkewParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(47, 223), module, BPMLFO2::SKEW_CV_ATTENUVERTER_PARAM));

		ParamWidget* phaseParam = createParam<RoundSmallFWKnob>(Vec(90, 171), module, BPMLFO2::PHASE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(phaseParam)->percentage = &module->phasePercentage;
		}
		addParam(phaseParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(92, 223), module, BPMLFO2::PHASE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<LEDButton>(Vec(75, 191), module, BPMLFO2::QUANTIZE_PHASE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(76.5, 192.5), module, BPMLFO2::QUANTIZE_PHASE_LIGHT));
		
		
		addParam(createParam<CKSS>(Vec(5, 262), module, BPMLFO2::CLOCK_MODE_PARAM));
		addParam(createParam<CKSS>(Vec(28, 262), module, BPMLFO2::OFFSET_PARAM));
		addParam(createParam<CKSS>(Vec(51, 262), module, BPMLFO2::WAVESHAPE_PARAM));
		addParam(createParam<CKSS>(Vec(76, 262), module, BPMLFO2::HOLD_CLOCK_BEHAVIOR_PARAM));
		addParam(createParam<CKSS>(Vec(99, 262), module, BPMLFO2::HOLD_MODE_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(30, 54), module, BPMLFO2::MULTIPLIER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(93, 54), module, BPMLFO2::DIVISION_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(8, 202), module, BPMLFO2::WAVESLOPE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(48, 202), module, BPMLFO2::SKEW_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(93, 202), module, BPMLFO2::PHASE_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(9, 312), module, BPMLFO2::CLOCK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(48, 312), module, BPMLFO2::RESET_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(80, 312), module, BPMLFO2::HOLD_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(5, 345), module, BPMLFO2::LFO_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(35, 345), module, BPMLFO2::LFO_45_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(65, 345), module, BPMLFO2::LFO_90_OUTPUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(95, 345), module, BPMLFO2::LFO_180_OUTPUT));



		addChild(createLight<LargeLight<RedLight>>(Vec(100, 313.5), module, BPMLFO2::HOLD_LIGHT));
	}
};



Model *modelBPMLFO2 = createModel<BPMLFO2, BPMLFO2Widget>("BPMLFO2");
