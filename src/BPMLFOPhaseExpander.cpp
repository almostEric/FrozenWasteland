//NOTE Because I am tipsy
//BPM based modulation of phase. Not sure if it means anything other than 2 BPM LFOS :)

#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define MAX_OUTPUTS 12

#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 13


struct BPMLFOPhaseExpander : Module {
	enum ParamIds {
		PHASE_DIVISION_PARAM,
		PHASE_DIVISION_CV_ATTENUVERTER_PARAM,
		FORCE_INTEGER_PARAM,
		WAVESHAPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PHASE_DIVISION_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LFO_1_OUTPUT,
		NUM_OUTPUTS = MAX_OUTPUTS
	};
	enum LightIds {
		FORCE_INTEGER_LIGHT,
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


	LowFrequencyOscillator oscillator;
	dsp::SchmittTrigger clockTrigger,resetTrigger,holdTrigger,forceIntegerTrigger;
	float multiplier = 1;
	float division = 1;
	float phaseDivision = 3;
	bool forceInteger = true;
	double timeElapsed = 0.0;
	double duration = 0;
	float waveshape = 0;
	float waveSlope = 0.0;
	float skew = 0.5;
	float initialPhase = 0.0;
	bool holding = false;
	bool firstClockReceived = false;
	bool secondClockReceived = false;
	bool phase_quantized = false;

	float lfoOutputValue[MAX_OUTPUTS] = {0.0};

	float lastWaveShape = -1;
	float lastWaveSlope = -1;
	float lastSkew = -1;
	
	//percentages
	float phaseDivisionPercentage = 0;
	float waveShapePercentage = 0;
	


	BPMLFOPhaseExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PHASE_DIVISION_PARAM, 3.0, 12.0, 3.0,"Phase Division");
		configParam(PHASE_DIVISION_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Phase Division CV Attenuation","%",0,100);
		configParam(WAVESHAPE_PARAM, 1.0, 5.0, 1.0,"Wave Shape");

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
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


void BPMLFOPhaseExpander::process(const ProcessArgs &args) {

	bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelBPMLFO || leftExpander.module->model == modelBPMLFO2 || leftExpander.module->model == modelBPMLFOPhaseExpander));
	//lights[CONNECTED_LIGHT].value = motherPresent;
	if (!motherPresent) {
		return;
	}

											
	// From Mother	
	float *messagesFromMother = (float*)leftExpander.consumerMessage;
	float clockConnected = messagesFromMother[0];
	float clockInput = messagesFromMother[1];
	float resetInput = messagesFromMother[2];
	float holdInput = messagesFromMother[3];
	multiplier = messagesFromMother[4];
	division = messagesFromMother[5];
	initialPhase = messagesFromMother[6];
	float offset = messagesFromMother[7];
	float holdMode = messagesFromMother[8];
	float holdClockMode = messagesFromMother[9];
	waveshape = messagesFromMother[10];
	waveSlope = messagesFromMother[11];
	skew = messagesFromMother[12]; 

	

	//If another expander is present, pass values on to it
	bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelBPMLFOPhaseExpander));
	if(anotherExpanderPresent)
	{					
		//QAR Pass through right
		float *messageToSlave = (float*)(rightExpander.module->leftExpander.producerMessage);	
		for(int i = 0; i < PASSTHROUGH_RIGHT_VARIABLE_COUNT;i++) {
			messageToSlave[i] = messagesFromMother[i];
		}	
	}
	
	leftExpander.messageFlipRequested = true;

	

    timeElapsed += 1.0 / args.sampleRate;
	if(clockConnected) {
		if(clockTrigger.process(clockInput)) {
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
	
	if (forceIntegerTrigger.process(params[FORCE_INTEGER_PARAM].getValue())) {
		forceInteger = !forceInteger;
	}
	lights[FORCE_INTEGER_LIGHT].value = forceInteger;

	phaseDivision = params[PHASE_DIVISION_PARAM].getValue() + (inputs[PHASE_DIVISION_INPUT].getVoltage() * params[PHASE_DIVISION_CV_ATTENUVERTER_PARAM].getValue());
	phaseDivisionPercentage = (phaseDivision - 3.0) / 9.0;
	if(forceInteger) {
		phaseDivision = std::floor(phaseDivision);
	}
	phaseDivision = clamp(phaseDivision,3.0,12.0f);	
	
	oscillator.offset = (offset > 0.0);
	oscillator.setBasePhase(initialPhase);
	oscillator.waveSlope = waveSlope;
	oscillator.skew = skew;
	oscillator.setPulseWidth(skew);

	//Recalcluate display waveform if something changed
	if(lastWaveShape != waveshape || lastWaveSlope != waveSlope || lastSkew != skew) {
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

	if(resetTrigger.process(resetInput)) {
		oscillator.hardReset();
	}		

	if(holdMode == 1.0) { //Latched is default		
		if(holdTrigger.process(holdInput)) {
			holding = !holding;
		}		
	} else {
		holding = holdInput >= 1; 			
	}
		

    if(!holding || (holding && holdClockMode == 0.0)) {
    	oscillator.step(1.0 / args.sampleRate);
    }

	if(!holding) {
		for(int i = 0; i<phaseDivision; i++) {
			float phase = (float)i/phaseDivision;
			if(waveshape == SKEWSAW_WAV) {
				lfoOutputValue[i] = 5.0 * oscillator.skewsaw(phase);
			}
			else {
				lfoOutputValue[i] = 5.0 * oscillator.sqr(phase);
			}		
		}
	}
	
	for(int i=0;i<MAX_OUTPUTS;i++) {
		outputs[LFO_1_OUTPUT+i].setVoltage(lfoOutputValue[i]);
	}
}

struct BPMLFOPhaseDisplay : TransparentWidget {
	BPMLFOPhaseExpander *module;
	std::shared_ptr<Font> font;

	BPMLFOPhaseDisplay() {
		//font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		nvgFontSize(args.vg, 8);
		nvgFontFaceId(args.vg, font->handle);
		nvgStrokeWidth(args.vg, 2);
		nvgTextLetterSpacing(args.vg, 0);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		//static const int portX0[4] = {20, 63, 106, 149};
		for (int i=0; i < module->phaseDivision; i++) {
			char fVal[10];
			snprintf(fVal, sizeof(fVal), "%1i", (int)std::round((float)i / module->phaseDivision * 360.0) );
			nvgText(args.vg, 0, i*22, fVal, NULL);
		}
	}
};

struct BPMLFOPhaseExpanderWidget : ModuleWidget {
	BPMLFOPhaseExpanderWidget(BPMLFOPhaseExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BPMLFOPhaseExpander.svg")));

		BPMLFOPhaseDisplay *phaseDisplay = new BPMLFOPhaseDisplay();
		phaseDisplay->module = module;
		phaseDisplay->box.pos = Vec(40, 112);
		phaseDisplay->box.size = Vec(50, 310);
		addChild(phaseDisplay);
		

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		
		ParamWidget* phaseDivisionParam = createParam<RoundSmallFWKnob>(Vec(4, 36), module, BPMLFOPhaseExpander::PHASE_DIVISION_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(phaseDivisionParam)->percentage = &module->phaseDivisionPercentage;
		}
		addParam(phaseDivisionParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(33, 61), module, BPMLFOPhaseExpander::PHASE_DIVISION_CV_ATTENUVERTER_PARAM));

		addParam(createParam<LEDButton>(Vec(7, 72), module, BPMLFOPhaseExpander::FORCE_INTEGER_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(35, 40), module, BPMLFOPhaseExpander::PHASE_DIVISION_INPUT));

        for(int i= 0;i<MAX_OUTPUTS;i++) {
		    addOutput(createOutput<FWPortOutSmall>(Vec(5, 100 + i * 22), module, BPMLFOPhaseExpander::LFO_1_OUTPUT + i));
        }

		addChild(createLight<LargeLight<BlueLight>>(Vec(8.5, 73.5), module, BPMLFOPhaseExpander::FORCE_INTEGER_LIGHT));	
	}
};



Model *modelBPMLFOPhaseExpander = createModel<BPMLFOPhaseExpander, BPMLFOPhaseExpanderWidget>("BPMLFOPhaseExpander");
