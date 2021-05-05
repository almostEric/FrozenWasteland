#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

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
		AMPLITUDE1_CV_ATTENUVERTER_PARAM,
		AMPLITUDE2_CV_ATTENUVERTER_PARAM,
		FREQX1_CV_ATTENUVERTER_PARAM,
		FREQY1_CV_ATTENUVERTER_PARAM,
		PHASEX1_CV_ATTENUVERTER_PARAM,		
		WAVESHAPEX1_CV_ATTENUVERTER_PARAM,
		WAVESHAPEY1_CV_ATTENUVERTER_PARAM,
		SKEWX1_CV_ATTENUVERTER_PARAM,
		SKEWY1_CV_ATTENUVERTER_PARAM,
		FREQX2_CV_ATTENUVERTER_PARAM,
		FREQY2_CV_ATTENUVERTER_PARAM,
		PHASEX2_CV_ATTENUVERTER_PARAM,
		WAVESHAPEX2_CV_ATTENUVERTER_PARAM,
		WAVESHAPEY2_CV_ATTENUVERTER_PARAM,
		SKEWX2_CV_ATTENUVERTER_PARAM,
		SKEWY2_CV_ATTENUVERTER_PARAM,
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
			if (offset) // Sin wave is 90 degrees out of phase with other waves
				return 1.0 - cosf(2*M_PI * (phase - 0.25)) ;
			else
				return sinf(2*M_PI * (phase - 0.25)) ;
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

	//percentages

	float amplitude1Percentage = 0;
	float amplitude2Percentage = 0;
	float freqX1Percentage = 0;
	float freqY1Percentage = 0;
	float freqX2Percentage = 0;
	float freqY2Percentage = 0;
	float phaseX1Percentage = 0;
	float phaseX2Percentage = 0;
	float waveShapeX1Percentage = 0;
	float waveShapeY1Percentage = 0;
	float waveShapeX2Percentage = 0;
	float waveShapeY2Percentage = 0;
	float skewX1Percentage = 0;
	float skewY1Percentage = 0;
	float skewX2Percentage = 0;
	float skewY2Percentage = 0;

	LissajousLFO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		configParam(AMPLITUDE1_PARAM, 0.0, 5.0, 2.5,"Amplitude 1","%",0,25);
		configParam(FREQX1_PARAM, -8.0, 3.0, 0.0,"X 1 Frequency", " Hz", 2, 1);
		configParam(FREQY1_PARAM, -8.0, 3.0, 2.0,"Y 1 Frequency", " Hz", 2, 1);
		configParam(PHASEX1_PARAM, 0.0, 0.9999, 0.0,"X 1 Phase","°",0,360);
		configParam(WAVESHAPEX1_PARAM, 0.0, 1.0, 1.0,"X 1 Wave Shape","%",0,100);
		configParam(WAVESHAPEY1_PARAM, 0.0, 1.0, 1.0,"Y 1 Wave Shape ","%",0,100);
		configParam(SKEWX1_PARAM, 0.0, 1.0, 0.5,"X 1 Skew","%",0,100);
		configParam(SKEWY1_PARAM, 0.0, 1.0, 0.5,"Y 1 Skew","%",0,100);

		configParam(AMPLITUDE1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Amplitude 1 CV Attenuation","%",0,100);
		configParam(FREQX1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 1 Frequency CV Attenuation","%",0,100);
		configParam(FREQY1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y 1 Frequency CV Attenuation","%",0,100);
		configParam(PHASEX1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 1 Phase CV Attenuation","%",0,100);
		configParam(WAVESHAPEX1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 1 Wave Shape CV Attenuation","%",0,100);
		configParam(WAVESHAPEY1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y 1 Wave Shape CV Attenuation","%",0,100);
		configParam(SKEWX1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 1 Skew CV Attenuation","%",0,100);
		configParam(SKEWY1_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y 1 Skew CV Attenuation","%",0,100);
		
		configParam(AMPLITUDE2_PARAM, 0.0, 5.0, 2.5,"Amplitude 1","%",0,25);
		configParam(FREQX2_PARAM, -8.0, 3.0, 0.0,"X 2 Frequency", " Hz", 2, 1);
		configParam(FREQY2_PARAM, -8.0, 3.0, 1.0,"Y 2 Frequency", " Hz", 2, 1);
		configParam(PHASEX2_PARAM, 0.0, 0.9999, 0.0,"Phase X 2","°",0,360);
		configParam(WAVESHAPEX2_PARAM, 0.0, 1.0, 1.0,"Wave Shape X 2","%",0,100);
		configParam(WAVESHAPEY2_PARAM, 0.0, 1.0, 1.0,"Wave Shape Y 2","%",0,100);
		configParam(SKEWX2_PARAM, 0.0, 1.0, 0.5,"Skew X 2","%",0,100);
		configParam(SKEWY2_PARAM, 0.0, 1.0, 0.5,"Skew Y 2","%",0,100);

		configParam(AMPLITUDE2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Amplitude 2 CV Attenuation","%",0,100);
		configParam(FREQX2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 2 Frequency CV Attenuation","%",0,100);
		configParam(FREQY2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y 2 Frequency CV Attenuation","%",0,100);
		configParam(PHASEX2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 2 Phase CV Attenuation","%",0,100);
		configParam(WAVESHAPEX2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 2 Wave Shape CV Attenuation","%",0,100);
		configParam(WAVESHAPEY2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y 2 Wave Shape CV Attenuation","%",0,100);
		configParam(SKEWX2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"X 2 Skew CV Attenuation","%",0,100);
		configParam(SKEWY2_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Y 2 Skew CV Attenuation","%",0,100);		
	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};
 

void LissajousLFO::process(const ProcessArgs &args) {

	float initialPhase;

	float amplitude1 = clamp(params[AMPLITUDE1_PARAM].getValue() + (inputs[AMPLITUDE1_INPUT].getVoltage() * params[AMPLITUDE1_CV_ATTENUVERTER_PARAM].getValue() / 2.0f),0.0f,5.0f);
	amplitude1Percentage = amplitude1 / 5.0;
	float amplitude2 = clamp(params[AMPLITUDE2_PARAM].getValue() + (inputs[AMPLITUDE2_INPUT].getVoltage() * params[AMPLITUDE2_CV_ATTENUVERTER_PARAM].getValue() / 2.0f),0.0f,5.0f);
	amplitude2Percentage = amplitude2 / 5.0;

	// Implement 4 oscillators
	float freqX1 = clamp(params[FREQX1_PARAM].getValue() + (inputs[FREQX1_INPUT].getVoltage() * params[FREQX1_CV_ATTENUVERTER_PARAM].getValue()),-8.0f,3.0f);
	freqX1Percentage = (freqX1 + 8.0) / 11.0;
	oscillatorX1.setPitch(freqX1);
	initialPhase = params[PHASEX1_PARAM].getValue() + (inputs[PHASEX1_INPUT].getVoltage() * params[PHASEX1_CV_ATTENUVERTER_PARAM].getValue() / 10.0);
	if (initialPhase >= 1.0)
		initialPhase -= 1.0;
	else if (initialPhase < 0)
		initialPhase += 1.0;
	phaseX1Percentage = initialPhase;
	oscillatorX1.setBasePhase(initialPhase);
	float waveShapeX1 =clamp(params[WAVESHAPEX1_PARAM].getValue() + (inputs[WAVESHAPEX1_INPUT].getVoltage() * params[WAVESHAPEX1_CV_ATTENUVERTER_PARAM].getValue() / 10.0),0.0f,1.0f);
	waveShapeX1Percentage = waveShapeX1;
	oscillatorX1.waveSlope = waveShapeX1;
	float skewX1 = clamp(params[SKEWX1_PARAM].getValue() + (inputs[SKEWX1_INPUT].getVoltage() * params[SKEWX1_CV_ATTENUVERTER_PARAM].getValue() / 10.0 ),0.0f,1.0f);
	skewX1Percentage = skewX1;
	oscillatorX1.skew = skewX1;
	
	float freqY1 = clamp(params[FREQY1_PARAM].getValue() + (inputs[FREQY1_INPUT].getVoltage() * params[FREQY1_CV_ATTENUVERTER_PARAM].getValue()),-8.0f,3.0f);
	freqY1Percentage = (freqY1 + 8.0) / 11.0;
	oscillatorY1.setPitch(freqY1);
	float waveShapeY1 =clamp(params[WAVESHAPEY1_PARAM].getValue() + (inputs[WAVESHAPEY1_INPUT].getVoltage() * params[WAVESHAPEY1_CV_ATTENUVERTER_PARAM].getValue() / 10.0),0.0f,1.0f);
	waveShapeY1Percentage = waveShapeY1;
	oscillatorY1.waveSlope = waveShapeY1;
	float skewY1 = clamp(params[SKEWY1_PARAM].getValue() + (inputs[SKEWY1_INPUT].getVoltage() * params[SKEWY1_CV_ATTENUVERTER_PARAM].getValue() / 10.0 ),0.0f,1.0f);
	skewY1Percentage = skewY1;
	oscillatorY1.skew = skewY1;
	
	float freqX2 = clamp(params[FREQX2_PARAM].getValue() + (inputs[FREQX2_INPUT].getVoltage() * params[FREQX2_CV_ATTENUVERTER_PARAM].getValue()),-8.0f,3.0f);
	freqX2Percentage = (freqX2 + 8.0) / 11.0;
	oscillatorX2.setPitch(freqX2);
	initialPhase = params[PHASEX2_PARAM].getValue() + (inputs[PHASEX2_INPUT].getVoltage() * params[PHASEX2_CV_ATTENUVERTER_PARAM].getValue() / 10.0);
	if (initialPhase >= 1.0)
		initialPhase -= 1.0;
	else if (initialPhase < 0)
		initialPhase += 1.0;
	phaseX2Percentage = initialPhase;
	oscillatorX2.setBasePhase(initialPhase);
	float waveShapeX2 =clamp(params[WAVESHAPEX2_PARAM].getValue() + (inputs[WAVESHAPEX2_INPUT].getVoltage() * params[WAVESHAPEX2_CV_ATTENUVERTER_PARAM].getValue() / 10.0),0.0f,1.0f);
	waveShapeX2Percentage = waveShapeX2;
	oscillatorX2.waveSlope = waveShapeX2;
	float skewX2 = clamp(params[SKEWX2_PARAM].getValue() + (inputs[SKEWX2_INPUT].getVoltage() * params[SKEWX2_CV_ATTENUVERTER_PARAM].getValue() / 10.0 ),0.0f,1.0f);
	skewX2Percentage = skewX2;
	oscillatorX2.skew = skewX2;
	
	float freqY2 = clamp(params[FREQY2_PARAM].getValue() + (inputs[FREQY2_INPUT].getVoltage() * params[FREQY2_CV_ATTENUVERTER_PARAM].getValue()),-8.0f,3.0f);
	freqY2Percentage = (freqY2 + 8.0) / 11.0;
	oscillatorY2.setPitch(freqY2);
	float waveShapeY2 =clamp(params[WAVESHAPEY2_PARAM].getValue() + (inputs[WAVESHAPEY2_INPUT].getVoltage() * params[WAVESHAPEY2_CV_ATTENUVERTER_PARAM].getValue() / 10.0),0.0f,1.0f);
	waveShapeY2Percentage = waveShapeY2;
	oscillatorY2.waveSlope = waveShapeY2;
	float skewY2 = clamp(params[SKEWY2_PARAM].getValue() + (inputs[SKEWY2_INPUT].getVoltage() * params[SKEWY2_CV_ATTENUVERTER_PARAM].getValue() / 10.0 ),0.0f,1.0f);
	skewY2Percentage = skewY2;
	oscillatorY2.skew = skewY2;


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
		nvgScissor(args.vg, box.pos.x, box.pos.y, box.size.x, box.size.y);
		nvgBeginPath(args.vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			Vec v;
			v.x = valuesX[i] / 2.0 + 0.5;
			v.y = valuesY[i] / 2.0 + 0.5;
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
			display->box.pos = Vec(0, 11);
			display->box.size = Vec(box.size.x, 140);
			addChild(display);
		}

		ParamWidget* amplitude1Param = createParam<RoundSmallFWKnob>(Vec(20, 167), module, LissajousLFO::AMPLITUDE1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(amplitude1Param)->percentage = &module->amplitude1Percentage;
		}
		addParam(amplitude1Param);							
		ParamWidget* freqX1Param = createParam<RoundSmallFWKnob>(Vec(49, 167), module, LissajousLFO::FREQX1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(freqX1Param)->percentage = &module->freqX1Percentage;
		}
		addParam(freqX1Param);							
		ParamWidget* freqY1Param = createParam<RoundSmallFWKnob>(Vec(78, 167), module, LissajousLFO::FREQY1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(freqY1Param)->percentage = &module->freqY1Percentage;
		}
		addParam(freqY1Param);							
		ParamWidget* phaseX1Param = createParam<RoundSmallFWKnob>(Vec(107, 167), module, LissajousLFO::PHASEX1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(phaseX1Param)->percentage = &module->phaseX1Percentage;
		}
		addParam(phaseX1Param);							
		ParamWidget* waveshapeX1Param = createParam<RoundSmallFWKnob>(Vec(136, 167), module, LissajousLFO::WAVESHAPEX1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(waveshapeX1Param)->percentage = &module->waveShapeX1Percentage;
		}
		addParam(waveshapeX1Param);							
		ParamWidget* waveShapeY1Param = createParam<RoundSmallFWKnob>(Vec(165, 167), module, LissajousLFO::WAVESHAPEY1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(waveShapeY1Param)->percentage = &module->waveShapeY1Percentage;
		}
		addParam(waveShapeY1Param);							
		ParamWidget* skewX1Param = createParam<RoundSmallFWKnob>(Vec(194, 167), module, LissajousLFO::SKEWX1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(skewX1Param)->percentage = &module->skewX1Percentage;
		}
		addParam(skewX1Param);							
		ParamWidget* skewY1Param = createParam<RoundSmallFWKnob>(Vec(223, 167), module, LissajousLFO::SKEWY1_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(skewY1Param)->percentage = &module->skewY1Percentage;
		}
		addParam(skewY1Param);							

		addParam(createParam<RoundReallySmallFWKnob>(Vec(22, 216), module, LissajousLFO::AMPLITUDE1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(51, 216), module, LissajousLFO::FREQX1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(80, 216), module, LissajousLFO::FREQY1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(109, 216), module, LissajousLFO::PHASEX1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(138, 216), module, LissajousLFO::WAVESHAPEX1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(167, 216), module, LissajousLFO::WAVESHAPEY1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(196, 216), module, LissajousLFO::SKEWX1_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(225, 216), module, LissajousLFO::SKEWY1_CV_ATTENUVERTER_PARAM));


		ParamWidget* amplitude2Param = createParam<RoundSmallFWKnob>(Vec(20, 256), module, LissajousLFO::AMPLITUDE2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(amplitude2Param)->percentage = &module->amplitude2Percentage;
		}
		addParam(amplitude2Param);							
		ParamWidget* freqX2Param = createParam<RoundSmallFWKnob>(Vec(49, 256), module, LissajousLFO::FREQX2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(freqX2Param)->percentage = &module->freqX2Percentage;
		}
		addParam(freqX2Param);							
		ParamWidget* freqY2Param = createParam<RoundSmallFWKnob>(Vec(78, 256), module, LissajousLFO::FREQY2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(freqY2Param)->percentage = &module->freqY2Percentage;
		}
		addParam(freqY2Param);							
		ParamWidget* phaseX2Param = createParam<RoundSmallFWKnob>(Vec(107, 256), module, LissajousLFO::PHASEX2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(phaseX2Param)->percentage = &module->phaseX2Percentage;
		}
		addParam(phaseX2Param);							
		ParamWidget* waveshapeX2Param = createParam<RoundSmallFWKnob>(Vec(136, 256), module, LissajousLFO::WAVESHAPEX2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(waveshapeX2Param)->percentage = &module->waveShapeX2Percentage;
		}
		addParam(waveshapeX2Param);							
		ParamWidget* waveshapeY2Param = createParam<RoundSmallFWKnob>(Vec(165, 256), module, LissajousLFO::WAVESHAPEY2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(waveshapeY2Param)->percentage = &module->waveShapeY2Percentage;
		}
		addParam(waveshapeY2Param);							
		ParamWidget* skewX2Param = createParam<RoundSmallFWKnob>(Vec(194, 256), module, LissajousLFO::SKEWX2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(skewX2Param)->percentage = &module->skewX2Percentage;
		}
		addParam(skewX2Param);							
		ParamWidget* skewY2Param = createParam<RoundSmallFWKnob>(Vec(223, 256), module, LissajousLFO::SKEWY2_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(skewY2Param)->percentage = &module->skewY2Percentage;
		}
		addParam(skewY2Param);							

		addParam(createParam<RoundReallySmallFWKnob>(Vec(22, 305), module, LissajousLFO::AMPLITUDE2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(51, 305), module, LissajousLFO::FREQX2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(80, 305), module, LissajousLFO::FREQY2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(109, 305), module, LissajousLFO::PHASEX2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(138, 305), module, LissajousLFO::WAVESHAPEX2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(167, 305), module, LissajousLFO::WAVESHAPEY2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(196, 305), module, LissajousLFO::SKEWX2_CV_ATTENUVERTER_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(225, 305), module, LissajousLFO::SKEWY2_CV_ATTENUVERTER_PARAM));


		addInput(createInput<FWPortInSmall>(Vec(23, 196), module, LissajousLFO::AMPLITUDE1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(52, 196), module, LissajousLFO::FREQX1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(81, 196), module, LissajousLFO::FREQY1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(110, 196), module, LissajousLFO::PHASEX1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(139, 196), module, LissajousLFO::WAVESHAPEX1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(168, 196), module, LissajousLFO::WAVESHAPEY1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(197, 196), module, LissajousLFO::SKEWX1_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(226, 196), module, LissajousLFO::SKEWY1_INPUT));
		
		addInput(createInput<FWPortInSmall>(Vec(23, 285), module, LissajousLFO::AMPLITUDE2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(52, 285), module, LissajousLFO::FREQX2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(81, 285), module, LissajousLFO::FREQY2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(110, 285), module, LissajousLFO::PHASEX2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(139, 285), module, LissajousLFO::WAVESHAPEX2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(168, 285), module, LissajousLFO::WAVESHAPEY2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(197, 285), module, LissajousLFO::SKEWX2_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(226, 285), module, LissajousLFO::SKEWY2_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(22, 343), module, LissajousLFO::OUTPUT_1));
		addOutput(createOutput<FWPortOutSmall>(Vec(51, 343), module, LissajousLFO::OUTPUT_2));
		addOutput(createOutput<FWPortOutSmall>(Vec(80, 343), module, LissajousLFO::OUTPUT_3));
		addOutput(createOutput<FWPortOutSmall>(Vec(109, 343), module, LissajousLFO::OUTPUT_4));
		addOutput(createOutput<FWPortOutSmall>(Vec(138, 343), module, LissajousLFO::OUTPUT_5));	
		addOutput(createOutput<FWPortOutSmall>(Vec(167, 343), module, LissajousLFO::OUTPUT_6));	
		addOutput(createOutput<FWPortOutSmall>(Vec(196, 343), module, LissajousLFO::OUTPUT_7));	
		addOutput(createOutput<FWPortOutSmall>(Vec(225, 343), module, LissajousLFO::OUTPUT_8));	
	}
};

Model *modelLissajousLFO = createModel<LissajousLFO, LissajousLFOWidget>("LissajousLFO");
