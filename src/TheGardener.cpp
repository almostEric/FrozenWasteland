#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define MAX_DELAY 10
#define VCV_POLYPHONY 16

struct TheGardener : Module {

    enum ParamIds {
		NUMBER_STEPS_RESEED_PARAM,
        NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM,
		NUMBER_STEPS_NEW_SEED_PARAM,
        NUMBER_STEPS_NEW_SEED_CV_ATTENUVERTER_PARAM,
		SEED_PROCESS_DELAY_COMPENSATION_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        SEED_INPUT,
        NUMBER_STEPS_RESEED_CV_INPUT,
        NUMBER_STEPS_NEW_SEED_CV_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
        CLOCK_OUTPUT,
        SEED_OUTPUT,
        RESEED_OUTPUT,
		NEW_SEED_TRIGGER_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		NUM_LIGHTS 
	};




	
	dsp::SchmittTrigger clockTrigger,resetTrigger;
	dsp::PulseGenerator reseedPulse, newSeedPulse;
    int reseedSteps, newSeedSteps;
	int reseedCount, newSeedCount;
	float reseedProgress = 0, newSeedProgress = 0;
	float clockIn;
	float seedIn[VCV_POLYPHONY];
	float newSeedOut[VCV_POLYPHONY];
	bool resetTriggered = false;
	bool reseedNeeded = false;
	bool newSeedNeeded = false;
	int sampleDelayCount = 0;
	int sampleDelay = 0;

	int nbrChannels = 0;
	bool polyModeDetected = false;

	float delayedClockOut;
	float clockDelayLine[MAX_DELAY+1];
	int delayIndex = 0;
	

	TheGardener() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
        
        configParam(NUMBER_STEPS_RESEED_PARAM, 1.0, 999.0, 1.0,"Reseed Division");
        configParam(NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Reseed Division CV Attenuation","%",0,100);

        configParam(NUMBER_STEPS_NEW_SEED_PARAM, 1.0f, 999.0, 1.0,"New Seed S&H Division");
        configParam(NUMBER_STEPS_NEW_SEED_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"New Seed S&H Division CV Attenuation","%",0,100);

        configParam(SEED_PROCESS_DELAY_COMPENSATION_PARAM, 0.0, 10.0, 0.0,"Seed Process Delay Compensation");


		reseedCount = 0;
		newSeedCount = 0;

		//Clear out delay
		for(int i=0;i<=MAX_DELAY;i++) {
			clockDelayLine[i] = 0.0;
		}	
	}

	void process(const ProcessArgs &args) override {
		sampleDelay = params[SEED_PROCESS_DELAY_COMPENSATION_PARAM].getValue();
        reseedSteps = params[NUMBER_STEPS_RESEED_PARAM].getValue() + inputs[NUMBER_STEPS_RESEED_CV_INPUT].getVoltage() * params[NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM].getValue() / 10.0f; 
        newSeedSteps = params[NUMBER_STEPS_NEW_SEED_PARAM].getValue() + inputs[NUMBER_STEPS_NEW_SEED_CV_INPUT].getVoltage() * params[NUMBER_STEPS_NEW_SEED_CV_ATTENUVERTER_PARAM].getValue() / 10.0f; 
			
		nbrChannels = inputs[SEED_INPUT].getChannels();
		polyModeDetected = nbrChannels > 1;
		for(int i=0;i<nbrChannels;i++) {
			seedIn[i] = inputs[SEED_INPUT].getVoltage(i);
		}

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			reseedCount = 0;
			newSeedCount = 0;
			reseedProgress = 0.0;
			newSeedProgress = 0.0;
			resetTriggered = true;				
		}

		clockIn = inputs[CLOCK_INPUT].getVoltage();
		clockDelayLine[(delayIndex + sampleDelay + (polyModeDetected ? 1 : 0)) % MAX_DELAY] = clockIn;
		delayedClockOut = clockDelayLine[delayIndex];
		delayIndex = (delayIndex+1) % MAX_DELAY;


		if (clockTrigger.process(clockIn)) {
			if(reseedCount >= reseedSteps || resetTriggered) {
				reseedCount = 0;
				reseedNeeded = true;
				sampleDelayCount = 0;
			}
			
			if(newSeedCount >= newSeedSteps || resetTriggered) {
				newSeedPulse.trigger(1e-3);
				reseedNeeded = true;
				newSeedNeeded = true;
				newSeedCount = 0;
				sampleDelayCount = 0;
			}
			resetTriggered = false;

			reseedCount +=1;
			newSeedCount +=1;

			reseedProgress = (float)reseedCount / reseedSteps;
			newSeedProgress = (float)newSeedCount / newSeedSteps;

		}


		if(sampleDelayCount >= sampleDelay) {
			if(newSeedNeeded) {
				for(int i=0;i<nbrChannels;i++) {
					newSeedOut[i] = seedIn[i];
				}
				newSeedNeeded = false;
			}
			if(reseedNeeded && (!polyModeDetected || sampleDelayCount >= sampleDelay+1)) {
				reseedPulse.trigger(1e-3);
				reseedNeeded = false;
			}
			if(!newSeedNeeded && !reseedNeeded)
				sampleDelayCount = 0;
		}

			
		outputs[CLOCK_OUTPUT].setVoltage(delayedClockOut);		
		outputs[RESEED_OUTPUT].setVoltage(reseedPulse.process(1.0 / args.sampleRate) ? 10.0 : 0);				
		outputs[NEW_SEED_TRIGGER_OUTPUT].setVoltage(newSeedPulse.process(1.0 / args.sampleRate) ? 10.0 : 0);		

		outputs[SEED_OUTPUT].setChannels(nbrChannels);
		for(int i=0;i<nbrChannels;i++) {
			outputs[SEED_OUTPUT].setVoltage(newSeedOut[i],i);	
		}	

		sampleDelayCount +=1;

	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

    
};



struct TheGardenerDisplay : TransparentWidget {
	TheGardener *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	TheGardenerDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}



	void drawSteps(const DrawArgs &args, Vec pos, int steps) {
		nvgFontSize(args.vg, 17);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
        nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
        snprintf(text, sizeof(text), " %i", steps);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawProgress(const DrawArgs &args, Vec pos, float progress) {
		nvgBeginPath(args.vg);
		nvgStrokeWidth(args.vg, 0.0);
		nvgRect(args.vg,pos.x,pos.y,progress*40.0,5);
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		nvgFill(args.vg);
	}


	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawSteps(args,Vec(44.0,55),(int)(module->reseedSteps));
		drawSteps(args,Vec(110.0,55),(int)(module->newSeedSteps));

		drawProgress(args,Vec(6.0,58),(float)(module->reseedProgress));
		drawProgress(args,Vec(72.0,58),(float)(module->newSeedProgress));
	}
};

struct TheGardenerWidget : ModuleWidget {
	TheGardenerWidget(TheGardener *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TheGardener.svg")));

		{
			TheGardenerDisplay *display = new TheGardenerDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}




        addParam(createParam<RoundSmallFWSnapKnob>(Vec(5, 67), module, TheGardener::NUMBER_STEPS_RESEED_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(35, 69), module, TheGardener::NUMBER_STEPS_RESEED_CV_INPUT));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34, 90), module, TheGardener::NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(65, 67), module, TheGardener::NUMBER_STEPS_NEW_SEED_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(95, 69), module, TheGardener::NUMBER_STEPS_NEW_SEED_CV_INPUT));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94, 90), module, TheGardener::NUMBER_STEPS_NEW_SEED_CV_ATTENUVERTER_PARAM));
	
	   
		addParam(createParam<RoundSmallFWSnapKnob>(Vec(14, 308), module, TheGardener::SEED_PROCESS_DELAY_COMPENSATION_PARAM));


        addInput(createInput<FWPortInSmall>(Vec(14, 155), module, TheGardener::RESET_INPUT));
        
		addInput(createInput<FWPortInSmall>(Vec(14, 212), module, TheGardener::CLOCK_INPUT));
        addOutput(createOutput<FWPortOutSmall>(Vec(75, 212), module, TheGardener::CLOCK_OUTPUT));
		
        addInput(createInput<FWPortInSmall>(Vec(14, 254), module, TheGardener::SEED_INPUT));
        addOutput(createOutput<FWPortOutSmall>(Vec(75, 254), module, TheGardener::SEED_OUTPUT));
        
        
        addOutput(createOutput<FWPortOutSmall>(Vec(75, 295), module, TheGardener::RESEED_OUTPUT));
        addOutput(createOutput<FWPortOutSmall>(Vec(75, 334), module, TheGardener::NEW_SEED_TRIGGER_OUTPUT));

	
	}
};

Model *modelTheGardener = createModel<TheGardener, TheGardenerWidget>("TheGardener");
 