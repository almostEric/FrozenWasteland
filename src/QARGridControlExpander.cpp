#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/display.hpp"

#define NBR_SCENES 8
#define TRACK_COUNT 4
#define MAX_STEPS 18
#define PASSTHROUGH_LEFT_VARIABLE_COUNT 13
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 9
#define STEP_LEVEL_PARAM_COUNT 6
#define TRACK_LEVEL_PARAM_COUNT TRACK_COUNT * 17
#define QAR_GRID_VALUES MAX_STEPS
#define PASSTHROUGH_OFFSET MAX_STEPS * TRACK_COUNT * STEP_LEVEL_PARAM_COUNT + TRACK_LEVEL_PARAM_COUNT + QAR_GRID_VALUES


struct QARGridControlExpander : Module {

    enum ParamIds {
        PIN_Y_AXIS_MODE_PARAM,
        Y_AXIS_PIN_POS_PARAM,
        Y_AXIS_ROTATION_PARAM,
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
        GRID_CONNECTED_LIGHT = PIN_Y_AXIS_MODE_LIGHT + 3,
		NUM_LIGHTS = GRID_CONNECTED_LIGHT + 3
	};

	
	// Expander
	float leftMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	float rightMessages[2][PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};
	
    bool trackDirty[TRACK_COUNT] = {0};

    float sceneData[NBR_SCENES][21] = {{0}};
	int sceneChangeMessage = 0;


	float gridValues[MAX_STEPS] = {};
    bool bidirectional = false;

    OneDimensionalCells *gridCells;

    dsp::SchmittTrigger pinYAxisModeTrigger;
    uint8_t pinYAxisMode = 0;

    //percentages
    float rotationPercentage = 0;
    float pinPosPercentage = 0;

    bool QARExpanderDisconnectReset = true;
	
	
	QARGridControlExpander() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        configParam(Y_AXIS_PIN_POS_PARAM, 0.0f, 1.0f, 0.0f, "Grid Y Axis Pin Position","%",0,100);
        configParam(Y_AXIS_ROTATION_PARAM, -1.0f, 1.0f, 0.0f, "Grid Y Axis Rotation","°",0,100);

        configButton(PIN_Y_AXIS_MODE_PARAM,"Pin Y Axis");

		configInput(GRID_X_CV_INPUT, "Grid X Shift");
		configInput(GRID_Y_CV_INPUT, "Grid Y Shift");
		configInput(GRID_Y_AXIS_PIN_POS_CV_INPUT, "Grid X Axis Pin Position");
		configInput(GRID_Y_AXIS_ROTATION_CV_INPUT, "Grid Y Axis Rotation");

		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

        gridCells = new OneDimensionalCellsWithRollover(128, MAX_STEPS, 0, 1, PIN_ROLLOVER_MODE, WRAP_AROUND_ROLLOVER_MODE,1.0);
	}


    void dataFromJson(json_t *root) override {
        json_t *pxmJ = json_object_get(root, "pinYAxisMode");
        if (json_is_integer(pxmJ)) {
            pinYAxisMode = (uint8_t) json_integer_value(pxmJ);
        }
        
        for(int i=0;i<MAX_STEPS;i++) {
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
    
    for(int i=0;i<MAX_STEPS;i++) {
        std::string buf = "gridValue-" + std::to_string(i) ;
        json_object_set(root, buf.c_str(),json_real((float) gridCells->cells[i]));
    }

    return root;
    }


	void saveScene(int scene) {
		sceneData[scene][0] = pinYAxisMode;
		sceneData[scene][1] = params[Y_AXIS_PIN_POS_PARAM].getValue();
		sceneData[scene][2] = params[Y_AXIS_ROTATION_PARAM].getValue();
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			sceneData[scene][stepNumber+3] = gridCells->cells[stepNumber];
		}
	}

	void loadScene(int scene) {
		pinYAxisMode = sceneData[scene][0];
		params[Y_AXIS_PIN_POS_PARAM].setValue(sceneData[scene][1]);
		params[Y_AXIS_ROTATION_PARAM].setValue(sceneData[scene][2]);
		for(int stepNumber=0;stepNumber<MAX_STEPS;stepNumber++) {
			 gridCells->cells[stepNumber] = sceneData[scene][stepNumber+3];
		}
	}




    void onReset() override {
        
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


        
		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelQuadAlgorithmicRhythm || leftExpander.module->model == modelQARWellFormedRhythmExpander || 
								leftExpander.module->model == modelQARProbabilityExpander || leftExpander.module->model == modelQARGrooveExpander || 
								leftExpander.module->model == modelQARWarpedSpaceExpander || leftExpander.module->model == modelQARIrrationalityExpander || 
								leftExpander.module->model == modelQARConditionalExpander || leftExpander.module->model == modelQARGridControlExpander ||
								leftExpander.module->model == modelQARGridControlExpander));

		//lights[CONNECTED_LIGHT].value = motherPresent;
		if (motherPresent) {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;

			sceneChangeMessage = messagesFromMother[PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT+ 0];
			if(sceneChangeMessage >= 20) {
				saveScene(sceneChangeMessage-20);
			} else if (sceneChangeMessage >=10) {
				loadScene(sceneChangeMessage-10);
			}

			//Initalize
			std::fill(messagesToMother, messagesToMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT, 0.0);

			//If another expander is present, get its values (we can overwrite them)
			bool anotherExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelQARWellFormedRhythmExpander || 
											rightExpander.module->model == modelQARGrooveExpander || 
											rightExpander.module->model == modelQARProbabilityExpander || rightExpander.module->model == modelQARIrrationalityExpander || 
											rightExpander.module->model == modelQARWarpedSpaceExpander || 
											rightExpander.module->model == modelQARConditionalExpander || 
											rightExpander.module->model == modelQuadAlgorithmicRhythm ||
											rightExpander.module->model == modelQARGridControlExpander));
			if(anotherExpanderPresent)
			{			
				float *messagesFromExpander = (float*)rightExpander.consumerMessage;
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);

                if(rightExpander.module->model != modelQuadAlgorithmicRhythm) { // Get QRE values							
					std::copy(messagesFromExpander,messagesFromExpander + PASSTHROUGH_OFFSET,messagesToMother);		
				}
				for(int i=0;i<TRACK_COUNT;i++) {
					trackDirty[i] = messagesFromExpander[i] || (!QARExpanderDisconnectReset);
				}

				QARExpanderDisconnectReset = true;

				//QAR Pass through left
				std::copy(messagesFromExpander+PASSTHROUGH_OFFSET,messagesFromExpander+PASSTHROUGH_OFFSET+PASSTHROUGH_LEFT_VARIABLE_COUNT,messagesToMother+PASSTHROUGH_OFFSET);		

				//QAR Pass through right
				std::copy(messagesFromMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT,messagesFromMother+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT + PASSTHROUGH_RIGHT_VARIABLE_COUNT,
									messageToExpander+PASSTHROUGH_OFFSET + PASSTHROUGH_LEFT_VARIABLE_COUNT);			

				rightExpander.module->leftExpander.messageFlipRequested = true;
			} else {
				std::fill(trackDirty,trackDirty+TRACK_COUNT,0);
				QARExpanderDisconnectReset = false;
			}

            bidirectional = leftExpander.module->model == modelQARGrooveExpander;
            // fprintf(stderr, "bid: %hu \n", bidirectional);	

            // To Master		
            // fprintf(stderr, "from Grid: %hu \n", PASSTHROUGH_OFFSET - MAX_STEPS);	
            for (int step = 0; step < MAX_STEPS; step++) {
                messagesToMother[PASSTHROUGH_OFFSET - MAX_STEPS + step] = gridCells->valueForPosition(step);
			}


                //fprintf(stderr, "%hu \n", gridControlExpanderCount);

			leftExpander.module->rightExpander.messageFlipRequested = true;
		}		

	}	
};





struct QARGridControlExpanderWidget : ModuleWidget {
    CellBarGrid *gridDisplay;

	QARGridControlExpanderWidget(QARGridControlExpander *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QARGridControlExpander.svg")));

		
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

            gridDisplay->box.pos = Vec(11, 26);
            gridDisplay->box.size = Vec(128, 144);
            addChild(gridDisplay);

            addInput(createInput<FWPortInSmall>(Vec(53, 210), module, QARGridControlExpander::GRID_X_CV_INPUT));
            addInput(createInput<FWPortInSmall>(Vec(78, 210), module, QARGridControlExpander::GRID_Y_CV_INPUT));

//Was 69
            ParamWidget* yAxisRotationParam = createParam<RoundSmallFWKnob>(Vec(50, 245), module, QARGridControlExpander::Y_AXIS_ROTATION_PARAM);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(yAxisRotationParam)->percentage = &module->rotationPercentage;
                dynamic_cast<RoundSmallFWKnob*>(yAxisRotationParam)->biDirectional = true;
            }
            addParam(yAxisRotationParam);							
            addInput(createInput<FWPortInSmall>(Vec(78, 248), module, QARGridControlExpander::GRID_Y_AXIS_ROTATION_CV_INPUT));

            addParam(createParam<LEDButton>(Vec(53,286), module, QARGridControlExpander::PIN_Y_AXIS_MODE_PARAM));
            addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(54.5, 287.5), module, QARGridControlExpander::PIN_Y_AXIS_MODE_LIGHT));

            ParamWidget* xAxisPinPosParam = createParam<RoundSmallFWKnob>(Vec(50, 306), module, QARGridControlExpander::Y_AXIS_PIN_POS_PARAM);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(xAxisPinPosParam)->percentage = &module->pinPosPercentage;
            }
            addParam(xAxisPinPosParam);							
            addInput(createInput<FWPortInSmall>(Vec(78, 309), module, QARGridControlExpander::GRID_Y_AXIS_PIN_POS_CV_INPUT));


        }
    
		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));
	}

    void step() override {
        if (module) {
            uint8_t bidirectional = ((QARGridControlExpander*)module)->bidirectional;
            if(bidirectional) {
                gridDisplay->yAxis = 64;
            } else {
                gridDisplay->yAxis = 0;
            }
        }
        Widget::step();
    }

};

Model *modelQARGridControlExpander = createModel<QARGridControlExpander, QARGridControlExpanderWidget>("QARGridControlExpander");
 