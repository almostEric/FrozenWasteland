#include "FrozenWasteland.hpp"
#include "ui/ports.hpp"

#define NUM_TAPS 16


struct PWTapBreakoutExpander : Module {

    enum ParamIds {
		NUM_PARAMS
	};

	enum InputIds {
		TAP_PRE_FB_INPUT_L,
		TAP_PRE_FB_INPUT_R = TAP_PRE_FB_INPUT_L + NUM_TAPS,
		NUM_INPUTS = TAP_PRE_FB_INPUT_R + NUM_TAPS
	};

	enum OutputIds {
        TAP_OUTPUT_L,
        TAP_OUTPUT_R = TAP_OUTPUT_L + NUM_TAPS,
		NUM_OUTPUTS = TAP_OUTPUT_R + NUM_TAPS
	};

	enum LightIds {
		NUM_LIGHTS 
	};

	
	// Expander
	float consumerMessage[NUM_TAPS * 4] = {};// this module must read from here
	float producerMessage[NUM_TAPS * 4] = {};// mother will write into here
	
	
	PWTapBreakoutExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
		
	}

	void process(const ProcessArgs &args) override {
        
		bool motherPresent = (leftExpander.module && leftExpander.module->model == modelPortlandWeather);
		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			// To Mother			
            for (int i = 0; i < NUM_TAPS; i++) {
				producerMessage[i] = inputs[TAP_PRE_FB_INPUT_L+i].isConnected();
				producerMessage[NUM_TAPS + i] = inputs[TAP_PRE_FB_INPUT_L+i].getVoltage();
				producerMessage[(NUM_TAPS * 2) + i] = inputs[TAP_PRE_FB_INPUT_R+i].isConnected();
				producerMessage[NUM_TAPS * 3 + i] = inputs[TAP_PRE_FB_INPUT_R+i].getVoltage();
			// From Mother	
				outputs[TAP_OUTPUT_L+i].setVoltage(consumerMessage[i]);
				outputs[TAP_OUTPUT_R+i].setVoltage(consumerMessage[NUM_TAPS+i]);
			}
							
			leftExpander.messageFlipRequested = true;
		
		}		
		
	}	
};





struct PWTapBreakoutExpanderWidget : ModuleWidget {
	PWTapBreakoutExpanderWidget(PWTapBreakoutExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PWTapBreakoutExpander.svg")));

		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

 		
		for(int i=0; i<NUM_TAPS;i++) {
    		addOutput(createOutput<FWPortOutSmall>(Vec(20, 34 + i*21), module, PWTapBreakoutExpander::TAP_OUTPUT_L + i));
    		addOutput(createOutput<FWPortOutSmall>(Vec(42, 34 + i*21), module, PWTapBreakoutExpander::TAP_OUTPUT_R + i));

			addInput(createInput<FWPortInSmall>(Vec(73, 34 + i*21), module, PWTapBreakoutExpander::TAP_PRE_FB_INPUT_L + i));
    		addInput(createInput<FWPortInSmall>(Vec(95, 34 + i*21), module, PWTapBreakoutExpander::TAP_PRE_FB_INPUT_R + i));
        }


    
		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		


	}
};

Model *modelPWTapBreakoutExpander = createModel<PWTapBreakoutExpander, PWTapBreakoutExpanderWidget>("PWTapBreakoutExpander");
 