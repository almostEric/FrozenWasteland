#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/display.hpp"

#define NUM_TAPS 16


struct PWGridControlExpander : Module {

    enum ParamIds {
        PIN_Y_AXIS_MODE_PARAM,
        Y_AXIS_PIN_POS_PARAM,
        Y_AXIS_ROTATION_PARAM,
        DESTINATION_LEVEL_PARAM,
        DESTINATION_PAN_PARAM,
        DESTINATION_FC_PARAM,
        DESTINATION_Q_PARAM,
        DESTINATION_PITCH_PARAM,
        DESTINATION_DETUNE_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
        GRID_X_CV_INPUT,
        GRID_Y_CV_INPUT,
        GRID_Y_AXIS_PIN_POS_CV_INPUT,
        GRID_Y_AXIS_ROTATION_CV_INPUT,
        NUM_INPUTS
	};

	enum OutputIds {
		NUM_OUTPUTS
	};

	enum LightIds {
        PIN_Y_AXIS_MODE_LIGHT,
        DESTINATION_LEVEL_LIGHT = PIN_Y_AXIS_MODE_LIGHT + 3,
        DESTINATION_PAN_LIGHT = DESTINATION_LEVEL_LIGHT + 3,
        DESTINATION_FC_LIGHT = DESTINATION_PAN_LIGHT + 3,
        DESTINATION_Q_LIGHT = DESTINATION_FC_LIGHT + 3,
        DESTINATION_PITCH_LIGHT = DESTINATION_Q_LIGHT + 3,
        DESTINATION_DETUNE_LIGHT = DESTINATION_PITCH_LIGHT + 3,
		NUM_LIGHTS = DESTINATION_DETUNE_LIGHT + 3
	};

	
	// Expander
	float leftMessages[2][NUM_TAPS * 15] = {};// this module must read from here
	float rightMessages[2][NUM_TAPS * 15] = {};// this module must read from here
	
	float gridValues[NUM_TAPS] = {};

    OneDimensionalCells *gridCells;

    dsp::SchmittTrigger pinYAxisModeTrigger,destinationTrigger[6];
    uint8_t pinYAxisMode = 0;
    uint8_t destination = 1;

    //percentages
    float rotationPercentage = 0;
    float pinPosPercentage = 0;


	
	PWGridControlExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        configParam(Y_AXIS_PIN_POS_PARAM, 0.0f, 1.0f, 0.0f, "Grid Y Axis Pin Position","%",0,100);
        configParam(Y_AXIS_ROTATION_PARAM, -1.0f, 1.0f, 0.0f, "Grid Y Axis Rotation","°",0,100);

        configButton(PIN_Y_AXIS_MODE_PARAM,"Pin Y Axis");

        configButton(DESTINATION_LEVEL_PARAM,"Grid Controls: Level");
        configButton(DESTINATION_PAN_PARAM,"Grid Controls: Panning");
        configButton(DESTINATION_FC_PARAM,"Grid Controls: Filter Cutoff");
        configButton(DESTINATION_Q_PARAM,"Grid Controls: Filter Q");
        configButton(DESTINATION_PITCH_PARAM,"Grid Controls: Pitch Shift");
        configButton(DESTINATION_DETUNE_PARAM,"Grid Controls: Pitch Detune");

		configInput(GRID_X_CV_INPUT, "Grid X Shift");
		configInput(GRID_Y_CV_INPUT, "Grid Y Shift");
		configInput(GRID_Y_AXIS_PIN_POS_CV_INPUT, "Grid X Axis Pin Position");
		configInput(GRID_Y_AXIS_ROTATION_CV_INPUT, "Grid Y Axis Rotation");

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

        gridCells = new OneDimensionalCellsWithRollover(128, NUM_TAPS, 0, 1, PIN_ROLLOVER_MODE, WRAP_AROUND_ROLLOVER_MODE,1.0);
	}


    void dataFromJson(json_t *root) override {
        json_t *pxmJ = json_object_get(root, "pinYAxisMode");
        if (json_is_integer(pxmJ)) {
            pinYAxisMode = (uint8_t) json_integer_value(pxmJ);
        }

        json_t *dJ = json_object_get(root, "destination");
        if (json_is_integer(dJ)) {
            destination = (uint8_t) json_integer_value(dJ);
        }

        
        for(int i=0;i<NUM_TAPS;i++) {
            std::string buf = "gridValue-" + std::to_string(i) ;
            json_t *gvJ = json_object_get(root, buf.c_str());
            if (gvJ) {
                gridCells->cells[i] = json_real_value(gvJ);
            }
        }
    }

    json_t *dataToJson() override {
    json_t *root = json_object();

    json_object_set(root, "pinYAxisMode", json_integer(pinYAxisMode));
    json_object_set(root, "destination", json_integer(destination));
    
    for(int i=0;i<NUM_TAPS;i++) {
        std::string buf = "gridValue-" + std::to_string(i) ;
        json_object_set(root, buf.c_str(),json_real((float) gridCells->cells[i]));
    }

    return root;
    }

    void onReset() override {
        
        destination = 1;
        pinYAxisMode = 0;

        // reset the cells
        gridCells->reset();
    }



    float paramValue (uint16_t param, uint16_t input, float low, float high) {
        float current = params[param].getValue();

        if (inputs[input].isConnected()) {
            // high - low, divided by one tenth input voltage, plus the current value
            current += ((inputs[input].getVoltage() / 10) * (high - low));
        }

        return clamp(current, low, high);
    }


	void process(const ProcessArgs &args) override {

        float pinYAxisPos = paramValue(Y_AXIS_PIN_POS_PARAM, GRID_Y_AXIS_PIN_POS_CV_INPUT, 0, 1);
        pinPosPercentage = pinYAxisPos;
        float yAxisRotation = paramValue(Y_AXIS_ROTATION_PARAM, GRID_Y_AXIS_ROTATION_CV_INPUT, -1, 1);
        rotationPercentage = yAxisRotation;
        if (pinYAxisModeTrigger.process(params[PIN_Y_AXIS_MODE_PARAM].getValue())) {
            pinYAxisMode = (pinYAxisMode + 1) % 5;
        }
        gridCells->pinXAxisValues = pinYAxisMode;
        gridCells->pinXAxisPosition = pinYAxisPos;
        gridCells->rotateX = yAxisRotation;
        switch (pinYAxisMode) {
            case 0 :
            lights[PIN_Y_AXIS_MODE_LIGHT+0].value = 0;
            lights[PIN_Y_AXIS_MODE_LIGHT+1].value = 0;
            lights[PIN_Y_AXIS_MODE_LIGHT+2].value = 0;
            break;
            case 1 :
            lights[PIN_Y_AXIS_MODE_LIGHT].value = .15;
            lights[PIN_Y_AXIS_MODE_LIGHT+1].value = 1;
            lights[PIN_Y_AXIS_MODE_LIGHT+2].value = .15;
            break;
            case 2 :
            lights[PIN_Y_AXIS_MODE_LIGHT].value = 0;
            lights[PIN_Y_AXIS_MODE_LIGHT+1].value = 1;
            lights[PIN_Y_AXIS_MODE_LIGHT+2].value = 0;
            break;
            case 3 :
            lights[PIN_Y_AXIS_MODE_LIGHT].value = 1;
            lights[PIN_Y_AXIS_MODE_LIGHT+1].value = .15;
            lights[PIN_Y_AXIS_MODE_LIGHT+2].value = .15;
            break;
            case 4 :
            lights[PIN_Y_AXIS_MODE_LIGHT].value = 1;
            lights[PIN_Y_AXIS_MODE_LIGHT+1].value = 0;
            lights[PIN_Y_AXIS_MODE_LIGHT+2].value = 0;
            break;
        }

        float gridShiftX = inputs[GRID_X_CV_INPUT].getVoltage() / 5.0;
        float gridShiftY = inputs[GRID_Y_CV_INPUT].getVoltage() / 5.0;
        gridCells->shiftX = gridShiftX;
        gridCells->shiftY = gridShiftY;

        for(int i=0;i<6;i++) {
            if (destinationTrigger[i].process(params[DESTINATION_LEVEL_PARAM+i].getValue())) {
                destination = i;
            }
        }
        for(int i=0;i<6;i++) {
            if(i!=destination) {
                lights[DESTINATION_LEVEL_LIGHT+i*3].value = 0;    
                lights[DESTINATION_LEVEL_LIGHT+i*3+1].value = 0;    
                lights[DESTINATION_LEVEL_LIGHT+i*3+2].value = 0;    
            }
        }
        switch (destination) {
            case 0 :
            lights[DESTINATION_LEVEL_LIGHT+0].value = 0;
            lights[DESTINATION_LEVEL_LIGHT+1].value = 0;
            lights[DESTINATION_LEVEL_LIGHT+2].value = 1;

            gridCells->lowRange = 0.0f; 
            gridCells->totalRange = 1.0f;
            break;
            case 1 :
            lights[DESTINATION_PAN_LIGHT].value = .25;
            lights[DESTINATION_PAN_LIGHT+1].value = .25;
            lights[DESTINATION_PAN_LIGHT+2].value = 1;
            gridCells->lowRange = -1.0f; 
            gridCells->totalRange = 2.0f;
            break;
            case 2 :
            lights[DESTINATION_FC_LIGHT].value = 0;
            lights[DESTINATION_FC_LIGHT+1].value = 1;
            lights[DESTINATION_FC_LIGHT+2].value = 0;
            gridCells->lowRange = 0.0f; 
            gridCells->totalRange = 1.0f;
            break;
            case 3 :
            lights[DESTINATION_Q_LIGHT].value = .25;
            lights[DESTINATION_Q_LIGHT+1].value = 1;
            lights[DESTINATION_Q_LIGHT+2].value = .25;
            gridCells->lowRange = 0.0f; 
            gridCells->totalRange = 1.0f;
            break;
            case 4 :
            lights[DESTINATION_PITCH_LIGHT].value = 1;
            lights[DESTINATION_PITCH_LIGHT+1].value = 0;
            lights[DESTINATION_PITCH_LIGHT+2].value = 0;
            gridCells->lowRange = -1.0f; 
            gridCells->totalRange = 2.0f;
            break;
            case 5 :
            lights[DESTINATION_DETUNE_LIGHT].value = 1;
            lights[DESTINATION_DETUNE_LIGHT+1].value = 0.5;
            lights[DESTINATION_DETUNE_LIGHT+2].value = 0;
            gridCells->lowRange = -1.0f; 
            gridCells->totalRange = 2.0f;
            break;
        }

        
		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelPortlandWeather || leftExpander.module->model == modelPWTapBreakoutExpander || leftExpander.module->model == modelPWGridControlExpander));

		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

            uint8_t gridControlExpanderCount = 0;

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPWAlgorithmicExpander || rightExpander.module->model == modelPWTapBreakoutExpander || rightExpander.module->model == modelPWGridControlExpander));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                gridControlExpanderCount = messagesFromExpander[4];

                //fprintf(stderr, "%hu \n", gridControlExpanderCount);

				//QAR Pass through left
				messagesToMother[2] = messagesFromExpander[2]; // Tap Breakout present
				messagesToMother[3] = messagesFromExpander[3]; // Algorithm present
                memcpy(&messagesToMother[5], &messagesFromExpander[5], sizeof(float) * gridControlExpanderCount);
                memcpy(&messagesToMother[NUM_TAPS * 3], &messagesFromExpander[NUM_TAPS * 3], sizeof(float) * NUM_TAPS * 12);

				//QAR Pass through right
				messageToExpander[0] = messagesFromMother[0]; //Clock
				messageToExpander[1] = messagesFromMother[1]; //Division
                memcpy(&messageToExpander[NUM_TAPS], &messagesFromMother[NUM_TAPS], sizeof(float) * NUM_TAPS * 2);
				
				rightExpander.module->leftExpander.messageFlipRequested = true;
			} else {
                //set other stuff to 0
				messagesToMother[2] = 0; // Tap Breakout not present
				messagesToMother[3] = 0; // Algorithm not present
                memset(&messagesToMother[NUM_TAPS * 3], 0, sizeof(float) * NUM_TAPS * 12);
            }

            gridControlExpanderCount++;
            // To Master			
            messagesToMother[4] = gridControlExpanderCount; // Number of GCEs 
            messagesToMother[4+gridControlExpanderCount] = destination+1; // Parameter Destination 
            for (int tap = 0; tap < NUM_TAPS; tap++) {
				gridValues[tap] = gridCells->valueForPosition(tap);
			}

			memcpy(&messagesToMother[NUM_TAPS * (7+gridControlExpanderCount)], &gridValues, sizeof(float) * NUM_TAPS);

                //fprintf(stderr, "%hu \n", gridControlExpanderCount);

			leftExpander.module->rightExpander.messageFlipRequested = true;
		}		


		
	}	
};





struct PWGridControlExpanderWidget : ModuleWidget {
    CellBarGrid *gridDisplay;

	PWGridControlExpanderWidget(PWGridControlExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PWGridControlExpander.svg")));

		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        {
            gridDisplay = new CellBarGrid();
            if (module) {
                gridDisplay->cells = module->gridCells;
                gridDisplay->gridName = "Grid";
            }

            gridDisplay->box.pos = Vec(56, 26);
            gridDisplay->box.size = Vec(128, 128);
            addChild(gridDisplay);

            addInput(createInput<FWPortInSmall>(Vec(8, 34), module, PWGridControlExpander::GRID_X_CV_INPUT));
            addInput(createInput<FWPortInSmall>(Vec(33, 34), module, PWGridControlExpander::GRID_Y_CV_INPUT));

            ParamWidget* yAxisRotationParam = createParam<RoundSmallFWKnob>(Vec(5, 69), module, PWGridControlExpander::Y_AXIS_ROTATION_PARAM);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(yAxisRotationParam)->percentage = &module->rotationPercentage;
                dynamic_cast<RoundSmallFWKnob*>(yAxisRotationParam)->biDirectional = true;
            }
            addParam(yAxisRotationParam);							
            addInput(createInput<FWPortInSmall>(Vec(33, 72), module, PWGridControlExpander::GRID_Y_AXIS_ROTATION_CV_INPUT));

            addParam(createParam<LEDButton>(Vec(8,110), module, PWGridControlExpander::PIN_Y_AXIS_MODE_PARAM));
            addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(9.5, 111.5), module, PWGridControlExpander::PIN_Y_AXIS_MODE_LIGHT));

            ParamWidget* xAxisPinPosParam = createParam<RoundSmallFWKnob>(Vec(5, 130), module, PWGridControlExpander::Y_AXIS_PIN_POS_PARAM);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(xAxisPinPosParam)->percentage = &module->pinPosPercentage;
            }
            addParam(xAxisPinPosParam);							
            addInput(createInput<FWPortInSmall>(Vec(33, 133), module, PWGridControlExpander::GRID_Y_AXIS_PIN_POS_CV_INPUT));


        }


        addParam(createParam<LEDButton>(Vec(10,208), module, PWGridControlExpander::DESTINATION_LEVEL_PARAM));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 209.5), module, PWGridControlExpander::DESTINATION_LEVEL_LIGHT));
        addParam(createParam<LEDButton>(Vec(10,230), module, PWGridControlExpander::DESTINATION_PAN_PARAM));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 231.5), module, PWGridControlExpander::DESTINATION_PAN_LIGHT));
        addParam(createParam<LEDButton>(Vec(10,252), module, PWGridControlExpander::DESTINATION_FC_PARAM));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 253.5), module, PWGridControlExpander::DESTINATION_FC_LIGHT));
        addParam(createParam<LEDButton>(Vec(10,274), module, PWGridControlExpander::DESTINATION_Q_PARAM));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 275.5), module, PWGridControlExpander::DESTINATION_Q_LIGHT));
        addParam(createParam<LEDButton>(Vec(10,296), module, PWGridControlExpander::DESTINATION_PITCH_PARAM));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 297.5), module, PWGridControlExpander::DESTINATION_PITCH_LIGHT));
        addParam(createParam<LEDButton>(Vec(10,318), module, PWGridControlExpander::DESTINATION_DETUNE_PARAM));
        addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(11.5, 319.5), module, PWGridControlExpander::DESTINATION_DETUNE_LIGHT));

    
		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));
	}

    void step() override {
        if (module) {
            uint8_t destination = ((PWGridControlExpander*)module)->destination;
            if(destination == 1 || destination == 4 || destination == 5) {
                gridDisplay->yAxis = 64;
            } else {
                gridDisplay->yAxis = 0;
            }
        }
        Widget::step();
    }
};

Model *modelPWGridControlExpander = createModel<PWGridControlExpander, PWGridControlExpanderWidget>("PWGridControlExpander");
 