#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "filters/biquad.h"

using namespace std;

#define BANDS 16

struct MrBlueSky : Module {
	enum ParamIds {
		BG_PARAM,
		ATTACK_PARAM = BG_PARAM + BANDS,
		DECAY_PARAM,
		CARRIER_Q_PARAM,
		MOD_Q_PARAM,
		BAND_OFFSET_PARAM,
		GMOD_PARAM,
		GCARR_PARAM,
		G_PARAM,
		SHAPE_PARAM,
		ATTACK_CV_ATTENUVERTER_PARAM,
		DECAY_CV_ATTENUVERTER_PARAM,
		CARRIER_Q_CV_ATTENUVERTER_PARAM,
		MODIFER_Q_CV_ATTENUVERTER_PARAM,
		SHIFT_BAND_OFFSET_CV_ATTENUVERTER_PARAM,
		MOD_INVERT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CARRIER_IN,
		IN_MOD = CARRIER_IN + BANDS,
		IN_CARR,
		ATTACK_INPUT,
		DECAY_INPUT,
		CARRIER_Q_INPUT,
		MOD_Q_INPUT,
		SHIFT_BAND_OFFSET_LEFT_INPUT,
		SHIFT_BAND_OFFSET_RIGHT_INPUT,
		SHIFT_BAND_OFFSET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MOD_OUT,
		OUT = MOD_OUT + BANDS,
		NUM_OUTPUTS
	};
	enum LightIds {
		LEARN_LIGHT,
		MOD_INVERT_LIGHT,
		NUM_LIGHTS
	};
	Biquad* iFilter[2*BANDS];
	Biquad* cFilter[2*BANDS];
	float mem[BANDS] = {0};
	float freq[BANDS] = {125,185,270,350,430,530,630,780,950,1150,1380,1680,2070,2780,3800,6400};
	float peaks[BANDS] = {0};
	float lastCarrierQ = 0;
	float lastModQ = 0;

	bool bandModInvert = false;
	int bandOffset = 0;
	int shiftIndex = 0;
	int lastBandOffset = 0;
	dsp::SchmittTrigger shiftLeftTrigger,shiftRightTrigger,modInvertTrigger;

	//percentages
	float attackPercentage = 0;
	float decayPercentage = 0;
	float carrierQPercentage = 0;
	float modQPercentage = 0;
	float bandOffsetPercentage = 0;

	MrBlueSky() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		for (int i = 0; i < BANDS; i++) {
			configParam(BG_PARAM + i, 0, 2, 1);
		}
		configParam(ATTACK_PARAM, 0.0, 0.25, 0.0,"Attack");
		configParam(DECAY_PARAM, 0.0, 0.25, 0.0,"Decay");
		configParam(CARRIER_Q_PARAM, 1.0, 15.0, 5.0,"Carrier Q");
		configParam(MOD_Q_PARAM, 1.0, 15.0, 5.0,"Modulator Q");
		configParam(BAND_OFFSET_PARAM, -15.5, 15.5, 0.0,"Band Offset");
		configParam(GMOD_PARAM, 1, 10, 5,"Modulator Gain");
		configParam(GCARR_PARAM, 1, 10, 5,"Carrier Gain");
		configParam(G_PARAM, 1, 10, 5,"Overall Gain");

		configParam(ATTACK_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Attack CV Attentuation","%",0,100);
		configParam(DECAY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Decay CV Attentuation","%",0,100);
		configParam(CARRIER_Q_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Carrier Q CV Attentuation","%",0,100);
		configParam(MODIFER_Q_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Modulator Q CV Attentuation","%",0,100);
		configParam(SHIFT_BAND_OFFSET_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0,"Band Offset CV Attentuation","%",0,100);

		float sampleRate = APP->engine->getSampleRate();

		for(int i=0; i<2*BANDS; i++) {
			iFilter[i] = new Biquad(bq_type_bandpass, freq[i%BANDS] / sampleRate, 5, 0);
			cFilter[i] = new Biquad(bq_type_bandpass, freq[i%BANDS] / sampleRate, 5, 0);
		};
	}

	void process(const ProcessArgs &args) override;
	void onSampleRateChange() override;

	// void reset() override {
	// 	bandOffset =0;
	// }
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "bandModInvert", json_integer((int) bandModInvert));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		json_t *sumJ = json_object_get(rootJ, "bandModInvert");
		if (sumJ) {
			bandModInvert = json_integer_value(sumJ);			
		}				
	}

};

void MrBlueSky::onSampleRateChange() {
	float sampleRate = APP->engine->getSampleRate();

	for(int i=0; i<2*BANDS; i++) {
		iFilter[i]->setFc(freq[i%BANDS] / sampleRate);
		cFilter[i]->setFc(freq[i%BANDS] / sampleRate);
	};
}


void MrBlueSky::process(const ProcessArgs &args) {
	// Band Offset Processing
	bandOffset = params[BAND_OFFSET_PARAM].getValue();
	if(inputs[SHIFT_BAND_OFFSET_INPUT].isConnected()) {
		bandOffset = clamp(bandOffset + inputs[SHIFT_BAND_OFFSET_INPUT].getVoltage() * params[SHIFT_BAND_OFFSET_CV_ATTENUVERTER_PARAM].getValue(),-BANDS+0.0f,BANDS+0.0f);
	}
	bandOffsetPercentage = bandOffset / float(BANDS);
	if(bandOffset != lastBandOffset) {
		shiftIndex = 0;
		lastBandOffset = bandOffset;
	}

	if(inputs[SHIFT_BAND_OFFSET_LEFT_INPUT].isConnected()) {
		if (shiftLeftTrigger.process(inputs[SHIFT_BAND_OFFSET_LEFT_INPUT].getVoltage())) {
			shiftIndex -= 1;
			if(shiftIndex <= -BANDS) {
				shiftIndex = BANDS -1;
			}
		}
	}

	if(inputs[SHIFT_BAND_OFFSET_RIGHT_INPUT].isConnected()) {
		if (shiftRightTrigger.process(inputs[SHIFT_BAND_OFFSET_RIGHT_INPUT].getVoltage())) {
			shiftIndex += 1;
			if(shiftIndex >= BANDS) {
				shiftIndex = (-BANDS) + 1;
			}
		}
	}

	if (modInvertTrigger.process(params[MOD_INVERT_PARAM].getValue())) {
		bandModInvert = !bandModInvert;
	}
	lights[MOD_INVERT_LIGHT].value = bandModInvert;

	bandOffset +=shiftIndex;
	//Hack until I can do int clamping
	if(bandOffset <= -BANDS) {
		bandOffset += (BANDS*2) - 1;
	}
	if(bandOffset >= BANDS) {
		bandOffset -= (BANDS*2) + 1;
	}


	//So some vocoding!
	float inM = inputs[IN_MOD].getVoltage()/5;
	float inC = inputs[IN_CARR].getVoltage()/5;
	const float slewMin = 0.001;
	const float slewMax = 500.0;
	const float shapeScale = 1/10.0;
	const float qEpsilon = 0.1;
	float attack = params[ATTACK_PARAM].getValue();
	float decay = params[DECAY_PARAM].getValue();
	if(inputs[ATTACK_INPUT].isConnected()) {
		attack = clamp(attack + inputs[ATTACK_INPUT].getVoltage() * params[ATTACK_CV_ATTENUVERTER_PARAM].getValue() / 40.0f,0.0f,0.25f);
	}
	attackPercentage = attack * 4.0;
	if(inputs[DECAY_INPUT].isConnected()) {
		decay = clamp(decay + inputs[DECAY_INPUT].getVoltage() * params[DECAY_CV_ATTENUVERTER_PARAM].getValue() / 40.0f,0.0f,0.25f);
	}
	decayPercentage = decay * 4.0;
	float slewAttack = slewMax * powf(slewMin / slewMax, attack);
	float slewDecay = slewMax * powf(slewMin / slewMax, decay);
	float out = 0.0;

	//Check Mod Q
	float currentQ = params[MOD_Q_PARAM].getValue();
	if(inputs[MOD_Q_PARAM].isConnected()) {
		currentQ += inputs[MOD_Q_INPUT].getVoltage() * params[MODIFER_Q_CV_ATTENUVERTER_PARAM].getValue();
	}
	currentQ = clamp(currentQ,1.0f,15.0f);
	modQPercentage = (currentQ-1.0) / 14.0;
	if (abs(currentQ - lastModQ) >= qEpsilon ) {
		for(int i=0; i<2*BANDS; i++) {
			iFilter[i]->setQ(currentQ);
		}
		lastModQ = currentQ;
	}

	//Check Carrier Q
	currentQ = params[CARRIER_Q_PARAM].getValue();
	if(inputs[CARRIER_Q_INPUT].isConnected()) {
		currentQ += inputs[CARRIER_Q_INPUT].getVoltage() * params[CARRIER_Q_CV_ATTENUVERTER_PARAM].getValue();
	}

	currentQ = clamp(currentQ,1.0f,15.0f);
	carrierQPercentage = (currentQ-1.0) / 14.0;
	if (abs(currentQ - lastCarrierQ) >= qEpsilon ) {
		for(int i=0; i<2*BANDS; i++) {
			cFilter[i]->setQ(currentQ);
		}
		lastCarrierQ = currentQ;
	}


	float gainAdjustedModifierInput =inM*params[GMOD_PARAM].getValue();
	float maxCoeff = 0.0;
	//First process all the modifier bands
	for(int i=0; i<BANDS; i++) {
		float coeff = mem[i];
		float peak = abs(iFilter[i+BANDS]->process(iFilter[i]->process(gainAdjustedModifierInput)));
		if (peak>coeff) {
			coeff += slewAttack * shapeScale * (peak - coeff) / args.sampleRate;
			if (coeff > peak)
				coeff = peak;
		}
		else if (peak < coeff) {
			coeff -= slewDecay * shapeScale * (coeff - peak) / args.sampleRate;
			if (coeff < peak)
				coeff = peak;
		}
		peaks[i]=peak;
		mem[i]=coeff;
		if(coeff > maxCoeff) {
			maxCoeff = coeff;
		}
		outputs[MOD_OUT+i].setVoltage(coeff * 5.0);
	}

	//Then process carrier bands. Mod bands are normalled to their matched carrier band unless an insert
	for(int i=0; i<BANDS; i++) {
		float coeff;
		int actualBand = i+bandOffset;
		if (actualBand < 0) {
			actualBand +=BANDS;
		} else if (actualBand >= BANDS) {
			actualBand -=BANDS;
		}
		if(inputs[CARRIER_IN+actualBand].isConnected()) {
			coeff = inputs[CARRIER_IN+actualBand].getVoltage() / 5.0;
		} else {			
			coeff = mem[actualBand];
		}

		
		if(bandModInvert) {
			coeff = maxCoeff - coeff;
		}
		
		float bandOut = cFilter[i+BANDS]->process(cFilter[i]->process(inC*params[GCARR_PARAM].getValue())) * coeff * params[BG_PARAM+i].getValue();
		out += bandOut;
	}
	float makeupGain = bandModInvert ? 1.25 : 5.0;
	outputs[OUT].setVoltage(out * makeupGain * params[G_PARAM].getValue());

}

struct MrBlueSkyBandDisplay : TransparentWidget {
	MrBlueSky *module;
	std::shared_ptr<Font> font;

	MrBlueSkyBandDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		nvgFontSize(args.vg, 10);
		nvgFontFaceId(args.vg, font->handle);
		nvgStrokeWidth(args.vg, 2);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
		//static const int portX0[4] = {20, 63, 106, 149};
		for (int i=0; i<BANDS; i++) {
			char fVal[10];
			snprintf(fVal, sizeof(fVal), "%1i", (int)module->freq[i]);
			nvgFillColor(args.vg,nvgRGBA(255, rescale(clamp(module->peaks[i],0.0f,1.0f),0,1,255,0), rescale(clamp(module->peaks[i],0.0f,1.0f),0,1,255,0), 255));
			nvgText(args.vg, 14, 19 + 22*i, fVal, NULL);
		}
	}
};

struct BandOffsetDisplay : TransparentWidget {
	MrBlueSky *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	BandOffsetDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/01 Digit.ttf"));
	}

	void drawOffset(const DrawArgs &args, Vec pos, float bandOffset) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % 2.0f", bandOffset);
		nvgText(args.vg, pos.x + 22, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		drawOffset(args, Vec(0, box.size.y - 150), module->bandOffset);
	}
};

struct MrBlueSkyWidget : ModuleWidget {
	MrBlueSkyWidget(MrBlueSky *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MrBlueSky.svg")));
		
		{
			MrBlueSkyBandDisplay *bandDisplay = new MrBlueSkyBandDisplay();
			bandDisplay->module = module;
			bandDisplay->box.pos = Vec(14, 12);
			bandDisplay->box.size = Vec(100, 370);
			addChild(bandDisplay);
		}
		{
			BandOffsetDisplay *offsetDisplay = new BandOffsetDisplay();
			offsetDisplay->module = module;
			offsetDisplay->box.pos = Vec(214, 232);
			offsetDisplay->box.size = Vec(box.size.x, 150);
			addChild(offsetDisplay);
		}

		for (int i = 0; i < BANDS; i++) {
			addOutput(createOutput<FWPortOutSmall>(Vec(44,	 20 + 22*i), module, MrBlueSky::MOD_OUT + i));
			addInput(createInput<FWPortInSmall>(Vec(84, 20 + 22*i), module, MrBlueSky::CARRIER_IN + i));
			addParam(createParam<RoundReallySmallFWKnob>(Vec(116 , 18 + 22*i), module, MrBlueSky::BG_PARAM + i));
		}


		ParamWidget* attackParam = createParam<RoundFWKnob>(Vec(156, 50), module, MrBlueSky::ATTACK_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(attackParam)->percentage = &module->attackPercentage;
		}
		addParam(attackParam);							
		ParamWidget* decayParam = createParam<RoundFWKnob>(Vec(216, 50), module, MrBlueSky::DECAY_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(decayParam)->percentage = &module->decayPercentage;
		}
		addParam(decayParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(190, 75), module, MrBlueSky::ATTACK_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(250, 75), module, MrBlueSky::DECAY_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(190, 52), module, MrBlueSky::ATTACK_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(250, 52), module, MrBlueSky::DECAY_INPUT));

		ParamWidget* carrierQParam = createParam<RoundFWKnob>(Vec(156, 130), module, MrBlueSky::CARRIER_Q_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(carrierQParam)->percentage = &module->carrierQPercentage;
		}
		addParam(carrierQParam);							
		ParamWidget* modQParam = createParam<RoundFWKnob>(Vec(216, 130), module, MrBlueSky::MOD_Q_PARAM);
		if (module) {
			dynamic_cast<RoundFWKnob*>(modQParam)->percentage = &module->modQPercentage;
		}
		addParam(modQParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(190, 155), module, MrBlueSky::CARRIER_Q_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(250, 155), module, MrBlueSky::MODIFER_Q_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(190, 132), module, MrBlueSky::CARRIER_Q_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(250, 132), module, MrBlueSky::MOD_Q_INPUT));

		ParamWidget* bandOffsetParam = createParam<RoundFWSnapKnob>(Vec(174, 202), module, MrBlueSky::BAND_OFFSET_PARAM);
		if (module) {
			dynamic_cast<RoundFWSnapKnob*>(bandOffsetParam)->percentage = &module->bandOffsetPercentage;
			dynamic_cast<RoundFWSnapKnob*>(bandOffsetParam)->biDirectional = true;
		}
		addParam(bandOffsetParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(204, 230), module, MrBlueSky::SHIFT_BAND_OFFSET_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(148, 207), module, MrBlueSky::SHIFT_BAND_OFFSET_LEFT_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(212, 207), module, MrBlueSky::SHIFT_BAND_OFFSET_RIGHT_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(179, 234), module, MrBlueSky::SHIFT_BAND_OFFSET_INPUT));

		addParam(createParam<LEDButton>(Vec(250, 262), module, MrBlueSky::MOD_INVERT_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(251.5, 263.5), module, MrBlueSky::MOD_INVERT_LIGHT));

		addParam(createParam<RoundFWKnob>(Vec(154, 290), module, MrBlueSky::GMOD_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(204, 290	), module, MrBlueSky::GCARR_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(254, 290), module, MrBlueSky::G_PARAM));
		
		addInput(createInput<PJ301MPort>(Vec(154, 335), module, MrBlueSky::IN_MOD));
		addInput(createInput<PJ301MPort>(Vec(204, 335), module, MrBlueSky::IN_CARR));

		addOutput(createOutput<PJ301MPort>(Vec(256, 335), module, MrBlueSky::OUT));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};

Model *modelMrBlueSky = createModel<MrBlueSky, MrBlueSkyWidget>("MrBlueSky");
