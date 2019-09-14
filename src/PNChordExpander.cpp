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
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	
	// Expander
	float consumerMessage[8] = {};// this module must read from here
	float producerMessage[8] = {};// mother will write into here

	
	
	PNChordExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;

		

		configParam(DISSONANCE5_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Dissonance V Probability","%",0,100);
		configParam(DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Dissonance V Probability CV Attenuation","%",0,100);

		configParam(DISSONANCE7_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Dissonance VII Probability","%",0,100);
		configParam(DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Dissonance VII Probability CV Attenuation","%",0,100);

		configParam(SUSPENSIONS_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Suspensions Probability","%",0,100);
		configParam(SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Suspensions Probability CV Attenuation","%",0,100);

		configParam(INVERSION_PROBABILITY_PARAM, 0.0f, 1.0f, 0.0f,"Inversions Probability","%",0,100);
		configParam(INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Inverions Probability CV Attenuation","%",0,100);

    }

	

	void process(const ProcessArgs &args) override {
		
	

		bool motherPresent = (leftExpander.module && leftExpander.module->model == modelProbablyNote);
		if (motherPresent) {
			// To Mother
			float *producerMessage = (float*) leftExpander.producerMessage;

			producerMessage[0] = clamp(params[DISSONANCE5_PROBABILITY_PARAM].getValue() + (inputs[DISSONANCE5_PROBABILITY_INPUT].isConnected() ? inputs[DISSONANCE5_PROBABILITY_INPUT].getVoltage() / 10 * params[DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			producerMessage[1] = clamp(params[DISSONANCE7_PROBABILITY_PARAM].getValue() + (inputs[DISSONANCE7_PROBABILITY_INPUT].isConnected() ? inputs[DISSONANCE7_PROBABILITY_INPUT].getVoltage() / 10 * params[DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			producerMessage[2] = clamp(params[SUSPENSIONS_PROBABILITY_PARAM].getValue() + (inputs[SUSPENSIONS_PROBABILITY_INPUT].isConnected() ? inputs[SUSPENSIONS_PROBABILITY_INPUT].getVoltage() / 10 * params[SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			//producerMessage[3] = clamp(params[INVERSION_PROBABILITY_PARAM].getValue() + (inputs[INVERSION_PROBABILITY_INPUT].isConnected() ? inputs[INVERSION_PROBABILITY_INPUT].getVoltage() / 10 * params[INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM].getValue() : 0.0f),0.0,1.0f);
			producerMessage[4] = inputs[DISSONANCE5_EXTERNAL_RANDOM_INPUT].isConnected() ? inputs[DISSONANCE5_EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f : -1;
			producerMessage[5] = inputs[DISSONANCE7_EXTERNAL_RANDOM_INPUT].isConnected() ? inputs[DISSONANCE7_EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f : -1;
			producerMessage[6] = inputs[SUSPENSIONS_EXTERNAL_RANDOM_INPUT].isConnected() ? inputs[SUSPENSIONS_EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f : -1;
		
						
			// From Mother	
			
			
			leftExpander.messageFlipRequested = true;
		
		}		
		
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

struct PNChordExpanderWidget : ModuleWidget {
	PNChordExpanderWidget(PNChordExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PNChordExpander.svg")));

		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		addParam(createParam<RoundSmallFWKnob>(Vec(8,75), module, PNChordExpander::DISSONANCE5_PROBABILITY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,101), module, PNChordExpander::DISSONANCE5_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 79), module, PNChordExpander::DISSONANCE5_PROBABILITY_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(8,155), module, PNChordExpander::DISSONANCE7_PROBABILITY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,181), module, PNChordExpander::DISSONANCE7_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 159), module, PNChordExpander::DISSONANCE7_PROBABILITY_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(8,235), module, PNChordExpander::SUSPENSIONS_PROBABILITY_PARAM));			
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,261), module, PNChordExpander::SUSPENSIONS_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 239), module, PNChordExpander::SUSPENSIONS_PROBABILITY_INPUT));

		addInput(createInput<FWPortInSmall>(Vec(8, 115), module, PNChordExpander::DISSONANCE5_EXTERNAL_RANDOM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(8, 195), module, PNChordExpander::DISSONANCE7_EXTERNAL_RANDOM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(8, 275), module, PNChordExpander::SUSPENSIONS_EXTERNAL_RANDOM_INPUT));

		// addParam(createParam<RoundSmallFWKnob>(Vec(8,270), module, PNChordExpander::INVERSION_PROBABILITY_PARAM));			
        // addParam(createParam<RoundReallySmallFWKnob>(Vec(34,296), module, PNChordExpander::INVERSION_PROBABILITY_CV_ATTENUVERTER_PARAM));			
		// addInput(createInput<FWPortInSmall>(Vec(36, 274), module, PNChordExpander::INVERSION_PROBABILITY_INPUT));


	}
};

Model *modelPNChordExpander = createModel<PNChordExpander, PNChordExpanderWidget>("PNChordExpander");
