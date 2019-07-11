#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"


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
		INSIDE_OUTSIDE_PARAM,
		OFFSET_PARAM,	
		NUM_PARAMS
	};
	enum InputIds {
		RADIUS_RATIO_INPUT,
		GENERATOR_ECCENTRICITY_INPUT,
		FIXED_ECCENTRICITY_INPUT,
		DISTANCE_INPUT,
		FREQUENCY_INPUT,
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

	RouletteLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RADIUS_RATIO_PARAM, 1, 10.0, 2,"Radius Ration");
		configParam(GENERATOR_ECCENTRICITY_PARAM, 1.0f, 10.0f, 1.0,"Generator Eccentricty");
		configParam(FIXED_ECCENTRICITY_PARAM, 1.0f, 10.0f, 1.0f,"Fixed Eccentricity");
		configParam(DISTANCE_PARAM, 0.1, 10.0, 1.0,"Pole Distance");
		configParam(FREQUENCY_PARAM, -8.0, 4.0, 0.0,"Frequency", " Hz", 2, 1);

		configParam(RADIUS_RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Radius Ratio CV Attenuation","%",0,100);
		configParam(GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Genertor Eccentricity CV Attenuation","%",0,100);
		configParam(FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Fixed Eccentricity CV Attenuation","%",0,100);
		configParam(DISTANCE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Pole Distance CV Attenuation","%",0,100);
		configParam(FREQUENCY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Frequeny CV Attenuation","%",0,100);

		configParam(INSIDE_OUTSIDE_PARAM, 0.0, 1.0, 0.0);		
		configParam(OFFSET_PARAM, 0.0, 1.0, 1.0);	
	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void RouletteLFO::process(const ProcessArgs &args) {

	float pitch = fminf(params[FREQUENCY_PARAM].getValue() + inputs[FREQUENCY_INPUT].getVoltage() * params[FREQUENCY_CV_ATTENUVERTER_PARAM].getValue(), 8.0);
	float ratio = clamp(params[RADIUS_RATIO_PARAM].getValue() + inputs[RADIUS_RATIO_INPUT].getVoltage() * 2.0f * params[RADIUS_RATIO_CV_ATTENUVERTER_PARAM].getValue(),1.0,20.0);
	float eG = clamp(params[GENERATOR_ECCENTRICITY_PARAM].getValue() + inputs[GENERATOR_ECCENTRICITY_INPUT].getVoltage() * params[GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM].getValue(),1.0f,10.0f);
	float eF = clamp(params[FIXED_ECCENTRICITY_PARAM].getValue() + inputs[FIXED_ECCENTRICITY_INPUT].getVoltage() * params[FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM].getValue(),1.0f,10.0f);
	float d = clamp(params[DISTANCE_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage() * params[DISTANCE_CV_ATTENUVERTER_PARAM].getValue(),0.1,10.0f);

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

	float generatorTheta = generatorPhase * 2 * M_PI;
	float fixedTheta = fixedPhase * 2 * M_PI;

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
		x1 = (fixedX - generatorX) / ratio ;
		y1 = (fixedY - generatorY) / ratio ;
	} else {
		x1 = (fixedX + generatorX) / ratio ;
		y1 = (fixedY + generatorY) / ratio ;
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
	


	// if(params[EPI_HYPO_PARAM].getValue() == HYPOTROCHOID_ROULETTE) {
	// 	float r = clamp(params[ROTATING_RADIUS_PARAM].getValue() + inputs[ROATATING_RADIUS_INPUT].getVoltage(),1.0,10.0);
	// 	float Rx = clamp(params[FIXED_RADIUS_X_PARAM].getValue() +inputs[FIXED_RADIUS_X_INPUT].getVoltage(),r,20.0);
	// 	float Ry = clamp(params[FIXED_RADIUS_Y_PARAM].getValue() +inputs[FIXED_RADIUS_Y_INPUT].getVoltage(),r,20.0);
	// 	float d = clamp(params[DISTANCE_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage(),1.0,10.0);
	// 	if(params[FIXED_D_PARAM].getValue()) {
	// 		d=r;
	// 	}
	// 	if(params[FIXED_RY_PARAM].getValue()) {
	// 		Ry=Rx;
	// 	}

	// 	//float R = std::max(Rx,Ry);
	// 	float R =(Rx + Ry) / 2.0f;
	// 	float amplitudeScaling = 5.0 / (R-r+d);

	// 	float theta = phase * 2 * M_PI;
	// 	x1 = amplitudeScaling * (((Rx-r) * cosf(theta)) + (d * cosf((Rx-r)/r * theta)));
	// 	y1 = amplitudeScaling * (((Ry-r) * sinf(theta)) - (d * sinf((Ry-r)/r * theta)));
	// } else {
	// 	float r = clamp(params[ROTATING_RADIUS_PARAM].getValue() + inputs[ROATATING_RADIUS_INPUT].getVoltage(),1.0,10.0);
	// 	float Rx = clamp(params[FIXED_RADIUS_X_PARAM].getValue() +inputs[FIXED_RADIUS_X_INPUT].getVoltage(),r,20.0);
	// 	float Ry = clamp(params[FIXED_RADIUS_Y_PARAM].getValue() +inputs[FIXED_RADIUS_Y_INPUT].getVoltage(),r,20.0);
	// 	float d = clamp(params[DISTANCE_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage(),1.0,20.0);
	// 	if(params[FIXED_D_PARAM].getValue()) {
	// 		d=r;
	// 	}
	// 	if(params[FIXED_RY_PARAM].getValue()) {
	// 		Ry=Rx;
	// 	}
	// 	//float R = std::max(Rx,Ry);
	// 	float R =(Rx + Ry) / 2.0f;
	// 	float amplitudeScaling = 5.0 / (R+r+d);

	// 	float theta = phase * 2 * M_PI;
		// x1 = amplitudeScaling * (((R+r) * cosf(theta)) - (d * cosf((R+r)/r * theta)));
		// y1 = amplitudeScaling * (((R+r) * sinf(theta)) - (d * sinf((R+r)/r * theta)));

	// }
	outputs[OUTPUT_X].setVoltage(x1);
	outputs[OUTPUT_Y].setVoltage(y1);	
}





struct RouletteScopeDisplay : TransparentWidget {
	RouletteLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;


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
			display->box.pos = Vec(0, 41);
			display->box.size = Vec(box.size.x, 131);
			addChild(display);
		}

		addParam(createParam<RoundFWKnob>(Vec(10, 187), module, RouletteLFO::RADIUS_RATIO_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(13, 250), module, RouletteLFO::RADIUS_RATIO_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(48, 187), module, RouletteLFO::FIXED_ECCENTRICITY_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(51, 250), module, RouletteLFO::FIXED_ECCENTRICITY_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(86, 187), module, RouletteLFO::GENERATOR_ECCENTRICITY_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(89, 250), module, RouletteLFO::GENERATOR_ECCENTRICITY_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(124, 187), module, RouletteLFO::DISTANCE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(127, 250), module, RouletteLFO::DISTANCE_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(160, 187), module, RouletteLFO::FREQUENCY_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(163, 250), module, RouletteLFO::FREQUENCY_CV_ATTENUVERTER_PARAM));
		addParam(createParam<CKSS>(Vec(32, 295), module, RouletteLFO::INSIDE_OUTSIDE_PARAM));
		addParam(createParam<CKSS>(Vec(77, 295), module, RouletteLFO::OFFSET_PARAM));
		

		addInput(createInput<PJ301MPort>(Vec(13, 220), module, RouletteLFO::RADIUS_RATIO_INPUT));
		addInput(createInput<PJ301MPort>(Vec(51, 220), module, RouletteLFO::FIXED_ECCENTRICITY_INPUT));
		addInput(createInput<PJ301MPort>(Vec(89, 220), module, RouletteLFO::GENERATOR_ECCENTRICITY_INPUT));
		addInput(createInput<PJ301MPort>(Vec(127, 220), module, RouletteLFO::DISTANCE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(163, 220), module, RouletteLFO::FREQUENCY_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(110, 330), module, RouletteLFO::OUTPUT_X));
		addOutput(createOutput<PJ301MPort>(Vec(150, 330), module, RouletteLFO::OUTPUT_Y));

	}
};

Model *modelRouletteLFO = createModel<RouletteLFO, RouletteLFOWidget>("RouletteLFO");
