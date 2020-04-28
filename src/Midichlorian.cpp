#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define TRACK_COUNT 4
#define MAX_STEPS 18
#define NUM_TAPS 16
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 8
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 9
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * 3 + TRACK_LEVEL_PARAM_COUNT


struct Midichlorian : Module {

    enum ParamIds {
		NUM_PARAMS
	};

	enum InputIds {
        FREQ_CV_INPUT,
        NUM_INPUTS
	};

	enum OutputIds {
		MIDI_OUT,
		PITCH_BEND_OUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		NUM_LIGHTS 
	};

	
	
	Midichlorian() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		
        onReset();
	}


	void process(const ProcessArgs &args) override {
		
        
        uint8_t nbrChannels = inputs[FREQ_CV_INPUT].getChannels();

        outputs[MIDI_OUT].setChannels(nbrChannels);
        outputs[PITCH_BEND_OUT].setChannels(nbrChannels);
        for(uint8_t channel = 0;channel < nbrChannels; channel++) {
            float inputCV = inputs[FREQ_CV_INPUT].getVoltage(channel);
            float note = inputCV * 12.0;
            float pitchBend = (note - floor(note)) * 5.0;

            outputs[MIDI_OUT].setVoltage(floor(note) / 12,channel);
            outputs[PITCH_BEND_OUT].setVoltage(pitchBend,channel);
        }

	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

};




struct MidichlorianWidget : ModuleWidget {
	MidichlorianWidget(Midichlorian *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Midichlorian.svg")));

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	   

        addInput(createInput<FWPortInSmall>(Vec(14.5, 184), module, Midichlorian::FREQ_CV_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(14.5, 278), module, Midichlorian::MIDI_OUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(14.5, 328), module, Midichlorian::PITCH_BEND_OUT));


        

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		


	}
};

Model *modelMidichlorian = createModel<Midichlorian, MidichlorianWidget>("Midichlorian");
 