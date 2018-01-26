#include <string.h>
#include "FrozenWasteland.hpp"
#include "dsp/digital.hpp"

struct QuadEuclideanRhythm : Module {
	enum ParamIds {
		STEPS_1_PARAM,
		DIVISIONS_1_PARAM,
		OFFSET_1_PARAM,
		STEPS_2_PARAM,
		DIVISIONS_2_PARAM,
		OFFSET_2_PARAM,
		STEPS_3_PARAM,
		DIVISIONS_3_PARAM,
		OFFSET_3_PARAM,
		STEPS_4_PARAM,
		DIVISIONS_4_PARAM,
		OFFSET_4_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		STEPS_1_INPUT,
		DIVISIONS_1_INPUT,
		OFFSET_1_INPUT,
		STEPS_2_INPUT,
		DIVISIONS_2_INPUT,
		OFFSET_2_INPUT,
		STEPS_3_INPUT,
		DIVISIONS_3_INPUT,
		OFFSET_3_INPUT,
		STEPS_4_INPUT,
		DIVISIONS_4_INPUT,
		OFFSET_4_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		NUM_OUTPUTS
	};

	bool beatMatrix[4][16];
	int beatIndex[4];
	int stepsCount[4];

	int levelArray[16];

	SchmittTrigger clockTrigger,resetTrigger;


	QuadEuclideanRhythm() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void QuadEuclideanRhythm::step() {

	for(int trackNumber=0;trackNumber<4;trackNumber++) {
		//clear out the matrix and levels
		for(int j=0;j<16;j++)
		{
			beatMatrix[trackNumber][j] = false; 
			levelArray[j] = 0;			
		}

		float stepCountf = params[trackNumber * 3].value;
		if(inputs[trackNumber * 3].active) {
			stepCountf += inputs[trackNumber * 3].value;
		}
		float divisionf = params[(trackNumber * 3) + 1].value;
		if(inputs[(trackNumber * 3) + 1].active) {
			divisionf += inputs[(trackNumber * 3) + 1].value;
		}
		float offsetf = params[(trackNumber * 3) + 2].value;
		if(inputs[(trackNumber * 3) + 2].active) {
			offsetf += inputs[(trackNumber * 3) + 2].value;
		}

		stepsCount[trackNumber] = int(clampf(stepCountf,1.0,16.0));
		int division = int(clampf(divisionf,1.0,16.0));
		int offset = int(clampf(offsetf,0.0,15.0));		

		int level = 0;
		int restsLeft = stepsCount[trackNumber]-division;
		do {
			levelArray[level] = std::min(restsLeft,division);
			restsLeft = restsLeft - division;
			level += 1;
		} while (restsLeft > 0);

		int tempIndex =0;
		for (int j = 0; j < division; j++) {
            beatMatrix[trackNumber][(tempIndex + offset) % stepsCount[trackNumber]] = true;
            tempIndex++;
            for (int k = 0; k < 16; k++) {
                if (levelArray[k] > j) {
                    tempIndex++;
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
		}
	}

	//Advance beat on trigger
	if(inputs[CLOCK_INPUT].active) {
		if(clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			for(int trackNumber=0;trackNumber < 4;trackNumber++)
			{
				beatIndex[trackNumber]++;
				if(beatIndex[trackNumber] >= stepsCount[trackNumber]) {
					beatIndex[trackNumber] = 0;
				}
			}
		}
	}


	// Set output to current state
	for(int trackNumber=0;trackNumber<4;trackNumber++) {
		if(beatMatrix[trackNumber][beatIndex[trackNumber]] == true) {
			outputs[trackNumber].value = inputs[CLOCK_INPUT].value;	
		} else {
			outputs[trackNumber].value = 0.0;	
		}				
	}
}

struct BeatDisplay : TransparentWidget {
	QuadEuclideanRhythm *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	BeatDisplay() {
		font = Font::load(assetPlugin(plugin, "res/fonts/Sudo.ttf"));
	}

	void drawBox(NVGcontext *vg, float stepNumber, float trackNumber,bool isBeat,bool isCurrent) {

		nvgSave(vg);
		Rect b = Rect(Vec(0, 0), box.size);
		nvgScissor(vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(vg);
		
		float boxX = stepNumber * 22.5;
		float boxY = trackNumber * 28.0;

		NVGcolor strokeColor = nvgRGBA(0x9f, 0xe4, 0x36, 0xff);
		NVGcolor fillColor = nvgRGBA(0xff,0xc0,0,0xff);
		if(isCurrent)
		{
			strokeColor = nvgRGBA(0x3f, 0xf4, 0x36, 0xff);
			fillColor = nvgRGBA(0x2f,0xf0,0,0xff);			
		}

		nvgLineCap(vg, NVG_ROUND);
		nvgMiterLimit(vg, 2.0);
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
		nvgStrokeColor(vg, strokeColor);
		nvgStrokeWidth(vg, 3.0);
		nvgRect(vg,boxX,boxY,21,28.0);
		if(isBeat) {
			nvgFillColor(vg, fillColor);
			nvgFill(vg);
		}
		nvgStroke(vg);
		nvgResetScissor(vg);
		nvgRestore(vg);
	}

	

	

	void draw(NVGcontext *vg) override {

		for(int trackNumber = 0;trackNumber < 4;trackNumber++) {
			for(int stepNumber = 0;stepNumber < module->stepsCount[trackNumber];stepNumber++) {
				bool isBeat = module->beatMatrix[trackNumber][stepNumber];
				bool isCurrent = module->beatIndex[trackNumber] == stepNumber;				
				drawBox(vg, float(stepNumber), float(trackNumber),isBeat,isCurrent);
			}

		}
	}
};


QuadEuclideanRhythmWidget::QuadEuclideanRhythmWidget() {
	QuadEuclideanRhythm *module = new QuadEuclideanRhythm();
	setModule(module);
	box.size = Vec(15*24, RACK_GRID_HEIGHT);

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
		display->box.pos = Vec(0, 47);
		display->box.size = Vec(box.size.x, 140);
		addChild(display);
	}


	addParam(createParam<Davies1900hBlackKnob>(Vec(27, 186), module, QuadEuclideanRhythm::STEPS_1_PARAM, 1.0, 16.0, 16.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(77, 186), module, QuadEuclideanRhythm::DIVISIONS_1_PARAM, 1.0, 16.0, 2.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(128, 186), module, QuadEuclideanRhythm::OFFSET_1_PARAM, 0.0, 15.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(27, 265), module, QuadEuclideanRhythm::STEPS_2_PARAM, 1.0, 16.0, 16.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(77, 265), module, QuadEuclideanRhythm::DIVISIONS_2_PARAM, 1.0, 16.0, 2.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(128, 265), module, QuadEuclideanRhythm::OFFSET_2_PARAM, 0.0, 15.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(200, 186), module, QuadEuclideanRhythm::STEPS_3_PARAM, 1.0, 16.0, 16.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(250, 186), module, QuadEuclideanRhythm::DIVISIONS_3_PARAM, 1.0, 16.0, 2.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(302, 186), module, QuadEuclideanRhythm::OFFSET_3_PARAM, 0.0, 15.0, 0.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(200, 265), module, QuadEuclideanRhythm::STEPS_4_PARAM, 1.0, 16.0, 16.5));
	addParam(createParam<Davies1900hBlackKnob>(Vec(250, 265), module, QuadEuclideanRhythm::DIVISIONS_4_PARAM, 1.0, 16.0, 2.0));
	addParam(createParam<Davies1900hBlackKnob>(Vec(302, 265), module, QuadEuclideanRhythm::OFFSET_4_PARAM, 0.0, 15.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(34, 223), module, QuadEuclideanRhythm::STEPS_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(83, 223), module, QuadEuclideanRhythm::DIVISIONS_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(134, 223), module, QuadEuclideanRhythm::OFFSET_1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(34, 302), module, QuadEuclideanRhythm::STEPS_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(83, 302), module, QuadEuclideanRhythm::DIVISIONS_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(134, 302), module, QuadEuclideanRhythm::OFFSET_2_INPUT));
	addInput(createInput<PJ301MPort>(Vec(205, 223), module, QuadEuclideanRhythm::STEPS_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(255, 223), module, QuadEuclideanRhythm::DIVISIONS_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(306, 223), module, QuadEuclideanRhythm::OFFSET_3_INPUT));
	addInput(createInput<PJ301MPort>(Vec(205, 302), module, QuadEuclideanRhythm::STEPS_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(255, 302), module, QuadEuclideanRhythm::DIVISIONS_4_INPUT));
	addInput(createInput<PJ301MPort>(Vec(306, 302), module, QuadEuclideanRhythm::OFFSET_4_INPUT));

	addInput(createInput<PJ301MPort>(Vec(12, 329), module, QuadEuclideanRhythm::CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(74, 329), module, QuadEuclideanRhythm::RESET_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(215, 329), module, QuadEuclideanRhythm::OUTPUT_1));
	addOutput(createOutput<PJ301MPort>(Vec(250, 329), module, QuadEuclideanRhythm::OUTPUT_2));
	addOutput(createOutput<PJ301MPort>(Vec(285, 329), module, QuadEuclideanRhythm::OUTPUT_3));
	addOutput(createOutput<PJ301MPort>(Vec(320, 329), module, QuadEuclideanRhythm::OUTPUT_4));
	
}
