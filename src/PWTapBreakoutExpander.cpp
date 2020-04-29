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
	float leftMessages[2][NUM_TAPS * 7 + 4] = {};// this module must read from here
	float rightMessages[2][NUM_TAPS * 7 + 4] = {};// this module must read from here
	
	float returnLConnected[NUM_TAPS] = {};
	float returnL[NUM_TAPS] = {};
	float returnRConnected[NUM_TAPS] = {};
	float returnR[NUM_TAPS] = {};
	
	PWTapBreakoutExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

	}

	void process(const ProcessArgs &args) override {
        
		bool motherPresent = (leftExpander.module && leftExpander.module->model == modelPortlandWeather);

		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

            // To Master			
            messagesToMother[NUM_TAPS * 7 + 2] = true; // Tap Breakout Expadner present 
            for (int tap = 0; tap < NUM_TAPS; tap++) {
				returnLConnected[tap] = inputs[TAP_PRE_FB_INPUT_L+tap].isConnected();
				returnL[tap] = inputs[TAP_PRE_FB_INPUT_L+tap].getVoltage();;
				returnRConnected[tap] = inputs[TAP_PRE_FB_INPUT_R+tap].isConnected();
				returnR[tap] = inputs[TAP_PRE_FB_INPUT_R+tap].getVoltage();
			// From Mother	
				outputs[TAP_OUTPUT_L+tap].setVoltage(messagesFromMother[tap]);
				outputs[TAP_OUTPUT_R+tap].setVoltage(messagesFromMother[NUM_TAPS+tap]);
			}

			memcpy(&messagesToMother[NUM_TAPS * 2], &returnLConnected, sizeof(float) * NUM_TAPS);
			memcpy(&messagesToMother[NUM_TAPS * 3], &returnL, sizeof(float) * NUM_TAPS);
			memcpy(&messagesToMother[NUM_TAPS * 4], &returnRConnected, sizeof(float) * NUM_TAPS);
			memcpy(&messagesToMother[NUM_TAPS * 5], &returnR, sizeof(float) * NUM_TAPS);



			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPWAlgorithmicExpander));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

				//QAR Pass through left
				messagesToMother[NUM_TAPS * 7 + 3] = messagesFromExpander[NUM_TAPS * 7 + 3]; // Algorithm presebt
				for(int i = 0; i < NUM_TAPS; i++) {
					messagesToMother[NUM_TAPS * 6 + i] = messagesFromExpander[NUM_TAPS * 6 + i]; // Delay Times
				}

				//QAR Pass through right
				messageToExpander[NUM_TAPS * 7 + 0] = messagesFromMother[NUM_TAPS * 7 + 0]; //Clock
				messageToExpander[NUM_TAPS * 7 + 1] = messagesFromMother[NUM_TAPS * 7 + 1]; //Division
				
				rightExpander.module->leftExpander.messageFlipRequested = true;
			} 

			leftExpander.module->rightExpander.messageFlipRequested = true;
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
 