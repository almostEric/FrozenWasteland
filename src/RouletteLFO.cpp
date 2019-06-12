#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"


#define BUFFER_SIZE 512

struct RouletteLFO : Module {
	enum ParamIds {
		FIXED_RADIUS_PARAM,
		ROTATING_RADIUS_PARAM,
		DISTANCE_PARAM,
		FREQUENCY_PARAM,
		EPI_HYPO_PARAM,
		FIXED_D_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FIXED_RADIUS_INPUT,
		ROATATING_RADIUS_INPUT,
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
		HYPOTROCHOID_ROULETTE,
		EPITROCHIID_ROULETTE
	};
	

	float bufferX1[BUFFER_SIZE] = {};
	float bufferY1[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;	
	float scopeDeltaTime = powf(2.0, -8);

	//SchmittTrigger resetTrigger;


	float x1 = 0.0;
	float y1 = 0.0;
	float phase = 0.0;

	RouletteLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FIXED_RADIUS_PARAM, 1, 20.0, 5,"Fixed Radius");
		configParam(ROTATING_RADIUS_PARAM, 1, 10.0, 3,"Rotating Radius");
		configParam(DISTANCE_PARAM, 1, 10.0, 5.0,"Distance");
		configParam(FREQUENCY_PARAM, -8.0, 4.0, 0.0,"Frequency", " Hz", 2, 1);
		configParam(EPI_HYPO_PARAM, 0.0, 1.0, 0.0);
		configParam(FIXED_D_PARAM, 0.0, 1.0, 0.0);
	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void RouletteLFO::process(const ProcessArgs &args) {

	float pitch = fminf(params[FREQUENCY_PARAM].getValue() + inputs[FREQUENCY_INPUT].getVoltage(), 8.0);
	float freq = powf(2.0, pitch);
	float deltaTime = 1.0 / args.sampleRate;
	float deltaPhase = fminf(freq * deltaTime, 0.5);
	phase += deltaPhase;
	if (phase >= 1.0)
		phase -= 1.0;

	if(params[EPI_HYPO_PARAM].getValue() == HYPOTROCHOID_ROULETTE) {
		float r = clamp(params[ROTATING_RADIUS_PARAM].getValue() + inputs[ROATATING_RADIUS_INPUT].getVoltage(),1.0,10.0);
		float R = clamp(params[FIXED_RADIUS_PARAM].getValue() +inputs[FIXED_RADIUS_INPUT].getVoltage(),r,20.0);
		float d = clamp(params[DISTANCE_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage(),1.0,10.0);
		if(params[FIXED_D_PARAM].getValue()) {
			d=r;
		}

		float amplitudeScaling = 5.0 / (R-r+d);

		float theta = phase * 2 * M_PI;
		x1 = amplitudeScaling * (((R-r) * cosf(theta)) + (d * cosf((R-r)/r * theta)));
		y1 = amplitudeScaling * (((R-r) * sinf(theta)) - (d * sinf((R-r)/r * theta)));
	} else {
		float R = clamp(params[FIXED_RADIUS_PARAM].getValue() +inputs[FIXED_RADIUS_INPUT].getVoltage(),1.0,20.0);
		float r = clamp(params[ROTATING_RADIUS_PARAM].getValue() + inputs[ROATATING_RADIUS_INPUT].getVoltage(),1.0,10.0);
		float d = clamp(params[DISTANCE_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage(),1.0,20.0);
		if(params[FIXED_D_PARAM].getValue()) {
			d=r;
		}

		float amplitudeScaling = 5.0 / (R+r+d);

		float theta = phase * 2 * M_PI;
		x1 = amplitudeScaling * (((R+r) * cosf(theta)) - (d * cosf((R+r)/r * theta)));
		y1 = amplitudeScaling * (((R+r) * sinf(theta)) - (d * sinf((R+r)/r * theta)));

	}
	outputs[OUTPUT_X].setVoltage(x1);
	outputs[OUTPUT_Y].setVoltage(y1);
	

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
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float x, y;
			if (valuesY) {
				x = valuesX[i] / 2.0 + 0.5;
				y = valuesY[i] / 2.0 + 0.5;
			}
			else {
				x = (float)i / (BUFFER_SIZE - 1);
				y = valuesX[i] / 2.0 + 0.5;
			}
			Vec p;
			p.x = b.pos.x + b.size.x * x;
			p.y = b.pos.y + b.size.y * (1.0 - y);
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
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX1[j]) / 5.0;
			valuesY[i] = (module->bufferY1[j]) / 5.0;
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
			display->box.pos = Vec(0, 35);
			display->box.size = Vec(box.size.x, 140);
			addChild(display);
		}

		addParam(createParam<RoundFWKnob>(Vec(10, 186), module, RouletteLFO::FIXED_RADIUS_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(60, 186), module, RouletteLFO::ROTATING_RADIUS_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(113, 186), module, RouletteLFO::DISTANCE_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(160, 186), module, RouletteLFO::FREQUENCY_PARAM));
		addParam(createParam<CKSS>(Vec(55, 265), module, RouletteLFO::EPI_HYPO_PARAM));
		addParam(createParam<CKSS>(Vec(130, 265), module, RouletteLFO::FIXED_D_PARAM));

		addInput(createInput<PJ301MPort>(Vec(13, 220), module, RouletteLFO::FIXED_RADIUS_INPUT));
		addInput(createInput<PJ301MPort>(Vec(63, 220), module, RouletteLFO::ROATATING_RADIUS_INPUT));
		addInput(createInput<PJ301MPort>(Vec(116, 220), module, RouletteLFO::DISTANCE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(163, 220), module, RouletteLFO::FREQUENCY_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(57, 335), module, RouletteLFO::OUTPUT_X));
		addOutput(createOutput<PJ301MPort>(Vec(113, 335), module, RouletteLFO::OUTPUT_Y));

	}
};

Model *modelRouletteLFO = createModel<RouletteLFO, RouletteLFOWidget>("RouletteLFO");
