#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

struct PNChordExpander : Module {
	enum ParamIds {
        // MIN_NOTE_PARAM,
		// MIN_NOTE_CV_ATTENUVERTER_PARAM,
        // MAX_NOTE_PARAM,
		// MAX_NOTE_CV_ATTENUVERTER_PARAM,
        // NOTE_NUMBER_BALANCE_PARAM,
		// NOTE_NUMBER_BALANCE_CV_ATTENUVERTER_PARAM,
		DISSONANCE5_PROBABILITY_PARAM,
		DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM,
		DISSONANCE7_PROBABILITY_PARAM,
		DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM,
		SUSPENSIONS_PROBABILITY_PARAM,
		SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM,
		INVERSION_PROBABILITY_PARAM,
		INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
        // MIN_NOTE_PARAM,
        // MAX_NOTE_PARAM,
        // NOTE_NUMBER_BALANCE_PARAM,
		DISSONANCE5_PROBABILITY_INPUT,
		DISSONANCE7_PROBABILITY_INPUT,
		SUSPENSIONS_PROBABILITY_INPUT,
		INVERSION_PROBABILITY_INPUT,
		DISSONANCE5_EXTERNAL_RANDOM_INPUT,
		DISSONANCE7_EXTERNAL_RANDOM_INPUT,
		SUSPENSIONS_EXTERNAL_RANDOM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		// TEST1_OUTPUT,
		// TEST2_OUTPUT,
		// TEST3_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	float dissonance5Probability,dissonance7Probability,suspensionProbability;
	
	// Expander
	float consumerMessage[12] = {};// this module must read from here
	float producerMessage[12] = {};// mother will write into here


	float thirdOffset,fifthOffset,seventhOffset;
	
	
	PNChordExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
	
		configParam(DISSONANCE5_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Dissonance V Probability","%",0,100);
		configParam(DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Dissonance V Probability CV Attenuation","%",0,100);

		configParam(DISSONANCE7_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Dissonance VII Probability","%",0,100);
		configParam(DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Dissonance VII Probability CV Attenuation","%",0,100);

		configParam(SUSPENSIONS_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Suspensions Probability","%",0,100);
		configParam(SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Suspensions Probability CV Attenuation","%",0,100);

		configParam(INVERSION_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Inversions Probability","%",0,100);
		configParam(INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Inverions Probability CV Attenuation","%",0,100);

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;


    }

	

	void process(const ProcessArgs &args) override {
		
		bool motherPresent = (leftExpander.module && leftExpander.module->model == modelProbablyNote);
		if (motherPresent) {
			// To Mother
			//float *producerMessage = (float*) leftExpander.producerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

			dissonance5Probability = clamp(params[DISSONANCE5_PROBABILITY_PARAM].getValue() + (inputs[DISSONANCE5_PROBABILITY_INPUT].isConnected() ? inputs[DISSONANCE5_PROBABILITY_INPUT].getVoltage() / 10 * params[DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			dissonance7Probability = clamp(params[DISSONANCE7_PROBABILITY_PARAM].getValue() + (inputs[DISSONANCE7_PROBABILITY_INPUT].isConnected() ? inputs[DISSONANCE7_PROBABILITY_INPUT].getVoltage() / 10 * params[DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			suspensionProbability = clamp(params[SUSPENSIONS_PROBABILITY_PARAM].getValue() + (inputs[SUSPENSIONS_PROBABILITY_INPUT].isConnected() ? inputs[SUSPENSIONS_PROBABILITY_INPUT].getVoltage() / 10 * params[SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);

			messagesToMother[0] = dissonance5Probability;
			messagesToMother[1] = dissonance7Probability;
			messagesToMother[2] = suspensionProbability;
			//messagesToMother[3] = clamp(params[INVERSION_PROBABILITY_PARAM].getValue() + (inputs[INVERSION_PROBABILITY_INPUT].isConnected() ? inputs[INVERSION_PROBABILITY_INPUT].getVoltage() / 10 * params[INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			messagesToMother[4] = inputs[DISSONANCE5_EXTERNAL_RANDOM_INPUT].isConnected() ? inputs[DISSONANCE5_EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f : -1;
			messagesToMother[5] = inputs[DISSONANCE7_EXTERNAL_RANDOM_INPUT].isConnected() ? inputs[DISSONANCE7_EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f : -1;
			messagesToMother[6] = inputs[SUSPENSIONS_EXTERNAL_RANDOM_INPUT].isConnected() ? inputs[SUSPENSIONS_EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f : -1;

			leftExpander.module->rightExpander.messageFlipRequested = true;


			// From Mother	
			float *messagesFromMother = (float*)leftExpander.consumerMessage;		
			thirdOffset = messagesFromMother[0]; 
			fifthOffset = messagesFromMother[1]; 
			seventhOffset = messagesFromMother[2]; 

			//thirdOffset = 1;
			
			//leftExpander.messageFlipRequested = true;
			
					
		} else {
			thirdOffset = 2.0f;
			fifthOffset = 2.0f;
			seventhOffset = 2.0f;
		}	
		// outputs[TEST1_OUTPUT].setVoltage(thirdOffset);
		// outputs[TEST2_OUTPUT].setVoltage(fifthOffset);
		// outputs[TEST3_OUTPUT].setVoltage(seventhOffset);
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


struct PNChordExpanderDisplay : TransparentWidget {
	PNChordExpander *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	PNChordExpanderDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
	}

	void drawChordRange(const DrawArgs &args, float y, float chordProbability,float selectedChord) 
	{		
		// Draw indicator

			//float opacity = std::max(chordProbability-0.5,0.0) * 511;
			float opacity = std::min(chordProbability*511.0,255.0);
			if(selectedChord == -1.0f) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			} else {
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			}

			nvgBeginPath(args.vg);
			nvgRect(args.vg,3,y,18.0,20.0);
			nvgClosePath(args.vg);		
			nvgFill(args.vg);

			if(selectedChord == 1.0f) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			} else {
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			}

			nvgBeginPath(args.vg);
			nvgRect(args.vg,39,y,18.0,20.0);
			nvgClosePath(args.vg);		
			nvgFill(args.vg);

			opacity = std::min((1.0 - chordProbability)*511.0,255.0);
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));

			if(selectedChord == 0.0f) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			} else {
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			}

			nvgBeginPath(args.vg);
			nvgRect(args.vg,21,y,18.0,20.0);
			nvgClosePath(args.vg);		
			nvgFill(args.vg);

			// nvgFontSize(args.vg, 8);
			// nvgFontFaceId(args.vg, font->handle);
			// nvgTextLetterSpacing(args.vg, -1);
			// nvgFillColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));

			// char text[128];
			// snprintf(text, sizeof(text), "%s", module->noteNames[actualTarget]);
			// x= notePosition[i][0];
			// y= notePosition[i][1];
			// int align = (int)notePosition[i][2];

			// nvgTextAlign(args.vg,align);
			// nvgText(args.vg, x, y, text, NULL);

	

	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return; 

		drawChordRange(args, 54, module->dissonance5Probability,module->fifthOffset);
		drawChordRange(args, 154, module->dissonance7Probability,module->seventhOffset);
		drawChordRange(args, 254, module->suspensionProbability,module->thirdOffset);
	}
};

struct PNChordExpanderWidget : ModuleWidget {
	PNChordExpanderWidget(PNChordExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PNChordExpander.svg")));

		{
			PNChordExpanderDisplay *display = new PNChordExpanderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		addParam(createParam<RoundSmallFWKnob>(Vec(8,75), module, PNChordExpander::DISSONANCE5_PROBABILITY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,101), module, PNChordExpander::DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 79), module, PNChordExpander::DISSONANCE5_PROBABILITY_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(8,175), module, PNChordExpander::DISSONANCE7_PROBABILITY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,201), module, PNChordExpander::DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 179), module, PNChordExpander::DISSONANCE7_PROBABILITY_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(8,275), module, PNChordExpander::SUSPENSIONS_PROBABILITY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,301), module, PNChordExpander::SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 279), module, PNChordExpander::SUSPENSIONS_PROBABILITY_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(8, 115), module, PNChordExpander::DISSONANCE5_EXTERNAL_RANDOM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(8, 215), module, PNChordExpander::DISSONANCE7_EXTERNAL_RANDOM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(8, 315), module, PNChordExpander::SUSPENSIONS_EXTERNAL_RANDOM_INPUT));

		// addOutput(createOutput<FWPortInSmall>(Vec(10, 345),  module, PNChordExpander::TEST1_OUTPUT));
		// addOutput(createOutput<FWPortInSmall>(Vec(30, 345),  module, PNChordExpander::TEST2_OUTPUT));
		// addOutput(createOutput<FWPortInSmall>(Vec(50, 345),  module, PNChordExpander::TEST3_OUTPUT));

		// addParam(createParam<RoundSmallFWKnob>(Vec(8,270), module, PNChordExpander::INVERSION_PROBABILITY_PARAM));			
        // addParam(createParam<RoundReallySmallFWKnob>(Vec(34,296), module, PNChordExpander::INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		// addInput(createInput<FWPortInSmall>(Vec(36, 274), module, PNChordExpander::INVERSION_PROBABILITY_INPUT));


	}
};

Model *modelPNChordExpander = createModel<PNChordExpander, PNChordExpanderWidget>("PNChordExpander");
