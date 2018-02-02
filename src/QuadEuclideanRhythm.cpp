#include <string.h>
#include "FrozenWasteland.hpp"
#include "dsp/digital.hpp"

struct QuadEuclideanRhythm : Module {
	enum ParamIds {
		STEPS_1_PARAM,
		DIVISIONS_1_PARAM,
		OFFSET_1_PARAM,
		PAD_1_PARAM,
		ACCENTS_1_PARAM,
		ACCENT_ROTATE_1_PARAM,
		STEPS_2_PARAM,
		DIVISIONS_2_PARAM,
		OFFSET_2_PARAM,
		PAD_2_PARAM,
		ACCENTS_2_PARAM,
		ACCENT_ROTATE_2_PARAM,
		STEPS_3_PARAM,
		DIVISIONS_3_PARAM,
		OFFSET_3_PARAM,
		PAD_3_PARAM,
		ACCENTS_3_PARAM,
		ACCENT_ROTATE_3_PARAM,
		STEPS_4_PARAM,
		DIVISIONS_4_PARAM,
		OFFSET_4_PARAM,
		PAD_4_PARAM,
		ACCENTS_4_PARAM,
		ACCENT_ROTATE_4_PARAM,
		CHAIN_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		STEPS_1_INPUT,
		DIVISIONS_1_INPUT,
		OFFSET_1_INPUT,
		PAD_1_INPUT,
		ACCENTS_1_INPUT,
		ACCENT_ROTATE_1_INPUT,
		START_1_INPUT,
		STEPS_2_INPUT,
		DIVISIONS_2_INPUT,
		OFFSET_2_INPUT,
		PAD_2_INPUT,
		ACCENTS_2_INPUT,
		ACCENT_ROTATE_2_INPUT,
		START_2_INPUT,
		STEPS_3_INPUT,
		DIVISIONS_3_INPUT,
		OFFSET_3_INPUT,
		PAD_3_INPUT,
		ACCENTS_3_INPUT,
		ACCENT_ROTATE_3_INPUT,
		START_3_INPUT,
		STEPS_4_INPUT,
		DIVISIONS_4_INPUT,
		OFFSET_4_INPUT,
		PAD_4_INPUT,
		ACCENTS_4_INPUT,
		ACCENT_ROTATE_4_INPUT,
		START_4_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		ACCENT_OUTPUT_1,
		EOC_OUTPUT_1,
		OUTPUT_2,
		ACCENT_OUTPUT_2,
		EOC_OUTPUT_2,
		OUTPUT_3,
		ACCENT_OUTPUT_3,
		EOC_OUTPUT_3,
		OUTPUT_4,
		ACCENT_OUTPUT_4,
		EOC_OUTPUT_4,
		NUM_OUTPUTS
	};
	enum LightIds {
		CHAIN_MODE_NONE_LIGHT,
		CHAIN_MODE_BOSS_LIGHT,
		CHAIN_MODE_EMPLOYEE_LIGHT,
		NUM_LIGHTS
	};
	enum ChainModes {
		CHAIN_MODE_NONE,
		CHAIN_MODE_BOSS,
		CHAIN_MODE_EMPLOYEE
	};


	bool beatMatrix[4][16];
	bool accentMatrix[4][16];
	int beatIndex[4];
	int stepsCount[4];

	bool running[4];
	int chainMode;
	bool initialized = false;

	SchmittTrigger clockTrigger,resetTrigger,chainModeTrigger,startTrigger[4];
	PulseGenerator eocPulse[4];


	QuadEuclideanRhythm() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS,NUM_LIGHTS) {
		for(unsigned i = 0; i < 4; i++) {
			beatIndex[i] = 0;
			stepsCount[i] = 16;
			running[i] = true;
			for(unsigned j = 0; j < 16; j++) {
				beatMatrix[i][j] = false;
				accentMatrix[i][j] = false;
			}
		}
	}
	void step() override;


	json_t *toJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "chainMode", json_integer((int) chainMode));
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "chainMode");
		if (sumJ)
			chainMode = json_integer_value(sumJ);

	}

	void setRunningState() {
	for(int trackNumber=0;trackNumber<4;trackNumber++)
	{
		if(chainMode == CHAIN_MODE_EMPLOYEE && inputs[(trackNumber * 7) + 6].active) { //START Input needs to be active
			running[trackNumber] = false;
		}
		else {
			running[trackNumber] = true;
		}
	}
}

	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};



void QuadEuclideanRhythm::step() {

	int levelArray[16];
	int accentLevelArray[16];
	int beatLocation[16];

	//Set startup state
	if(!initialized) {
		setRunningState();
		initialized = true;
	}

	// Modes
	if (chainModeTrigger.process(params[CHAIN_MODE_PARAM].value)) {
		chainMode = (chainMode + 1) % 3;
		setRunningState();
	}
	lights[CHAIN_MODE_NONE_LIGHT].value = chainMode == CHAIN_MODE_NONE ? 1.0 : 0.0;
	lights[CHAIN_MODE_BOSS_LIGHT].value = chainMode == CHAIN_MODE_BOSS ? 1.0 : 0.0;
	lights[CHAIN_MODE_EMPLOYEE_LIGHT].value = chainMode == CHAIN_MODE_EMPLOYEE ? 1.0 : 0.0;



	for(int trackNumber=0;trackNumber<4;trackNumber++) {
		//clear out the matrix and levels
		for(int j=0;j<16;j++)
		{
			beatMatrix[trackNumber][j] = false;
			accentMatrix[trackNumber][j] = false;
			levelArray[j] = 0;
			accentLevelArray[j] = 0;
			beatLocation[j] = 0;
		}

		float stepsCountf = params[trackNumber * 6].value;
		if(inputs[trackNumber * 7].active) {
			stepsCountf += inputs[trackNumber * 7].value;
		}
		stepsCountf = clampf(stepsCountf,0,16);

		float divisionf = params[(trackNumber * 6) + 1].value;
		if(inputs[(trackNumber * 7) + 1].active) {
			divisionf += inputs[(trackNumber * 7) + 1].value;
		}
		divisionf = clampf(divisionf,0,stepsCountf);

		float offsetf = params[(trackNumber * 6) + 2].value;
		if(inputs[(trackNumber * 7) + 2].active) {
			offsetf += inputs[(trackNumber * 7) + 2].value;
		}
		offsetf = clampf(offsetf,0,15);

		float padf = params[trackNumber * 6 + 3].value;
		if(inputs[trackNumber * 7 + 3].active) {
			padf += inputs[trackNumber * 7 + 3].value;
		}
		padf = clampf(padf,0,stepsCountf - divisionf);


		//Use this to reduce range of accent params/inputs so the range of motion of knob/modulation is more useful.
		float divisionScale = 1;
		if(stepsCountf > 0) {
			divisionScale = divisionf / stepsCountf;
		}

		float accentDivisionf = params[(trackNumber * 6) + 4].value * divisionScale;
		if(inputs[(trackNumber * 7) + 4].active) {
			accentDivisionf += inputs[(trackNumber * 7) + 4].value * divisionScale;
		}
		accentDivisionf = clampf(accentDivisionf,0,divisionf);

		float accentRotationf = params[(trackNumber * 6) + 5].value * divisionScale;
		if(inputs[(trackNumber * 7) + 5].active) {
			accentRotationf += inputs[(trackNumber * 7) + 5].value * divisionScale;
		}
		if(divisionf > 0) {
			accentRotationf = clampf(accentRotationf,0,divisionf-1);
		} else {
			accentRotationf = 0;
		}



		stepsCount[trackNumber] = int(stepsCountf);
		int division = int(divisionf);
		int offset = int(offsetf);
		int pad = int(padf);
		int accentDivision = int(accentDivisionf);
		int accentRotation = int(accentRotationf);

		if(stepsCount[trackNumber] > 0) {
			//Calculate Beats
			int level = 0;
			int restsLeft = std::max(0,stepsCount[trackNumber]-division-pad); // just make sure no negatives
			do {
				levelArray[level] = std::min(restsLeft,division);
				restsLeft = restsLeft - division;
				level += 1;
			} while (level < 16 && restsLeft > 0);

			int tempIndex =pad;
			int beatIndex = 0;
			for (int j = 0; j < division; j++) {
	            beatMatrix[trackNumber][(tempIndex + offset) % stepsCount[trackNumber]] = true;
	            beatLocation[beatIndex] = (tempIndex + offset) % stepsCount[trackNumber];
	            tempIndex++;
	            beatIndex++;
	            for (int k = 0; k < 16; k++) {
	                if (levelArray[k] > j) {
	                    tempIndex++;
	                }
	            }
	        }

	        //Calculate Accents
	        level = 0;
	        restsLeft = std::max(0,division-accentDivision); // just make sure no negatives
	        do {
				accentLevelArray[level] = std::min(restsLeft,accentDivision);
				restsLeft = restsLeft - accentDivision;
				level += 1;
			} while (level < 16 && restsLeft > 0);

			tempIndex =0;
			for (int j = 0; j < accentDivision; j++) {
	            accentMatrix[trackNumber][beatLocation[(tempIndex + accentRotation) % division]] = true;
	            tempIndex++;
	            for (int k = 0; k < 16; k++) {
	                if (accentLevelArray[k] > j) {
	                    tempIndex++;
	                }
	            }
	        }
        }
	}

	if(inputs[RESET_INPUT].active) {
		if(resetTrigger.process(inputs[RESET_INPUT].value)) {
			for(int trackNumber=0;trackNumber<4;trackNumber++)
			{
				beatIndex[trackNumber] = 0;
			}
			setRunningState();
		}
	}

	//See if need to start up
	for(int trackNumber=0;trackNumber < 4;trackNumber++) {
		if(chainMode != CHAIN_MODE_NONE && inputs[(trackNumber * 7) + 6].active && !running[trackNumber]) {
			if(startTrigger[trackNumber].process(inputs[(trackNumber * 7) + 6].value)) {
				running[trackNumber] = true;
			}
		}
	}

	//Advance beat on trigger
	if(inputs[CLOCK_INPUT].active) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			for(int trackNumber=0;trackNumber < 4;trackNumber++)
			{
				if(running[trackNumber]) {
					beatIndex[trackNumber]++;
					//End of Cycle
					if(beatIndex[trackNumber] >= stepsCount[trackNumber]) {
						beatIndex[trackNumber] = 0;
						eocPulse[trackNumber].trigger(1e-3);
						//If in a chain mode, stop running until start trigger received
						if(chainMode != CHAIN_MODE_NONE && inputs[(trackNumber * 7) + 6].active) { //START Input needs to be active
							running[trackNumber] = false;
						}
					}
				}
			}
		}
	}


	// Set output to current state
	for(int trackNumber=0;trackNumber<4;trackNumber++) {
		//Send out beat
		if(beatMatrix[trackNumber][beatIndex[trackNumber]] == true && running[trackNumber]) {
			outputs[trackNumber * 3].value = inputs[CLOCK_INPUT].value;
		} else {
			outputs[trackNumber * 3].value = 0.0;
		}
		//send out accent
		if(accentMatrix[trackNumber][beatIndex[trackNumber]] == true && running[trackNumber]) {
			outputs[trackNumber * 3 + 1].value = inputs[CLOCK_INPUT].value;
		} else {
			outputs[trackNumber * 3 + 1].value = 0.0;
		}
		//Send out End of Cycle
		outputs[(trackNumber * 3) + 2].value = eocPulse[trackNumber].process(1.0 / engineGetSampleRate()) ? 10.0 : 0;
	}
}

struct BeatDisplay : TransparentWidget {
	QuadEuclideanRhythm *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	BeatDisplay() {
		font = Font::load(assetPlugin(plugin, "res/fonts/Sudo.ttf"));
	}

	void drawBox(NVGcontext *vg, float stepNumber, float trackNumber,bool isBeat,bool isAccent,bool isCurrent) {

		//nvgSave(vg);
		//Rect b = Rect(Vec(0, 0), box.size);
		//nvgScissor(vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(vg);

		float boxX = stepNumber * 22.5;
		float boxY = trackNumber * 22.5;

		float opacity = 0x80; // Testing using opacity for accents

		if(isAccent) {
			opacity = 0xff;
		}


		NVGcolor strokeColor = nvgRGBA(0xef, 0xe0, 0, 0xff);
		NVGcolor fillColor = nvgRGBA(0xef,0xe0,0,opacity);
		if(isCurrent)
		{
			strokeColor = nvgRGBA(0x2f, 0xf0, 0, 0xff);
			fillColor = nvgRGBA(0x2f,0xf0,0,opacity);
		}

		nvgStrokeColor(vg, strokeColor);
		nvgStrokeWidth(vg, 1.0);
		nvgRect(vg,boxX,boxY,21,21.0);
		if(isBeat) {
			nvgFillColor(vg, fillColor);
			nvgFill(vg);
		}
		nvgStroke(vg);
		//nvgResetScissor(vg);
		//nvgRestore(vg);
	}

	void draw(NVGcontext *vg) override {

		for(int trackNumber = 0;trackNumber < 4;trackNumber++) {
			for(int stepNumber = 0;stepNumber < module->stepsCount[trackNumber];stepNumber++) {
				bool isBeat = module->beatMatrix[trackNumber][stepNumber];
				bool isAccent = module->accentMatrix[trackNumber][stepNumber];
				bool isCurrent = module->beatIndex[trackNumber] == stepNumber && module->running[trackNumber];
				drawBox(vg, float(stepNumber), float(trackNumber),isBeat,isAccent,isCurrent);
			}

		}
	}
};


QuadEuclideanRhythmWidget::QuadEuclideanRhythmWidget() {
	QuadEuclideanRhythm *module = new QuadEuclideanRhythm();
	setModule(module);
	box.size = Vec(15*26, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/QuadEuclideanRhythm.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	{
		BeatDisplay *display = new BeatDisplay();
		display->module = module;
		display->box.pos = Vec(16, 34);
		display->box.size = Vec(box.size.x-30, 135);
		addChild(display);
	}


	addParam(createParam<RoundSmallBlackKnob>(Vec(22, 138), module, QuadEuclideanRhythm::STEPS_1_PARAM, 0.0, 16.2, 16.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(61, 138), module, QuadEuclideanRhythm::DIVISIONS_1_PARAM, 1.0, 16.2, 2.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(100, 138), module, QuadEuclideanRhythm::OFFSET_1_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(139, 138), module, QuadEuclideanRhythm::PAD_1_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(178, 138), module, QuadEuclideanRhythm::ACCENTS_1_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(217, 138), module, QuadEuclideanRhythm::ACCENT_ROTATE_1_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(22, 195), module, QuadEuclideanRhythm::STEPS_2_PARAM, 0.0, 16.0, 16.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(61, 195), module, QuadEuclideanRhythm::DIVISIONS_2_PARAM, 1.0, 16.2, 2.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(100, 195), module, QuadEuclideanRhythm::OFFSET_2_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(139, 195), module, QuadEuclideanRhythm::PAD_2_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(178, 195), module, QuadEuclideanRhythm::ACCENTS_2_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(217, 195), module, QuadEuclideanRhythm::ACCENT_ROTATE_2_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(22, 252), module, QuadEuclideanRhythm::STEPS_3_PARAM, 0.0, 16.2, 16.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(61, 252), module, QuadEuclideanRhythm::DIVISIONS_3_PARAM, 1.0, 16.2, 2.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(100, 252), module, QuadEuclideanRhythm::OFFSET_3_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(139, 252), module, QuadEuclideanRhythm::PAD_3_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(178, 252), module, QuadEuclideanRhythm::ACCENTS_3_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(217, 252), module, QuadEuclideanRhythm::ACCENT_ROTATE_3_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(22, 309), module, QuadEuclideanRhythm::STEPS_4_PARAM, 0.0, 16.2, 16.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(61, 309), module, QuadEuclideanRhythm::DIVISIONS_4_PARAM, 1.0, 16.2, 2.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(100, 309), module, QuadEuclideanRhythm::OFFSET_4_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(139, 309), module, QuadEuclideanRhythm::PAD_4_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(178, 309), module, QuadEuclideanRhythm::ACCENTS_4_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(217, 309), module, QuadEuclideanRhythm::ACCENT_ROTATE_4_PARAM, 0.0, 15.2, 0.0));
	addParam(createParam<CKD6>(Vec(275, 290), module, QuadEuclideanRhythm::CHAIN_MODE_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(24, 167), module, QuadEuclideanRhythm::STEPS_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(63, 167), module, QuadEuclideanRhythm::DIVISIONS_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(102, 167), module, QuadEuclideanRhythm::OFFSET_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(141, 167), module, QuadEuclideanRhythm::PAD_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(180, 167), module, QuadEuclideanRhythm::ACCENTS_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(219, 167), module, QuadEuclideanRhythm::ACCENT_ROTATE_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(24, 224), module, QuadEuclideanRhythm::STEPS_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(63, 224), module, QuadEuclideanRhythm::DIVISIONS_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(102, 224), module, QuadEuclideanRhythm::OFFSET_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(141, 224), module, QuadEuclideanRhythm::PAD_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(180, 224), module, QuadEuclideanRhythm::ACCENTS_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(219, 224), module, QuadEuclideanRhythm::ACCENT_ROTATE_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(24, 281), module, QuadEuclideanRhythm::STEPS_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(63, 281), module, QuadEuclideanRhythm::DIVISIONS_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(102, 281), module, QuadEuclideanRhythm::OFFSET_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(141, 281), module, QuadEuclideanRhythm::PAD_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(180, 281), module, QuadEuclideanRhythm::ACCENTS_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(219, 281), module, QuadEuclideanRhythm::ACCENT_ROTATE_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(24, 338), module, QuadEuclideanRhythm::STEPS_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(63, 338), module, QuadEuclideanRhythm::DIVISIONS_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(102, 338), module, QuadEuclideanRhythm::OFFSET_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(141, 338), module, QuadEuclideanRhythm::PAD_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(180, 338), module, QuadEuclideanRhythm::ACCENTS_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(219, 338), module, QuadEuclideanRhythm::ACCENT_ROTATE_4_INPUT));

	addInput(createInput<PJ301MPort>(Vec(267, 333), module, QuadEuclideanRhythm::CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(330, 333), module, QuadEuclideanRhythm::RESET_INPUT));

	addInput(createInput<PJ301MPort>(Vec(322, 145), module, QuadEuclideanRhythm::START_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(322, 175), module, QuadEuclideanRhythm::START_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(322, 205), module, QuadEuclideanRhythm::START_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(322, 235), module, QuadEuclideanRhythm::START_4_INPUT));


	addOutput(createOutput<PJ301MPort>(Vec(255, 145), module, QuadEuclideanRhythm::OUTPUT_1));
	addOutput(createOutput<PJ301MPort>(Vec(286, 145), module, QuadEuclideanRhythm::ACCENT_OUTPUT_1));
	addOutput(createOutput<PJ301MPort>(Vec(354, 145), module, QuadEuclideanRhythm::EOC_OUTPUT_1));
	addOutput(createOutput<PJ301MPort>(Vec(256, 175), module, QuadEuclideanRhythm::OUTPUT_2));
	addOutput(createOutput<PJ301MPort>(Vec(286, 175), module, QuadEuclideanRhythm::ACCENT_OUTPUT_2));
	addOutput(createOutput<PJ301MPort>(Vec(354, 175), module, QuadEuclideanRhythm::EOC_OUTPUT_2));
	addOutput(createOutput<PJ301MPort>(Vec(256, 205), module, QuadEuclideanRhythm::OUTPUT_3));
	addOutput(createOutput<PJ301MPort>(Vec(286, 205), module, QuadEuclideanRhythm::ACCENT_OUTPUT_3));
	addOutput(createOutput<PJ301MPort>(Vec(354, 205), module, QuadEuclideanRhythm::EOC_OUTPUT_3));
	addOutput(createOutput<PJ301MPort>(Vec(256, 235), module, QuadEuclideanRhythm::OUTPUT_4));
	addOutput(createOutput<PJ301MPort>(Vec(286, 235), module, QuadEuclideanRhythm::ACCENT_OUTPUT_4));
	addOutput(createOutput<PJ301MPort>(Vec(354, 235), module, QuadEuclideanRhythm::EOC_OUTPUT_4));

	addChild(createLight<SmallLight<BlueLight>>(Vec(310, 276), module, QuadEuclideanRhythm::CHAIN_MODE_NONE_LIGHT));
	addChild(createLight<SmallLight<GreenLight>>(Vec(310, 291), module, QuadEuclideanRhythm::CHAIN_MODE_BOSS_LIGHT));
	addChild(createLight<SmallLight<RedLight>>(Vec(310, 306), module, QuadEuclideanRhythm::CHAIN_MODE_EMPLOYEE_LIGHT));


}
