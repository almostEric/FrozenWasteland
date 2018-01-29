#include "FrozenWasteland.hpp"
#include "dsp/decimator.hpp"
#include "filters/biquad.h"

using namespace std;

#define BANDS 16

struct MrBlueSky : Module {
	enum ParamIds {
		BG_PARAM,
		ATTACK_PARAM = BG_PARAM + BANDS,
		DECAY_PARAM,
		Q_PARAM,
		GMOD_PARAM,
		GCARR_PARAM, 	
		G_PARAM,
		SHAPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CARRIER_IN,		
		IN_MOD = CARRIER_IN + BANDS,
		IN_CARR,
		NUM_INPUTS
	};
	enum OutputIds {
		MOD_OUT,
		OUT = MOD_OUT + BANDS,
		NUM_OUTPUTS
	};
	enum LightIds {
		LEARN_LIGHT,
		NUM_LIGHTS
	};
	Biquad* iFilter[2*BANDS];
	Biquad* cFilter[2*BANDS];
	float mem[BANDS] = {0};
	float freq[BANDS] = {125,185,270,350,430,530,630,780,950,1150,1380,1680,2070,2780,3800,6400};
	float peaks[BANDS] = {0};

	MrBlueSky() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for(int i=0; i<2*BANDS; i++) {
			iFilter[i] = new Biquad(bq_type_bandpass, freq[i%BANDS] / engineGetSampleRate(), 5, 6);
			cFilter[i] = new Biquad(bq_type_bandpass, freq[i%BANDS] / engineGetSampleRate(), 5, 6);
			}
	}

	void step() override;

};

void MrBlueSky::step() {
	float inM = inputs[IN_MOD].value/5;
	float inC = inputs[IN_CARR].value/5;
	const float slewMin = 0.001;
	const float slewMax = 500.0;
	const float shapeScale = 1/10.0;
	float attack = params[ATTACK_PARAM].value;
	float decay = params[DECAY_PARAM].value;
	float slewAttack = slewMax * powf(slewMin / slewMax, attack);
	float slewDecay = slewMax * powf(slewMin / slewMax, decay);
	float out = 0.0;

	//First process all the modifier bands
	for(int i=0; i<BANDS; i++) {
		float coeff = mem[i];
		float peak = abs(iFilter[i+BANDS]->process(iFilter[i]->process(inM*params[GMOD_PARAM].value)));
		if (peak>coeff) {
			coeff += slewAttack * shapeScale * (peak - coeff) / engineGetSampleRate();
			if (coeff > peak)
				coeff = peak;
		}
		else if (peak < coeff) {
			coeff -= slewDecay * shapeScale * (coeff - peak) / engineGetSampleRate();
			if (coeff < peak)
				coeff = peak;
		}
		peaks[i]=peak;
		mem[i]=coeff;
		outputs[MOD_OUT+i].value = coeff * 5.0;
	}

	//Then process carrier bands. Mod bands are normalled to their matched carrier band unless an insert
	for(int i=0; i<BANDS; i++) {
		float coeff;
		if(inputs[CARRIER_IN+i].active) {
			coeff = inputs[CARRIER_IN].value / 5.0;
		} else {
			coeff = mem[i];
		}
		
		float bandOut = cFilter[i+BANDS]->process(cFilter[i]->process(inC*params[GCARR_PARAM].value)) * coeff * params[BG_PARAM+i].value;
		out += bandOut;
	}
	outputs[OUT].value = out * 5 * params[G_PARAM].value;
}

struct MrBlueSkyDisplay : TransparentWidget {
	MrBlueSky *module;
	std::shared_ptr<Font> font;

	MrBlueSkyDisplay() {
		font = Font::load(assetPlugin(plugin, "res/fonts/Sudo.ttf"));
	}

	void draw(NVGcontext *vg) override {
		nvgFontSize(vg, 14);
		nvgFontFaceId(vg, font->handle);
		nvgStrokeWidth(vg, 2);
		nvgTextLetterSpacing(vg, -2);
		nvgTextAlign(vg, NVG_ALIGN_CENTER);
		//static const int portX0[4] = {20, 63, 106, 149};
		for (int i=0; i<BANDS; i++) {
			char fVal[10];
			snprintf(fVal, sizeof(fVal), "%1i", (int)module->freq[i]);
			nvgFillColor(vg,nvgRGBA(rescalef(clampf(module->peaks[i],0,1),0,1,0,255), 0, 0, 255));
			nvgText(vg, 56 + 43*i, 50, fVal, NULL);
		}
	}
};

MrBlueSkyWidget::MrBlueSkyWidget() {
	MrBlueSky *module = new MrBlueSky();
	setModule(module);
	box.size = Vec(15*52, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/MrBlueSky.svg")));
		addChild(panel);
	}

	MrBlueSkyDisplay *display = new MrBlueSkyDisplay();
	display->module = module;
	display->box.pos = Vec(12, 12);
	display->box.size = Vec(700, 70);
	addChild(display);

	static const float portX0[4] = {20, 63, 106, 149};

	for (int i = 0; i < BANDS; i++) {
		addParam( createParam<RoundBlackKnob>(Vec(50 + 43*i, 160), module, MrBlueSky::BG_PARAM + i, 0, 2, 1));
	}
	addParam(createParam<RoundSmallBlackKnob>(Vec(34, 205), module, MrBlueSky::ATTACK_PARAM, 0.0, 0.25, 0.0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(116, 205), module, MrBlueSky::DECAY_PARAM, 0.0, 0.25, 0.0));
	addParam(createParam<RoundBlackKnob>(Vec(15, 255), module, MrBlueSky::GMOD_PARAM, 1, 10, 1));
	addParam(createParam<RoundBlackKnob>(Vec(70, 255), module, MrBlueSky::GCARR_PARAM, 1, 10, 1));
	addParam(createParam<RoundBlackKnob>(Vec(125, 255), module, MrBlueSky::G_PARAM, 1, 10, 1));


	for (int i = 0; i < BANDS; i++) {
		addInput(createInput<PJ301MPort>(Vec(56 + 43*i, 110), module, MrBlueSky::CARRIER_IN + i));
	}
	addInput(createInput<PJ301MPort>(Vec(10, 330), module, MrBlueSky::IN_MOD));
	addInput(createInput<PJ301MPort>(Vec(48, 330), module, MrBlueSky::IN_CARR));

	for (int i = 0; i < BANDS; i++) {
		addOutput(createOutput<PJ301MPort>(Vec(56 + 43*i, 70), module, MrBlueSky::MOD_OUT + i));
	}
	addOutput(createOutput<PJ301MPort>(Vec(85, 330), module, MrBlueSky::OUT));

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));
}
