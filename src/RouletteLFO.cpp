#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"


#define BUFFER_SIZE 512

struct RouletteLFO : Module {
	enum ParamIds {
		RADIUS_RATIO_PARAM,
		RADIUS_RATIO_CV_ATTENUVERTER_PARAM,
		GENERATOR_ECCENTRICITY_PARAM,
		GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM,
		FIXED_ECCENTRICITY_PARAM,
		FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM,
		DISTANCE_PARAM,
		DISTANCE_CV_ATTENUVERTER_PARAM,
		FREQUENCY_PARAM,
		FREQUENCY_CV_ATTENUVERTER_PARAM,
		GENERATOR_PHASE_PARAM,
		GENERATOR_PHASE_CV_ATTENUVERTER_PARAM,
		FIXED_PHASE_PARAM,
		FIXED_PHASE_CV_ATTENUVERTER_PARAM,
		X_GAIN_PARAM,
		X_GAIN_CV_ATTENUVERTER_PARAM,
		Y_GAIN_PARAM,
		Y_GAIN_CV_ATTENUVERTER_PARAM,
		INSIDE_OUTSIDE_PARAM,	
		OFFSET_PARAM,	
		NUM_PARAMS
	};
	enum InputIds {
		RADIUS_RATIO_INPUT,
		GENERATOR_ECCENTRICITY_INPUT,
		GENERATOR_PHASE_INPUT,
		FIXED_ECCENTRICITY_INPUT,
		FIXED_PHASE_INPUT,
		DISTANCE_INPUT,
		FREQUENCY_INPUT,
		X_GAIN_INPUT,
		Y_GAIN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_X,
		OUTPUT_Y,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	enum RouletteTypes {
		INSIDE_ROULETTE,
		OUTSIDE_ROULETTE
	};
	

	float bufferX1[BUFFER_SIZE] = {};
	float bufferY1[BUFFER_SIZE] = {};
	float displayScaling = 1;
	int bufferIndex = 0;
	float frameIndex = 0;	
	float scopeDeltaTime = powf(2.0, -8);

	//SchmittTrigger resetTrigger;


	float x1 = 0.0;
	float y1 = 0.0;
	float fixedPhase = 0.0;
	float generatorPhase = 0.0;

	//percenatages
	float radiusRatioPercentage = 0;
	float fixedEccentricityPercentage = 0;
	float fixedPhasePercentage = 0;
	float generatorEccentricityPercentage = 0;
	float generatorPhasePercentage = 0;
	float distancePercentage = 0;
	float frequencyPercentage = 0;
	float xGainPercentage = 0;
	float yGainPercentage = 0;


	RouletteLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RADIUS_RATIO_PARAM, 1, 10.0, 2,"Radius Ration");
		configParam(GENERATOR_ECCENTRICITY_PARAM, 1.0f, 10.0f, 1.0,"Generator Eccentricty");
		configParam(GENERATOR_PHASE_PARAM, 0.0, 0.9999, 0.0,"Generator Phase","°",0,360);
		configParam(FIXED_ECCENTRICITY_PARAM, 1.0f, 10.0f, 1.0f,"Fixed Eccentricity");
		configParam(FIXED_PHASE_PARAM, 0.0, 0.9999, 0.0,"Fixed Phase","°",0,360);
		configParam(DISTANCE_PARAM, 0.1, 10.0, 1.0,"Pole Distance");
		configParam(FREQUENCY_PARAM, -8.0, 4.0, 0.0,"Frequency", " Hz", 2, 1);
		configParam(X_GAIN_PARAM, 0.0, 2.0, 1.0,"X Amp");
		configParam(Y_GAIN_PARAM, 0.0, 2.0, 1.0,"Y Amp");

		configParam(RADIUS_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Radius Ratio CV Attenuation","%",0,100);
		configParam(GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Genertor Eccentricity CV Attenuation","%",0,100);
		configParam(GENERATOR_PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Genertor Phase CV Attenuation","%",0,100);
		configParam(FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Fixed Phase CV Attenuation","%",0,100);
		configParam(FIXED_PHASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Fixed Eccentricity CV Attenuation","%",0,100);
		configParam(DISTANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Pole Distance CV Attenuation","%",0,100);
		configParam(FREQUENCY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Frequeny CV Attenuation","%",0,100);
		configParam(X_GAIN_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X Gain CV Attenuation","%",0,100);
		configParam(Y_GAIN_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y Gain CV Attenuation","%",0,100);


		configParam(INSIDE_OUTSIDE_PARAM, 0.0, 1.0, 0.0);		
		configParam(OFFSET_PARAM, 0.0, 1.0, 1.0);	
	}
	void process(const ProcessArgs &args) override {


		float xAmplitude = clamp(params[X_GAIN_PARAM].getValue() + (inputs[X_GAIN_INPUT].getVoltage() / 5.0f * params[X_GAIN_CV_ATTENUVERTER_PARAM].getValue()),0.0f,2.0f);
		float yAmplitude = clamp(params[Y_GAIN_PARAM].getValue() + (inputs[Y_GAIN_INPUT].getVoltage() / 5.0f * params[Y_GAIN_CV_ATTENUVERTER_PARAM].getValue()),0.0f,2.0f);
		xGainPercentage = xAmplitude / 2.0;
		yGainPercentage = yAmplitude / 2.0;

		float fixedInitialPhase = params[FIXED_PHASE_PARAM].getValue();
		if(inputs[FIXED_PHASE_INPUT].isConnected()) {
			fixedInitialPhase += (inputs[FIXED_PHASE_INPUT].getVoltage() / 10 * params[FIXED_PHASE_CV_ATTENUVERTER_PARAM].getValue());
		}
		if (fixedInitialPhase >= 1.0)
			fixedInitialPhase -= 1.0;
		else if (fixedInitialPhase < 0)
			fixedInitialPhase += 1.0;
		fixedPhasePercentage = fixedInitialPhase;

		float generatorInitialPhase = params[GENERATOR_PHASE_PARAM].getValue();
		if(inputs[GENERATOR_PHASE_INPUT].isConnected()) {
			generatorInitialPhase += (inputs[GENERATOR_PHASE_INPUT].getVoltage() / 10 * params[GENERATOR_PHASE_CV_ATTENUVERTER_PARAM].getValue());
		}
		if (generatorInitialPhase >= 1.0)
			generatorInitialPhase -= 1.0;
		else if (generatorInitialPhase < 0)
			generatorInitialPhase += 1.0;
		generatorPhasePercentage = generatorInitialPhase;


		float pitch = fminf(params[FREQUENCY_PARAM].getValue() + inputs[FREQUENCY_INPUT].getVoltage() * params[FREQUENCY_CV_ATTENUVERTER_PARAM].getValue(), 8.0);
		frequencyPercentage = (pitch + 8.0) / 12.0;
		float ratio = clamp(params[RADIUS_RATIO_PARAM].getValue() + inputs[RADIUS_RATIO_INPUT].getVoltage() * 2.0f * params[RADIUS_RATIO_CV_ATTENUVERTER_PARAM].getValue(),1.0,20.0);
		radiusRatioPercentage = ratio / 20.0;
		float eG = clamp(params[GENERATOR_ECCENTRICITY_PARAM].getValue() + inputs[GENERATOR_ECCENTRICITY_INPUT].getVoltage() * params[GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM].getValue(),1.0f,10.0f);
		generatorEccentricityPercentage = (eG - 1.0) / 9.0;
		float eF = clamp(params[FIXED_ECCENTRICITY_PARAM].getValue() + inputs[FIXED_ECCENTRICITY_INPUT].getVoltage() * params[FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM].getValue(),1.0f,10.0f);
		fixedEccentricityPercentage = (eF - 1.0) / 9.0;
		float d = clamp(params[DISTANCE_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage() * params[DISTANCE_CV_ATTENUVERTER_PARAM].getValue(),0.1,10.0f);
		distancePercentage = d/10.0;

		displayScaling = fmaxf(eF + eG/2.0 + d*0.5,1.0f);

		float freq = powf(2.0, pitch);
		float deltaTime = 1.0 / args.sampleRate;
		float deltaPhase = fminf(freq * deltaTime, 0.5);
		
		generatorPhase += deltaPhase * ratio;
		if (generatorPhase >= 1.0)
			generatorPhase -= 1.0;

		fixedPhase += deltaPhase; //Fixed shape rolls in opposite direction
		if (fixedPhase >= 1.0)
			fixedPhase -= 1.0;

		float generatorTheta = (generatorInitialPhase + generatorPhase) * 2 * M_PI;
		float fixedTheta = (fixedInitialPhase + fixedPhase) * 2 * M_PI;

		//Fixed object is always horizontal, so major and minor axis vectors are constant
		float fixedX = ratio * (eF*sinf(fixedTheta));
		float fixedY = ratio * (cosf(fixedTheta));

		float a0 = 0.0f;
		float b0 = 0.0f;
		float ax = eG * cosf(generatorTheta);
		float ay = eG * sinf(generatorTheta);
		float bx = cosf(generatorTheta - M_PI / 2);
		float by = sinf(generatorTheta - M_PI / 2);

		float generatorX = a0 + d * (ax*sinf(generatorTheta) + bx*cosf(generatorTheta));
		float generatorY = b0 + d * (ay*sinf(generatorTheta) + by*cosf(generatorTheta));

	// x(theta) = a0 + ax*sin(theta) + bx*cos(theta)
	// y(theta) = b0 + ay*sin(theta) + by*cos(theta)

	// (a0,b0) is the center of the ellipse
	// (ax,ay) vector representing the major axis
	// (bx,by) vector representing the minor axis

		//float scaling = eF + eg/2.0;


		if(params[INSIDE_OUTSIDE_PARAM].getValue() == INSIDE_ROULETTE) {
			x1 = (fixedX - generatorX) / ratio * xAmplitude;
			y1 = (fixedY - generatorY) / ratio * yAmplitude;
		} else {
			x1 = (fixedX + generatorX) / ratio * xAmplitude;
			y1 = (fixedY + generatorY) / ratio * yAmplitude;
		}
		//scaling += d > 1 ? d - 1 : 0;
		float scaling = 10.0f / (displayScaling + (eF + eG + d / 2.0f - 2));


		//Update scope.
		int frameCount = (int)ceilf(scopeDeltaTime * args.sampleRate);

		// Add frame to buffers
		if (bufferIndex < BUFFER_SIZE) {
			if (++frameIndex > frameCount) {
				frameIndex = 0;
				bufferX1[bufferIndex] = x1;
				bufferY1[bufferIndex] = y1;
				bufferIndex++;
			}
		}

		// Are we waiting on the next trigger?
		if (bufferIndex >= BUFFER_SIZE) {
			bufferIndex = 0;
			frameIndex = 0;
		}

		x1 = x1 * scaling;
		y1 = y1 * scaling;

		if(params[OFFSET_PARAM].getValue() == 1) {
			x1 = x1 + 5;
			y1 = y1 + 5;
		}
		


		
		outputs[OUTPUT_X].setVoltage(x1);
		outputs[OUTPUT_Y].setVoltage(y1);	
	}


	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};






struct RouletteScopeDisplay : TransparentWidget {
	RouletteLFO *module;
	int frame = 0;


	RouletteScopeDisplay() {
	}

	void drawWaveform(const DrawArgs &args, float *valuesX, float *valuesY) {
		if (!valuesX)
			return;
		nvgSave(args.vg);
		Rect b = Rect(Vec(0, 0), box.size);
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			Vec v;
			v.x = valuesX[i] / 1.5 + 0.5;
			v.y = valuesY[i] + 0.5;
			Vec p;
			p.x = rescale(v.x, 0.f, 1.f, 0, box.size.x);
			p.y = rescale(v.y, 0.f, 1.f, box.size.y, 0);
			if (i == 0)
				nvgMoveTo(args.vg, p.x, p.y);
			else
				nvgLineTo(args.vg, p.x, p.y);
		}
		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 2.0);
		nvgStrokeWidth(args.vg, 1.5);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgStroke(args.vg);
		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}

	


	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		float valuesX[BUFFER_SIZE];
		float valuesY[BUFFER_SIZE];
		float scaling = module->displayScaling;
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX1[j]) / (1.5f * scaling);
			valuesY[i] = (module->bufferY1[j]) / (1.5f * scaling);
		}

		nvgStrokeColor(args.vg, nvgRGBA(0x9f, 0xe4, 0x36, 0xc0));
		drawWaveform(args, valuesX, valuesY);
	}
};

struct RouletteLFOWidget : ModuleWidget {
	RouletteLFOWidget(RouletteLFO *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RouletteLFO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			RouletteScopeDisplay *display = new RouletteScopeDisplay();
			display->module = module;
			display->box.pos = Vec(0, 21);
			display->box.size = Vec(box.size.x, 131);
			addChild(display);
		}

		ParamWidget* radiusRatioParam = createParam<RoundSmallFWKnob>(Vec(10, 167), module, RouletteLFO::RADIUS_RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(radiusRatioParam)->percentage = &module->radiusRatioPercentage;
		}
		addParam(radiusRatioParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(12, 212), module, RouletteLFO::RADIUS_RATIO_CV_ATTENUVERTER_PARAM));

		ParamWidget* fixedEccentricityParam = createParam<RoundSmallFWKnob>(Vec(48, 167), module, RouletteLFO::FIXED_ECCENTRICITY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(fixedEccentricityParam)->percentage = &module->fixedEccentricityPercentage;
		}
		addParam(fixedEccentricityParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(50, 212), module, RouletteLFO::FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM));
		ParamWidget* fixedPhaseParam = createParam<RoundSmallFWKnob>(Vec(48, 247), module, RouletteLFO::FIXED_PHASE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(fixedPhaseParam)->percentage = &module->fixedPhasePercentage;
		}
		addParam(fixedPhaseParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(50, 292), module, RouletteLFO::FIXED_PHASE_CV_ATTENUVERTER_PARAM));


		ParamWidget* generatorEccentricityParam = createParam<RoundSmallFWKnob>(Vec(86, 167), module, RouletteLFO::GENERATOR_ECCENTRICITY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(generatorEccentricityParam)->percentage = &module->generatorEccentricityPercentage;
		}
		addParam(generatorEccentricityParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(88, 212), module, RouletteLFO::GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM));
		ParamWidget* generatorPhaseParam = createParam<RoundSmallFWKnob>(Vec(86, 247), module, RouletteLFO::GENERATOR_PHASE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(generatorPhaseParam)->percentage = &module->generatorPhasePercentage;
		}
		addParam(generatorPhaseParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(88, 292), module, RouletteLFO::GENERATOR_PHASE_CV_ATTENUVERTER_PARAM));


		ParamWidget* distanceParam = createParam<RoundSmallFWKnob>(Vec(124, 167), module, RouletteLFO::DISTANCE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(distanceParam)->percentage = &module->distancePercentage;
		}
		addParam(distanceParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(126, 212), module, RouletteLFO::DISTANCE_CV_ATTENUVERTER_PARAM));
		ParamWidget* frequencyParam = createParam<RoundSmallFWKnob>(Vec(160, 167), module, RouletteLFO::FREQUENCY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(frequencyParam)->percentage = &module->frequencyPercentage;
		}
		addParam(frequencyParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(162, 212), module, RouletteLFO::FREQUENCY_CV_ATTENUVERTER_PARAM));


		ParamWidget* xGainParam = createParam<RoundSmallFWKnob>(Vec(124, 247), module, RouletteLFO::X_GAIN_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(xGainParam)->percentage = &module->xGainPercentage;
		}
		addParam(xGainParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(126, 292), module, RouletteLFO::X_GAIN_CV_ATTENUVERTER_PARAM));

		ParamWidget* yGainParam = createParam<RoundSmallFWKnob>(Vec(160, 247), module, RouletteLFO::Y_GAIN_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(yGainParam)->percentage = &module->yGainPercentage;
		}
		addParam(yGainParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(162, 292), module, RouletteLFO::Y_GAIN_CV_ATTENUVERTER_PARAM));


		addParam(createParam<CKSS>(Vec(18, 327), module, RouletteLFO::INSIDE_OUTSIDE_PARAM));
		addParam(createParam<CKSS>(Vec(63, 327), module, RouletteLFO::OFFSET_PARAM));
		

		addInput(createInput<FWPortInSmall>(Vec(13, 193), module, RouletteLFO::RADIUS_RATIO_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(51, 193), module, RouletteLFO::FIXED_ECCENTRICITY_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(51, 273), module, RouletteLFO::FIXED_PHASE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(89, 193), module, RouletteLFO::GENERATOR_ECCENTRICITY_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(89, 273), module, RouletteLFO::GENERATOR_PHASE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(127, 193), module, RouletteLFO::DISTANCE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(163, 193), module, RouletteLFO::FREQUENCY_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(127, 273), module, RouletteLFO::X_GAIN_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(163, 273), module, RouletteLFO::Y_GAIN_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(110, 338), module, RouletteLFO::OUTPUT_X));
		addOutput(createOutput<PJ301MPort>(Vec(150, 338), module, RouletteLFO::OUTPUT_Y));

	}
};

Model *modelRouletteLFO = createModel<RouletteLFO, RouletteLFOWidget>("RouletteLFO");
