#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define BUFFER_SIZE 512

struct LissajousLFO : Module {
	enum ParamIds {
		AMPLITUDE1_PARAM,
		AMPLITUDE2_PARAM,
		FREQX1_PARAM,
		FREQY1_PARAM,
		PHASEX1_PARAM,		
		WAVESHAPEX1_PARAM,
		WAVESHAPEY1_PARAM,
		SKEWX1_PARAM,
		SKEWY1_PARAM,
		FREQX2_PARAM,
		FREQY2_PARAM,
		PHASEX2_PARAM,
		WAVESHAPEX2_PARAM,
		WAVESHAPEY2_PARAM,
		SKEWX2_PARAM,
		SKEWY2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AMPLITUDE1_INPUT,
		AMPLITUDE2_INPUT,
		FREQX1_INPUT,
		FREQY1_INPUT,
		PHASEX1_INPUT,
		WAVESHAPEX1_INPUT,
		WAVESHAPEY1_INPUT,
		SKEWX1_INPUT,
		SKEWY1_INPUT,
		FREQX2_INPUT,
		FREQY2_INPUT,
		PHASEX2_INPUT,
		WAVESHAPEX2_INPUT,
		WAVESHAPEY2_INPUT,
		SKEWX2_INPUT,
		SKEWY2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUT_5,
		OUTPUT_6,
		OUTPUT_7,
		OUTPUT_8,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT_1,
		BLINK_LIGHT_2,
		BLINK_LIGHT_3,
		BLINK_LIGHT_4,
		NUM_LIGHTS
	};

	struct LowFrequencyOscillator {
		float basePhase = 0.0;
		float phase = 0.0;
		float freq = 1.0;
		float skew = 0.5; // Triangle
		float waveSlope = 1.0; //Original (1 is sin)
		bool offset = false;
		
		float lerp(float v0, float v1, float t) {
			return (1 - t) * v0 + t * v1;
		}
		
		void setPitch(float pitch) {
			pitch = fminf(pitch, 8.0);
			freq = powf(2.0, pitch);
		}

		void setBasePhase(float initialPhase) {
			//Apply change, then remember
			phase += initialPhase - basePhase;
			if (phase >= 1.0)
				phase -= 1.0;
			else if (phase < 0)
				phase += 1.0;	
			basePhase = initialPhase;
		}
		

		void step(float dt) {
			float deltaPhase = fminf(freq * dt, 0.5);
			phase += deltaPhase;
			if (phase >= 1.0)
				phase -= 1.0;
		}

		float skewsaw(float x) {
			
			x = std::fabs(std::fmod(x ,1.0f));

			float inverseSkew = 1 - skew;
			float originalWave;
			if (skew == 0 && x == 0) //Avoid /0 error
				originalWave = 2;
			if (x <= skew)
				originalWave = 2.0 * (1- (-1 / skew * x + 1));
			else
				originalWave = 2.0 * (1-(1 / inverseSkew * (x - skew)));
			return originalWave;
		}
		
		float skewsaw() {
			if (offset)
				return lerp(skewsaw(phase),sin(),waveSlope);
			
			return lerp(skewsaw(phase) - 1 ,sin(),waveSlope);skewsaw(phase) ; //Going to keep same phase for now				
		}

		float sin() {
			if (offset)
				return 1.0 - cosf(2*M_PI * phase) ;
			else
				return sinf(2*M_PI * phase) ;
		}

	};

	

	float phase = 0.0;

	LowFrequencyOscillator oscillatorX1,oscillatorY1,oscillatorX2,oscillatorY2;

	float bufferX1[BUFFER_SIZE] = {};
	float bufferY1[BUFFER_SIZE] = {};
	float bufferX2[BUFFER_SIZE] = {};
	float bufferY2[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;
	float deltaTime = powf(2.0, -8);

	//SchmittTrigger resetTrigger;


	float x1 = 0.0;
	float y1 = 0.0;
	float x2 = 0.0;
	float y2 = 0.0;

	LissajousLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		configParam(AMPLITUDE1_PARAM, 0.0, 5.0, 2.5,"Amplitude 1","%",0,25);
		configParam(FREQX1_PARAM, -8.0, 3.0, 0.0,"X 1 Frequency", " Hz", 2, 1);
		configParam(FREQY1_PARAM, -8.0, 3.0, 2.0,"Y 1 Frequency", " Hz", 2, 1);
		configParam(PHASEX1_PARAM, 0.0, 0.9999, 0.0,"Phase X 1","°",0,360);
		configParam(WAVESHAPEX1_PARAM, 0.0, 1.0, 1.0,"Wave Shape X 1","%",0,100);
		configParam(WAVESHAPEY1_PARAM, 0.0, 1.0, 1.0,"Wave Shape Y 1","%",0,100);
		configParam(SKEWX1_PARAM, 0.0, 1.0, 0.5,"Skew X 1","%",0,100);
		configParam(SKEWY1_PARAM, 0.0, 1.0, 0.5,"Skew Y 1","%",0,100);
		

		configParam(AMPLITUDE2_PARAM, 0.0, 5.0, 2.5,"Amplitude 1","%",0,25);
		configParam(FREQX2_PARAM, -8.0, 3.0, 0.0,"X 2 Frequency", " Hz", 2, 1);
		configParam(FREQY2_PARAM, -8.0, 3.0, 1.0,"Y 2 Frequency", " Hz", 2, 1);
		configParam(PHASEX2_PARAM, 0.0, 0.9999, 0.0,"Phase X 2","°",0,360);
		configParam(WAVESHAPEX2_PARAM, 0.0, 1.0, 1.0,"Wave Shape X 2","%",0,100);
		configParam(WAVESHAPEY2_PARAM, 0.0, 1.0, 1.0,"Wave Shape Y 2","%",0,100);
		configParam(SKEWX2_PARAM, 0.0, 1.0, 0.5,"Skew X 2","%",0,100);
		configParam(SKEWY2_PARAM, 0.0, 1.0, 0.5,"Skew Y 2","%",0,100);
	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void LissajousLFO::process(const ProcessArgs &args) {

	float initialPhase;

	float amplitude1 = clamp(params[AMPLITUDE1_PARAM].getValue() + (inputs[AMPLITUDE1_INPUT].getVoltage() / 2.0f),0.0f,5.0f);
	float amplitude2 = clamp(params[AMPLITUDE2_PARAM].getValue() + (inputs[AMPLITUDE2_INPUT].getVoltage() / 2.0f),0.0f,5.0f);


	// Implement 4 oscillators
	oscillatorX1.setPitch(params[FREQX1_PARAM].getValue() + inputs[FREQX1_INPUT].getVoltage());
	initialPhase = params[PHASEX1_PARAM].getValue() + inputs[PHASEX1_INPUT].getVoltage();
	if (initialPhase >= 1.0)
		initialPhase -= 1.0;
	else if (initialPhase < 0)
		initialPhase += 1.0;
	oscillatorX1.setBasePhase(initialPhase);
	oscillatorX1.waveSlope = clamp(params[WAVESHAPEX1_PARAM].getValue() + inputs[WAVESHAPEX1_INPUT].getVoltage(),0.0f,1.0f);
	oscillatorX1.skew = clamp(params[SKEWX1_PARAM].getValue() + inputs[SKEWX1_INPUT].getVoltage(),0.0f,1.0f);
	
	oscillatorY1.setPitch(params[FREQY1_PARAM].getValue() + inputs[FREQY1_INPUT].getVoltage());
	oscillatorY1.waveSlope = clamp(params[WAVESHAPEY1_PARAM].getValue() + inputs[WAVESHAPEY1_INPUT].getVoltage(),0.0f,1.0f);
	oscillatorY1.skew = clamp(params[SKEWY1_PARAM].getValue() + inputs[SKEWY1_INPUT].getVoltage(),0.0f,1.0f);
	
	oscillatorX2.setPitch(params[FREQX2_PARAM].getValue() + inputs[FREQX2_INPUT].getVoltage());
	initialPhase = params[PHASEX2_PARAM].getValue() + inputs[PHASEX2_INPUT].getVoltage();
	if (initialPhase >= 1.0)
		initialPhase -= 1.0;
	else if (initialPhase < 0)
		initialPhase += 1.0;
	oscillatorX2.setBasePhase(initialPhase);
	oscillatorX2.waveSlope = clamp(params[WAVESHAPEX2_PARAM].getValue() + inputs[WAVESHAPEX2_INPUT].getVoltage(),0.0f,1.0f);
	oscillatorX2.skew = clamp(params[SKEWX2_PARAM].getValue() + inputs[SKEWX2_INPUT].getVoltage(),0.0f,1.0f);
	
	oscillatorY2.setPitch(params[FREQY2_PARAM].getValue() + inputs[FREQY2_INPUT].getVoltage());
	oscillatorY2.waveSlope = clamp(params[WAVESHAPEY2_PARAM].getValue() + inputs[WAVESHAPEY2_INPUT].getVoltage(),0.0f,1.0f);
	oscillatorY2.skew = clamp(params[SKEWY2_PARAM].getValue() + inputs[SKEWY2_INPUT].getVoltage(),0.0f,1.0f);


	oscillatorX1.step(1.0 / args.sampleRate);
	oscillatorY1.step(1.0 / args.sampleRate);
	oscillatorX2.step(1.0 / args.sampleRate);
	oscillatorY2.step(1.0 / args.sampleRate);

	
	float x1 = amplitude1 * oscillatorX1.skewsaw();
	float y1 = amplitude1 * oscillatorY1.skewsaw();
	float x2 = amplitude2 * oscillatorX2.skewsaw();
	float y2 = amplitude2 * oscillatorY2.skewsaw();

	outputs[OUTPUT_1].setVoltage((x1 + x2) / 2);
	outputs[OUTPUT_2].setVoltage((y1 + y2) / 2);
	outputs[OUTPUT_3].setVoltage((x1 + x2 + y1 + y2) / 4);
	float out4 = (x1/x2);
	outputs[OUTPUT_4].setVoltage(std::isfinite(out4) ? clamp(out4,-5.0f,5.0f) : 0.f);
	float out5 = (y1/y2);
	outputs[OUTPUT_5].setVoltage(std::isfinite(out5) ? clamp(out5,-5.0f,5.0f) : 0.f);
	float out6 = (x1*x2);
	outputs[OUTPUT_6].setVoltage(clamp(out6,-5.0f,5.0f) );
	float out7 = (y1*y2);
	outputs[OUTPUT_7].setVoltage(clamp(out7,-5.0f,5.0f) );
	float out8 = (x1*x2*y1*y2);
	outputs[OUTPUT_8].setVoltage(clamp(out8,-5.0f,5.0f) );

	//Update scope.
	int frameCount = (int)ceilf(deltaTime * args.sampleRate);

	// Add frame to buffers
	if (bufferIndex < BUFFER_SIZE) {
		if (++frameIndex > frameCount) {
			frameIndex = 0;
			bufferX1[bufferIndex] = x1;
			bufferY1[bufferIndex] = y1;
			bufferX2[bufferIndex] = x2;
			bufferY2[bufferIndex] = y2;
			bufferIndex++;
		}
	}

	// Are we waiting on the next trigger?
	if (bufferIndex >= BUFFER_SIZE) {
		bufferIndex = 0;
		frameIndex = 0;
	}
}





struct ScopeDisplay : TransparentWidget {
	LissajousLFO *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin, vmax;
		void calculate(float *values) {
			vrms = 0.0;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	ScopeDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
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

		float gainX = powf(2.0, 1);
		float gainY = powf(2.0, 1);
		//float offsetX = module->x1;
		//float offsetY = module->y1;

		float valuesX[BUFFER_SIZE];
		float valuesY[BUFFER_SIZE];
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX1[j]) * gainX / 10.0;
			valuesY[i] = (module->bufferY1[j]) * gainY / 10.0;
		}

		// Draw waveforms for LFO 1
		// X x Y
		nvgStrokeColor(args.vg, nvgRGBA(0x9f, 0xe4, 0x36, 0xc0));
		drawWaveform(args, valuesX, valuesY);


		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX2[j]) * gainX / 10.0;
			valuesY[i] = (module->bufferY2[j]) * gainY / 10.0;
		}

		// Draw waveforms for LFO 2
		// X x Y
		nvgStrokeColor(args.vg, nvgRGBA(0x3f, 0xe4, 0x96, 0xc0));
		drawWaveform(args, valuesX, valuesY);
	}
};

struct LissajousLFOWidget : ModuleWidget {
	LissajousLFOWidget(LissajousLFO *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LissajousLFO.svg")));
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			ScopeDisplay *display = new ScopeDisplay();
			display->module = module;
			display->box.pos = Vec(0, 35);
			display->box.size = Vec(box.size.x, 140);
			addChild(display);
		}

		addParam(createParam<RoundFWKnob>(Vec(20, 186), module, LissajousLFO::AMPLITUDE1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(52, 186), module, LissajousLFO::FREQX1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(87, 186), module, LissajousLFO::FREQY1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(122, 186), module, LissajousLFO::PHASEX1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(157, 186), module, LissajousLFO::WAVESHAPEX1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(192, 186), module, LissajousLFO::WAVESHAPEY1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(227, 186), module, LissajousLFO::SKEWX1_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(262, 186), module, LissajousLFO::SKEWY1_PARAM));
		

		addParam(createParam<RoundFWKnob>(Vec(20, 265), module, LissajousLFO::AMPLITUDE2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(52, 265), module, LissajousLFO::FREQX2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(87, 265), module, LissajousLFO::FREQY2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(122, 265), module, LissajousLFO::PHASEX2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(157, 265), module, LissajousLFO::WAVESHAPEX2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(192, 265), module, LissajousLFO::WAVESHAPEY2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(227, 265), module, LissajousLFO::SKEWX2_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(262, 265), module, LissajousLFO::SKEWY2_PARAM));

		addInput(createInput<PJ301MPort>(Vec(22, 219), module, LissajousLFO::AMPLITUDE1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 219), module, LissajousLFO::FREQX1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(89, 219), module, LissajousLFO::FREQY1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(124, 219), module, LissajousLFO::PHASEX1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(159, 219), module, LissajousLFO::WAVESHAPEX1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(194, 219), module, LissajousLFO::WAVESHAPEY1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(229, 219), module, LissajousLFO::SKEWX1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(264, 219), module, LissajousLFO::SKEWY1_INPUT));
		
		addInput(createInput<PJ301MPort>(Vec(22, 298), module, LissajousLFO::AMPLITUDE2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(54, 298), module, LissajousLFO::FREQX2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(89, 298), module, LissajousLFO::FREQY2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(124, 298), module, LissajousLFO::PHASEX2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(159, 298), module, LissajousLFO::WAVESHAPEX2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(194, 298), module, LissajousLFO::WAVESHAPEY2_INPUT));
		addInput(createInput<PJ301MPort>(Vec(229, 298), module, LissajousLFO::SKEWX1_INPUT));
		addInput(createInput<PJ301MPort>(Vec(264, 298), module, LissajousLFO::SKEWY1_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(22, 338), module, LissajousLFO::OUTPUT_1));
		addOutput(createOutput<PJ301MPort>(Vec(53, 338), module, LissajousLFO::OUTPUT_2));
		addOutput(createOutput<PJ301MPort>(Vec(86, 338), module, LissajousLFO::OUTPUT_3));
		addOutput(createOutput<PJ301MPort>(Vec(126, 338), module, LissajousLFO::OUTPUT_4));
		addOutput(createOutput<PJ301MPort>(Vec(158, 338), module, LissajousLFO::OUTPUT_5));	
		addOutput(createOutput<PJ301MPort>(Vec(158, 338), module, LissajousLFO::OUTPUT_5));	
		addOutput(createOutput<PJ301MPort>(Vec(190, 338), module, LissajousLFO::OUTPUT_6));	
		addOutput(createOutput<PJ301MPort>(Vec(222, 338), module, LissajousLFO::OUTPUT_7));	
		addOutput(createOutput<PJ301MPort>(Vec(254, 338), module, LissajousLFO::OUTPUT_8));	
	}
};

Model *modelLissajousLFO = createModel<LissajousLFO, LissajousLFOWidget>("LissajousLFO");
