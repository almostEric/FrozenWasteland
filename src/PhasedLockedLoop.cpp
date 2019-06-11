/*
The filter DSP code has been derived from
Miller Puckette's code hosted at
https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
which is licensed for use under the following terms (MIT license):


Copyright (c) 2015, Miller Puckette. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

// The clipping function of a transistor pair is approximately tanh(x)
// TODO: Put this in a lookup table. 5th order approx doesn't seem to cut it
inline float clip(float x) {
	return tanhf(x);
}

template <int OVERSAMPLE, int QUALITY>
struct VoltageControlledOscillator {
	float phase = 0.0;
	float freq;
	float pw = 0.5;
	float pitch;

	dsp::Decimator<OVERSAMPLE, QUALITY> sqrDecimator;
	dsp::RCFilter sqrFilter;


	float sqrBuffer[OVERSAMPLE] = {};

	void setPitch(float pitchKnob, float pitchCv) {
		// Compute frequency
		pitch = pitchKnob;
		pitch = roundf(pitch);
		pitch += pitchCv;
		// Note C3
		freq = 261.626 * powf(2.0, pitch / 12.0);
	}
	void setPulseWidth(float pulseWidth) {
		const float pwMin = 0.01;
		pw = clamp(pulseWidth, pwMin, 1.0f - pwMin);
	}

	void process(float deltaTime) {

		// Advance phase
		float deltaPhase = clamp(freq * deltaTime, 1e-6f, 0.5f);

		
		sqrFilter.setCutoff(40.0 * deltaTime);

		for (int i = 0; i < OVERSAMPLE; i++) {

			sqrBuffer[i] = (phase < pw) ? 1.f : -1.f;

			// Advance phase
			phase += deltaPhase / OVERSAMPLE;
			phase = std::fabs(std::fmod(phase ,1.0f));			
		}
	}

	
	float sqr() {
		return sqrDecimator.process(sqrBuffer);
	}
	float light() {
		return sinf(2*M_PI * phase);
	}
};

struct PhaseComparator {
	bool clock = false;
	bool data = false;
	bool nandGate1 = false;
	bool nandGate2 = false;
	bool nandGate3 = false;
	bool nandGate4 = false;


	void setClock(float clockInput)  {
		clock = clockInput >= 0;
	}

	void setData(float dataInput)  {
		data = dataInput >= 0;
	}

	float XORoutput()  {
		return (clock ^ data) ? 5.0 : -5.0;
	}

	float FlipFlopOutput()  {
		bool invertedData = !data;
		nandGate1 = !(data && clock);
		nandGate2 = !(clock && invertedData);
		nandGate3 = !(nandGate1 && nandGate4);
		nandGate4 = !(nandGate3 && nandGate2);
		
		return nandGate3 ? 5.0 : -5.0;
	}
};




struct LadderFilter {
	float cutoff = 1000.0;
	float resonance = 0.0;
	float state[4] = {};

	void calculateDerivatives(float input, float *dstate, const float *state) {
		float cutoff2Pi = 2*M_PI * cutoff;

		float satstate0 = clip(state[0]);
		float satstate1 = clip(state[1]);
		float satstate2 = clip(state[2]);

		dstate[0] = cutoff2Pi * (clip(input - resonance * state[3]) - satstate0);
		dstate[1] = cutoff2Pi * (satstate0 - satstate1);
		dstate[2] = cutoff2Pi * (satstate1 - satstate2);
		dstate[3] = cutoff2Pi * (satstate2 - clip(state[3]));
	}

	void process(float input, float dt) {
		float deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

		calculateDerivatives(input, deriv1, state);
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5 * dt * deriv1[i];

		calculateDerivatives(input, deriv2, tempState);
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + 0.5 * dt * deriv2[i];

		calculateDerivatives(input, deriv3, tempState);
		for (int i = 0; i < 4; i++)
			tempState[i] = state[i] + dt * deriv3[i];

		calculateDerivatives(input, deriv4, tempState);
		for (int i = 0; i < 4; i++)
			state[i] += (1.0 / 6.0) * dt * (deriv1[i] + 2.0 * deriv2[i] + 2.0 * deriv3[i] + deriv4[i]);
	}
	void reset() {
		for (int i = 0; i < 4; i++) {
			state[i] = 0.0;
		}
	}
};


struct PhasedLockedLoop : Module {
	enum ParamIds {
		VCO_FREQ_PARAM,
		VCO_PW_PARAM,
		VCO_PWCV_PARAM,
		LPF_FREQ_PARAM,		
		COMPARATOR_TYPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VCO_CV_INPUT,
		VCO_PW_INPUT,
		PHASE_COMPARATOR_INPUT,
		SIGNAL_INPUT,
		LPF_FREQ_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SQUARE_OUTPUT,
		COMPARATOR_OUTPUT,
		LPF_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PHASE_LOCKED_LIGHT,
		XOR_COMPARATOR_LIGHT,
		FLIP_FLOP_COMPARATOR_LIGHT,
		NUM_LIGHTS
	};
	enum ComparatorTypes {
		XOR_COMPARATOR,
		FLIP_FLOP_COMARATOR
	};

	VoltageControlledOscillator<16,16> oscillator;
	PhaseComparator comparator;
	LadderFilter filter;

	dsp::SchmittTrigger modeTrigger;
	float filterOutput = 0;
	int currentComparatorType = XOR_COMPARATOR;

	
	PhasedLockedLoop() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(VCO_FREQ_PARAM, -54.0, 54.0, 0.0,"VCO Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(VCO_PW_PARAM, 0, 1, 0.5,"Pulse Width","%",0,100);
		configParam(VCO_PWCV_PARAM, 0, 1, 0,"Pulse Width CV","%",0,100);
		configParam(LPF_FREQ_PARAM, 0, 1, 0.5,"LPF Frequency"," Hz",540,15);
		configParam(COMPARATOR_TYPE_PARAM, 0.0, 1.0, 0.0);

	}
	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "comparatorType", json_integer((int) currentComparatorType));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "comparatorType");
		if (sumJ)
			currentComparatorType = json_integer_value(sumJ);

	}


	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void PhasedLockedLoop::process(const ProcessArgs &args) {
	// Modes
	if (modeTrigger.process(params[COMPARATOR_TYPE_PARAM].getValue())) {
		currentComparatorType = (currentComparatorType + 1) % 2; //only 2...for now!!!
	}
	lights[XOR_COMPARATOR_LIGHT].value = currentComparatorType == XOR_COMPARATOR ? 1.0 : 0.0;
	lights[FLIP_FLOP_COMPARATOR_LIGHT].value = currentComparatorType == FLIP_FLOP_COMARATOR ? 1.0 : 0.0;

	float pitchCv;
	if (inputs[VCO_CV_INPUT].isConnected()) {
		pitchCv =  12.0 * inputs[VCO_CV_INPUT].getVoltage();
	} else {
		pitchCv = 12.0 * filterOutput;
	}
	float pulseWidth = params[VCO_PW_PARAM].getValue();
	if(inputs[VCO_PW_INPUT].isConnected()) {
		pulseWidth += inputs[VCO_PW_INPUT].getVoltage() * (params[VCO_PWCV_PARAM].getValue() / 10.0);
	}
	//pitchCv = 0.0;// Test

	oscillator.setPitch(params[VCO_FREQ_PARAM].getValue(), pitchCv);
	oscillator.setPulseWidth(pulseWidth);

	oscillator.process(1.0 / args.sampleRate);


	float squareOutput = 5.0 * oscillator.sqr(); //Used a lot :)
	outputs[SQUARE_OUTPUT].setVoltage(squareOutput);

	//normally use internally genrated square wave, unless the input is being used
	float phaseComparatorData; //
	if(inputs[PHASE_COMPARATOR_INPUT].isConnected()) {
		phaseComparatorData = inputs[PHASE_COMPARATOR_INPUT].getVoltage();
	} else {
		phaseComparatorData = squareOutput;
	}
	comparator.setData(phaseComparatorData);

	//This is what we compare either the internal square wave, or alternate input too
	if(inputs[SIGNAL_INPUT].isConnected()) {
		comparator.setClock(inputs[SIGNAL_INPUT].getVoltage());
	}

	float comparatorOutput;
	switch (currentComparatorType) {
		case XOR_COMPARATOR :
			comparatorOutput = comparator.XORoutput();
			break;
		case FLIP_FLOP_COMARATOR :
			comparatorOutput = comparator.FlipFlopOutput();
			break;
		default:
			comparatorOutput = comparator.XORoutput();
			break;
	}
	outputs[COMPARATOR_OUTPUT].setVoltage(comparatorOutput);
	lights[PHASE_LOCKED_LIGHT].value = ((comparatorOutput >= 0.0  && phaseComparatorData >= 0.0) || (comparatorOutput < 0.0  && phaseComparatorData < 0.0));

	//feed comparator into the filter
	float filterInput = comparatorOutput / 5.0;

	
	// Set cutoff frequency
	float cutoffExp = params[LPF_FREQ_PARAM].getValue();
	if (inputs[LPF_FREQ_INPUT].isConnected()) {
		cutoffExp += (inputs[LPF_FREQ_INPUT].getVoltage() / 5);
	}
	cutoffExp = clamp(cutoffExp, 0.0f, 1.0f);
	const float minCutoff = 15.0;
	const float maxCutoff = 8400.0;
	filter.cutoff = minCutoff * powf(maxCutoff / minCutoff, cutoffExp);

	// Push a sample to the state filter
	filter.process(filterInput, 1.0/args.sampleRate);

	// Set outputs
	filterOutput = 5.0 * filter.state[3];
	outputs[LPF_OUTPUT].setVoltage(filterOutput);


}

struct PhasedLockedLoopWidget : ModuleWidget {
	PhasedLockedLoopWidget(PhasedLockedLoop *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PhasedLockedLoop.svg")));
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundSmallFWKnob>(Vec(100, 46), module, PhasedLockedLoop::VCO_FREQ_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(85, 79), module, PhasedLockedLoop::VCO_PW_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(118, 79), module, PhasedLockedLoop::VCO_PWCV_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(97, 307), module, PhasedLockedLoop::LPF_FREQ_PARAM));
		addParam(createParam<CKD6>(Vec(18, 202), module, PhasedLockedLoop::COMPARATOR_TYPE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(8, 30), module, PhasedLockedLoop::VCO_CV_INPUT));
		addInput(createInput<PJ301MPort>(Vec(8, 62), module, PhasedLockedLoop::VCO_PW_INPUT));
		addInput(createInput<PJ301MPort>(Vec(8, 135), module, PhasedLockedLoop::PHASE_COMPARATOR_INPUT));
		addInput(createInput<PJ301MPort>(Vec(8, 165), module, PhasedLockedLoop::SIGNAL_INPUT));
		addInput(createInput<PJ301MPort>(Vec(8, 289), module, PhasedLockedLoop::LPF_FREQ_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(8, 94), module, PhasedLockedLoop::SQUARE_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(8, 239), module, PhasedLockedLoop::COMPARATOR_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(8, 319), module, PhasedLockedLoop::LPF_OUTPUT));

		addChild(createLight<LargeLight<BlueLight>>(Vec(112, 155), module, PhasedLockedLoop::PHASE_LOCKED_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(62, 201), module, PhasedLockedLoop::XOR_COMPARATOR_LIGHT));
		addChild(createLight<SmallLight<BlueLight>>(Vec(62, 217), module, PhasedLockedLoop::FLIP_FLOP_COMPARATOR_LIGHT));
	}
};


Model *modelPhasedLockedLoop = createModel<PhasedLockedLoop, PhasedLockedLoopWidget>("PhasedLockedLoop");
