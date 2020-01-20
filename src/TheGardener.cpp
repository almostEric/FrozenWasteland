#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"



struct TheGardener : Module {

    enum ParamIds {
		NUMBER_STEPS_RESEED_PARAM,
        NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM,
		NUMBER_STEPS_CLOCK_PARAM,
        NUMBER_STEPS_CLOCK_CV_ATTENUVERTER_PARAM,
		RESEED_DELAY_PARAM,
		CLOCK_DELAY_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        CV_INPUT,
        NUMBER_STEPS_RESEED_CV_INPUT,
        NUMBER_STEPS_CLOCK_CV_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
        CLOCK_OUTPUT,
        RESEED_OUTPUT,
        CV_OUTPUT,
        STEP_HOLD_TRIGGER_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		NUM_LIGHTS 
	};




	
	dsp::SchmittTrigger clockTrigger,resetTrigger;
	dsp::PulseGenerator reseedPulse,clockPulse;
    int reseedSteps, clockSteps, reseedDelay, clockDelay;
	int reseedCount, clockCount, reseedDelayCount, clockDelayCount;
	bool reseedTriggered, clockTriggered;
	float seedIn;
	float newCVOut;

	
	TheGardener() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
        
        configParam(NUMBER_STEPS_RESEED_PARAM, 1.0, 128.0, 1.0,"Reseed Steps");
        configParam(NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Reseed Steps CV Attenuation","%",0,100);

        configParam(NUMBER_STEPS_CLOCK_PARAM, 1.0f, 128.0, 1.0,"Clock Steps");
        configParam(NUMBER_STEPS_CLOCK_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Clock Steps CV Attenuation","%",0,100);

        configParam(RESEED_DELAY_PARAM, 0.0, 10.0, 0.0,"# Samples to delay Reseed Trigger");
        configParam(CLOCK_DELAY_PARAM, 0.0, 10.0, 0.0,"# Samples to delay Clock Trigger");

		reseedCount = 0;
		clockCount = 0;

	
	}

	void process(const ProcessArgs &args) override {
        reseedSteps = params[NUMBER_STEPS_RESEED_PARAM].getValue() + inputs[NUMBER_STEPS_RESEED_CV_INPUT].getVoltage() * params[NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM].getValue() / 10.0f; 
        clockSteps = params[NUMBER_STEPS_CLOCK_PARAM].getValue() + inputs[NUMBER_STEPS_CLOCK_CV_INPUT].getVoltage() * params[NUMBER_STEPS_CLOCK_CV_ATTENUVERTER_PARAM].getValue() / 10.0f; 
			
        reseedDelay = params[RESEED_DELAY_PARAM].getValue();
        clockDelay = params[CLOCK_DELAY_PARAM].getValue();

		seedIn = inputs[CV_INPUT].getVoltage();

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			reseedCount = 0;
			clockCount = 0;
			reseedDelayCount = 0;
			clockDelayCount = 0;
			reseedTriggered = false;
			clockTriggered = false;
		}


		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			reseedCount +=1;
			clockCount +=1;

			if(reseedCount >= reseedSteps) {
				reseedCount = 0;
				reseedDelayCount = -1;
				reseedTriggered = true;
			}
			if(clockCount >= clockSteps) {
				clockCount = 0;
				clockDelayCount = -1;
				clockTriggered = true;
			}
		}

		if(reseedTriggered) {
			reseedDelayCount +=1;
			if(reseedDelayCount >= reseedDelay) {
				newCVOut = seedIn;
				reseedPulse.trigger(1e-3);
				reseedDelayCount = 0;
				reseedTriggered = false;	
			}
		}

		if(clockTriggered) {
			clockDelayCount +=1;
			if(clockDelayCount >= clockDelay) {
				clockPulse.trigger(1e-3);
				clockDelayCount = 0;
				clockTriggered = false;
			}
		}

			
		outputs[RESEED_OUTPUT].setVoltage(reseedPulse.process(1.0 / args.sampleRate) ? 10.0 : 0);				
		outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(1.0 / args.sampleRate) ? 10.0 : 0);		

		outputs[CV_OUTPUT].setVoltage(newCVOut);		

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
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}



	void drawSteps(const DrawArgs &args, Vec pos, int steps) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
        nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
        snprintf(text, sizeof(text), " %i", steps);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void drawDelay(const DrawArgs &args, Vec pos, int delay) {
		nvgFontSize(args.vg, 12);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
        nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
        snprintf(text, sizeof(text), " %i", delay);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}



	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawSteps(args,Vec(44.0,53),(int)(module->reseedSteps));
		drawSteps(args,Vec(110.0,53),(int)(module->clockSteps));
		drawDelay(args,Vec(50.0,161),(int)(module->reseedDelay));
		drawDelay(args,Vec(111.0,161),(int)(module->clockDelay));
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


		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		




        addParam(createParam<RoundSmallFWSnapKnob>(Vec(5, 65), module, TheGardener::NUMBER_STEPS_RESEED_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(35, 67), module, TheGardener::NUMBER_STEPS_RESEED_CV_INPUT));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34, 90), module, TheGardener::NUMBER_STEPS_RESEED_CV_ATTENUVERTER_PARAM));

        addParam(createParam<RoundSmallFWSnapKnob>(Vec(65, 65), module, TheGardener::NUMBER_STEPS_CLOCK_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(95, 67), module, TheGardener::NUMBER_STEPS_CLOCK_CV_INPUT));
        addParam(createParam<RoundReallySmallFWKnob>(Vec(94, 90), module, TheGardener::NUMBER_STEPS_CLOCK_CV_ATTENUVERTER_PARAM));


        addParam(createParam<RoundReallySmallFWSnapKnob>(Vec(5, 146), module, TheGardener::RESEED_DELAY_PARAM));
        addParam(createParam<RoundReallySmallFWSnapKnob>(Vec(65, 146), module, TheGardener::CLOCK_DELAY_PARAM));
	
	   
        addInput(createInput<FWPortInSmall>(Vec(10, 260), module, TheGardener::RESET_INPUT));
        
        addInput(createInput<FWPortInSmall>(Vec(40, 300), module, TheGardener::CV_INPUT));
        addOutput(createOutput<FWPortOutSmall>(Vec(95, 300), module, TheGardener::CV_OUTPUT));
        
        addInput(createInput<FWPortInSmall>(Vec(15, 340), module, TheGardener::CLOCK_INPUT));
        addOutput(createOutput<FWPortOutSmall>(Vec(55, 340), module, TheGardener::CLOCK_OUTPUT));
        addOutput(createOutput<FWPortOutSmall>(Vec(95, 340), module, TheGardener::RESEED_OUTPUT));


        // addChild(createLight<LargeLight<BlueLight>>(Vec(13.5, 313.5), module, TheGardener::USING_DIVS_LIGHT));
        // addChild(createLight<LargeLight<GreenLight>>(Vec(27.5, 342.5), module, TheGardener::GROOVE_IS_TRACK_LENGTH_LIGHT));
        // addChild(createLight<LargeLight<GreenLight>>(Vec(176.5, 352.5), module, TheGardener::GAUSSIAN_DISTRIBUTION_LIGHT));

        

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		


	}
};

Model *modelTheGardener = createModel<TheGardener, TheGardenerWidget>("TheGardener");
 