#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/buttons.hpp"

#define NBR_SCENES 64
#define NBR_INPUTS 4
#define NBR_OUTPUTS 16
#define MAX_STEPS 16
#define NBR_REPEAT_MODES 4

struct FillingStation : Module {

    enum ParamIds {
        SCENE_PARAM,
        RESET_PARAM,
        REPEAT_MODE_SELECT_PARAM,
		NUM_PARAMS = REPEAT_MODE_SELECT_PARAM + NBR_REPEAT_MODES
	};

	enum InputIds {
        SCENE_INPUT,
        TRACK_INPUT,
        RESET_INPUT = TRACK_INPUT + NBR_INPUTS,
        SHIFT_UP_INPUT,
        SHIFT_DOWN_INPUT,
        FLIP_TRACKS_INPUT,
        FLIP_STEPS_INPUT,
        NUM_INPUTS
	};

	enum OutputIds {
		TRACK_OUTPUT,
        EOC_OUTPUT = TRACK_OUTPUT + NBR_OUTPUTS,		
		NUM_OUTPUTS
	};

	enum LightIds {
        REPEAT_MODE_LIGHT,
		NUM_LIGHTS = REPEAT_MODE_LIGHT + NBR_REPEAT_MODES
	};

    enum RepeatModes {
		REPEAT_MODE_NONE,
		REPEAT_MODE_HIGHEST_STEP,
		REPEAT_MODE_LAST_STEP,
        REPEAT_MODE_INDEPENDENT
	};


    int repeatMode = 1;

    int currentSceneNbr = 0;
    int currentTrackNbr = 0;
    int currentStepNbr = 0;

    int currentStepCount = 0;

	uint8_t trackMatrix[NBR_SCENES][NBR_INPUTS][MAX_STEPS] = {{{0}}};
	int8_t trackIndex[NBR_INPUTS] = {-1};
	int8_t trackLength[NBR_INPUTS] = {0};

    dsp::SchmittTrigger clockTrigger[NBR_INPUTS],resetTrigger,repeatModeTrigger[NBR_REPEAT_MODES],shiftUpTrigger,shiftDownTrigger,flipTracksTrigger,flipStepsTrigger;
    dsp::PulseGenerator beatPulse[NBR_OUTPUTS],eocPulse;
	
	FillingStation() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(SCENE_PARAM, 0.0, NBR_SCENES-1, 0.0,"Scene #");
		
        currentStepCount = 0;

        onReset();
	}

    json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "currentSceneNbr", json_integer(currentSceneNbr));
		json_object_set_new(rootJ, "repeatMode", json_integer(repeatMode));


        for(int sc=0;sc<NBR_SCENES;sc++) {
            std::string sceneDetail = "";
            for(int t=0;t<NBR_INPUTS;t++) {
                for(int st=0;st<MAX_STEPS;st++) {
                    int value = trackMatrix[sc][t][st];
                    if(value > 0) {
                        sceneDetail += char(value+64);
                    } else {
                        sceneDetail += " ";
                    }
                }
            }
            std::string buf = "sceneData-" + std::to_string(sc);
            json_object_set_new(rootJ, buf.c_str(), json_string(sceneDetail.c_str()));

        }
		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {
		json_t *csnJ = json_object_get(rootJ, "currentSceneNbr");
		if (csnJ) {
			currentSceneNbr = json_integer_value(csnJ);			
		}	

		json_t *rmJ = json_object_get(rootJ, "repeatMode");
		if (rmJ)
			repeatMode = json_integer_value(rmJ);

        currentStepCount = 0;
        for(int sc=0;sc<NBR_SCENES;sc++) {
            std::string buf = "sceneData-" + std::to_string(sc);
            json_t *sdJ = json_object_get(rootJ, buf.c_str());
            if (sdJ) {
                std::string sceneDetail = json_string_value(sdJ);
                for(int t=0;t<NBR_INPUTS;t++) {
                    int st;
                    for(st=0;st<MAX_STEPS;st++) {
                        int pos = t*MAX_STEPS + st;
                        if(sceneDetail[pos] == ' ') {
                            trackMatrix[sc][t][st] = 0;
                            break;
                        } else {
                            trackMatrix[sc][t][st] = ((int)sceneDetail[pos])-64;
                        }
                    }
                    if(sc == currentSceneNbr) { 
                        trackLength[t] = st;
                        if(st > currentStepCount) {
                            currentStepCount = st;
                        }                                
                    }
                }
            }
        }
	}

    void onReset() override {

        repeatMode = 1;
		for(int i = 0; i < NBR_INPUTS; i++) {
			trackIndex[i] = -1;
		}
        //Didn't feel like messing with multi-dimensional std::fill
        for(int sc=0;sc<NBR_SCENES;sc++) {
            for(int t=0;t<NBR_INPUTS;t++) {
                for(int st=0;st<MAX_STEPS;st++) {
                    trackMatrix[sc][t][st] = 0;
                }
            }
        }
    }

	void process(const ProcessArgs &args) override {

        float sceneInput;
        if(inputs[SCENE_INPUT].isConnected()) {
		    sceneInput = clamp(inputs[SCENE_INPUT].getVoltage() * ((NBR_SCENES -1) / 10.0),0.0,NBR_SCENES-1);
        } else {
    		sceneInput = params[SCENE_PARAM].getValue();
        }
		if(sceneInput != currentSceneNbr) {
            currentSceneNbr = sceneInput;
            currentStepCount = 0;
            for(int t=0;t<NBR_INPUTS;t++) {
                trackIndex[t]=-1;
                int st;
                for(st=0;st<MAX_STEPS;st++) {
                    if(trackMatrix[currentSceneNbr][t][st] == 0) {
                        break;
                    } 
                }
                trackLength[t] = st;
                if(st > currentStepCount) {
                    currentStepCount = st;
                }                                
            }
        }

        if(shiftUpTrigger.process(inputs[SHIFT_UP_INPUT].getVoltage())) {
            for (int s = 0; s<MAX_STEPS;s++) {
                int v = trackMatrix[currentSceneNbr][0][s];
                for(int i= 0;i<NBR_INPUTS-1;i++) {
                    trackMatrix[currentSceneNbr][i][s]= trackMatrix[currentSceneNbr][i+1][s];
                }
                trackMatrix[currentSceneNbr][NBR_INPUTS-1][s] = v;
            }
            int l = trackLength[0];                
            for(int i= 0;i<NBR_INPUTS-1;i++) {
                trackLength[i] = trackLength[i+1];
            }
            trackLength[NBR_INPUTS-1] = l;
        }

        if(shiftDownTrigger.process(inputs[SHIFT_DOWN_INPUT].getVoltage())) {
            for (int s = 0; s<MAX_STEPS;s++) {
                int v = trackMatrix[currentSceneNbr][NBR_INPUTS-1][s];
                for(int i= NBR_INPUTS-1;i>0;i--) {
                    trackMatrix[currentSceneNbr][i][s]= trackMatrix[currentSceneNbr][i-1][s];
                }
                trackMatrix[currentSceneNbr][0][s] = v;
            }
            int l = trackLength[NBR_INPUTS-1];                
            for(int i= NBR_INPUTS-1;i>0;i--) {
                trackLength[i] = trackLength[i-1];
            }
            trackLength[0] = l;
        }

        if(flipTracksTrigger.process(inputs[FLIP_TRACKS_INPUT].getVoltage())) {
            for (int s = 0; s<MAX_STEPS;s++) {
                for(int i=0;i<(NBR_INPUTS/2);i++) {
                    int v = trackMatrix[currentSceneNbr][i][s];
                    trackMatrix[currentSceneNbr][i][s]= trackMatrix[currentSceneNbr][NBR_INPUTS-i-1][s];
                    trackMatrix[currentSceneNbr][NBR_INPUTS-i-1][s] = v;
                }
            }
            for(int i=0;i<(NBR_INPUTS/2);i++) {
                int l = trackLength[i];                
                trackLength[i] = trackLength[NBR_INPUTS-i-1];
                trackLength[NBR_INPUTS-i-1] = l;
            }
        }

        if(flipStepsTrigger.process(inputs[FLIP_STEPS_INPUT].getVoltage())) {
            for(int i=0;i<NBR_INPUTS;i++) {
                // fprintf(stderr, "I got clicked! %i %i\n",i,trackLength[i]);
                if(trackLength[i] > 0) {
                    for (int s = 0; s<(trackLength[i]/2);s++) {
                        int v = trackMatrix[currentSceneNbr][i][s];
                        trackMatrix[currentSceneNbr][i][s]= trackMatrix[currentSceneNbr][i][trackLength[i]-s-1];
                        trackMatrix[currentSceneNbr][i][trackLength[i]-s-1] = v;
                    }
                }
            }
        }



        for(int i=0;i<NBR_REPEAT_MODES;i++) {
            if(repeatModeTrigger[i].process(params[REPEAT_MODE_SELECT_PARAM+i].getValue())) {
                repeatMode = i;
            }
        }
        for(int i=0;i<NBR_REPEAT_MODES;i++) {
            lights[REPEAT_MODE_LIGHT+i].value = repeatMode == i;
        }

		float resetInput = inputs[RESET_INPUT].getVoltage();
		resetInput += params[RESET_PARAM].getValue(); //RESET BUTTON ALWAYS WORKS		
		if(resetTrigger.process(resetInput)) {
            for(int i=0;i<NBR_INPUTS;i++) {
                trackIndex[i]=-1;
            }
        }

        bool atEnd = false;
        bool trackEnd = false;
        for(int i=0;i<NBR_INPUTS;i++) {
                // fprintf(stderr, "I got clicked! %i %i %i\n",i,trackIndex[i],trackLength[i]);
            if(clockTrigger[i].process(inputs[TRACK_INPUT + i].getVoltage())) {
                if(trackIndex[i] < trackLength[i]) {
                    trackIndex[i]++;
                }
                if(trackIndex[i] < trackLength[i] && trackMatrix[currentSceneNbr][i][trackIndex[i]] > 0) {
                    beatPulse[trackMatrix[currentSceneNbr][i][trackIndex[i]] - 1].trigger(1e-3);
                }
                if(repeatMode == REPEAT_MODE_INDEPENDENT && trackIndex[i] >= trackLength[i] - 1) {
                    trackIndex[i] = -1;
                    trackEnd = true;
                } 
                if(repeatMode == REPEAT_MODE_LAST_STEP) {
                    bool stepsLeft = false;
                    for(int t =0;t<NBR_INPUTS;t++) {
                        if(trackIndex[t] < trackLength[t] - 1 ) {
                            stepsLeft = true;
                            break;
                        }
                    }
                    atEnd = !stepsLeft;
                }
                if(repeatMode == REPEAT_MODE_HIGHEST_STEP && trackIndex[i] >= currentStepCount -1) {
                    atEnd = true;
                }
            }
        }
        for(int o=0;o<NBR_OUTPUTS;o++) {
            outputs[TRACK_OUTPUT + o].setVoltage(beatPulse[o].process(args.sampleTime) ? 10.0 : 0);			
        }
        if(atEnd || trackEnd) {
            eocPulse.trigger(1e-3);
        }
        outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.0 : 0);			        
        if(atEnd) {
            for(int i=0;i<NBR_INPUTS;i++) {
                trackIndex[i] = -1;
            }
        }
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

};


struct FillingStationDisplay : FramebufferWidget {
	FillingStation *module;
	std::shared_ptr<Font> digitalFont;
	std::shared_ptr<Font> textFont;
    std::string digitalFontPath;
	std::string textFontPath;

    float initX = 0;
    float initY = 0;
    float dragX = 0;
    float dragY = 0;
    int sceneNbr = 0;
    int editTrack = 0;
    int editStep = 0;
    int initialValue = 0;
    int currentValue = 0;
    bool dragging = false;


	FillingStationDisplay() {
        digitalFontPath = asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf");
        textFontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
	}



    void onButton(const event::Button &e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            e.consume(this);
            initX = e.pos.x;
            initY = e.pos.y;
            dragging = false;
            editTrack = initY / 25;
            editStep = initX / 20;
            sceneNbr = module->currentSceneNbr;
            //fprintf(stderr, "I got clicked! \n");
        }
    }

     void onDragStart(const event::DragStart &e) override {
        dragX = APP->scene->rack->getMousePos().x;
        dragY = APP->scene->rack->getMousePos().y;
        if(editTrack >= 0 && editTrack < NBR_INPUTS && editStep >=0 && editStep < MAX_STEPS) {
            initialValue = module->trackMatrix[sceneNbr][editTrack][editStep];
            dragging = true;
        }

        // fprintf(stderr, "initX:%f dragX:%f track:%i step:%i  value:%i \n", initX, dragX, editTrack,editStep,initialValue);
    }

    void onDragMove(const event::DragMove &e) override {
        // float newDragX = APP->scene->rack->mousePos.x - 10.0; //Ignoring X for now
        float newDragY = APP->scene->rack->getMousePos().y;
        float difference = dragY - newDragY;

        if(dragging) {
            currentValue = clamp(initialValue + (difference / 20),0.0f,16.0f);
            module->trackMatrix[sceneNbr][editTrack][editStep] = currentValue;
        }
    }

    void onDragEnd(const event::DragEnd &e) override {
        int stepCount = 0;
        for(int t=0;t<NBR_INPUTS;t++) {
            int s;
            for(s=0;s<MAX_STEPS;s++) {
                if(module->trackMatrix[sceneNbr][t][s] == 0) {
                    break;
                }
            }
            module->trackLength[t] = s;
            if(s > stepCount) {
                stepCount = s;
            }
        }
        module->currentStepCount = stepCount;
        // fprintf(stderr, "New Step Count:%i \n", stepCount);
    }
	
	void drawStep(const DrawArgs &args, int trackNumber, int stepNumber, int stepValue, bool isCurrent) 
	{		

        float opacity = 0xff; // Testing using opacity for accents

		//TODO: Replace with switch statement
		//Default Euclidean Colors
		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor fillColor = nvgRGBA(0xef,0xe0,0,opacity);

		if(isCurrent) {
			strokeColor = nvgRGBA(0x2f, 0xf0, 0, 0xff);
			fillColor = nvgRGBA(0x2f,0xf0,0,opacity);			
		}

		nvgFillColor(args.vg, fillColor);

        
		// fprintf(stderr, "track:%f step:%f  theta:%f \n", trackNumber,stepNumber,baseEndDegree);


		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 0.5);
        nvgBeginPath(args.vg);
        nvgRect(args.vg,stepNumber * 20,trackNumber * 25,20,25);
        nvgStroke(args.vg);

		char text[8];
        snprintf(text, sizeof(text), "%i", stepValue);
		nvgText(args.vg, stepNumber * 20 + 10, trackNumber * 25 + 16, text, NULL);

	}

	void drawSceneNumber(const DrawArgs &args, Vec pos, int sceneNumber) {
		nvgFontSize(args.vg, 32);
		nvgFontFaceId(args.vg, textFont->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_CENTER);
		nvgTextLetterSpacing(args.vg, -1);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[32];
		snprintf(text, sizeof(text), "%i", sceneNumber );
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}



	void draw(const DrawArgs &args) override {
		digitalFont = APP->window->loadFont(digitalFontPath);
        textFont = APP->window->loadFont(textFontPath);

		if (!module)
			return;

        int sceneNbr = module->currentSceneNbr;
        drawSceneNumber(args,Vec(90, 155),sceneNbr);

        nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, textFont->handle);
		nvgTextAlign(args.vg,NVG_ALIGN_CENTER);
		
		for(int trackNumber = 0;trackNumber < NBR_INPUTS;trackNumber++) {
            for(int stepNumber = 0;stepNumber < MAX_STEPS;stepNumber++) {
                int8_t stepValue = module->trackMatrix[sceneNbr][trackNumber][stepNumber];
                if(stepValue > 0) {
                    bool isCurrent = module->trackIndex[trackNumber] + 1 == stepNumber;
                    drawStep(args,trackNumber,stepNumber, stepValue, isCurrent);
                } else {
                    break;
                }
			}
		}	
	}
};


struct FillingStationWidget : ModuleWidget {
	FillingStationWidget(FillingStation *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/FillingStation.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	   

		{
			FillingStationDisplay *display = new FillingStationDisplay();
			display->module = module;
			display->box.pos = Vec(15, 30);
			display->box.size = Vec(box.size.x-20, 150);
			addChild(display);
		}

        addParam(createParam<RoundFWSnapKnob>(Vec(20, 160), module, FillingStation::SCENE_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(56, 172), module, FillingStation::SCENE_INPUT));

		addParam(createParam<TL1105>(Vec(78, 328), module, FillingStation::RESET_PARAM));
        addInput(createInput<FWPortInSmall>(Vec(98, 328), module, FillingStation::RESET_INPUT));

		for(int i=0; i<NBR_INPUTS;i++) {
			addInput(createInput<FWPortInSmall>(Vec(34, 250 + i*26), module, FillingStation::TRACK_INPUT + i));
        }

		for(int i=0; i<NBR_OUTPUTS / 2;i++) {
    		addOutput(createOutput<FWPortOutSmall>(Vec(268, 181 + i*21), module, FillingStation::TRACK_OUTPUT + i));
    		addOutput(createOutput<FWPortOutSmall>(Vec(308, 181 + i*21), module, FillingStation::TRACK_OUTPUT + i + (NBR_OUTPUTS / 2)));
        }

		for(int i=0; i<NBR_REPEAT_MODES;i++) {
			addParam(createParam<LEDButton>(Vec(140, 250 + i *26), module, FillingStation::REPEAT_MODE_SELECT_PARAM+i));
			addChild(createLight<LargeLight<GreenLight>>(Vec(141.5, 251.5+i*26), module, FillingStation::REPEAT_MODE_LIGHT+i));
        }

        addInput(createInput<FWPortInSmall>(Vec(149, 148), module, FillingStation::SHIFT_UP_INPUT));
        addInput(createInput<FWPortInSmall>(Vec(149, 180), module, FillingStation::SHIFT_DOWN_INPUT));
        addInput(createInput<FWPortInSmall>(Vec(188, 180), module, FillingStation::FLIP_TRACKS_INPUT));
        addInput(createInput<FWPortInSmall>(Vec(209, 180), module, FillingStation::FLIP_STEPS_INPUT));


        addOutput(createOutput<FWPortOutSmall>(Vec(223, 328), module, FillingStation::EOC_OUTPUT));

	}
};

Model *modelFillingStation = createModel<FillingStation, FillingStationWidget>("FillingStation");
 