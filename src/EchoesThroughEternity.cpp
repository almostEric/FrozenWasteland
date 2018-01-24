#include "FrozenWasteland.hpp"
#include "dsp/samplerate.hpp"
#include "dsp/ringbuffer.hpp"
#include "dsp/filter.hpp"

#define FREQUENCY_BANDS (1<<2)
#define HISTORY_SIZE (1<<21)

struct EchoesThroughEternity : Module {
	enum ParamIds {
		MIX_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		DELAY_TIME_X,
		DELAY_TIME_Y,
		FEEDBACK_X,
		FEEDBACK_Y,
		MIX_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};

	DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer[FREQUENCY_BANDS];
	DoubleRingBuffer<float, 16> outBuffer[FREQUENCY_BANDS];
	SampleRateConverter<1> src;
	
	float lastWet[FREQUENCY_BANDS];
	float delayTimes[FREQUENCY_BANDS];
	float feedbackArray[FREQUENCY_BANDS];

	RCFilter lowpassFilter[FREQUENCY_BANDS];
	RCFilter highpassFilter[FREQUENCY_BANDS];


	EchoesThroughEternity() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
		float delayOffset = 0.1; //temporary!
		for(int frequencyIndex = 0;frequencyIndex < FREQUENCY_BANDS;frequencyIndex++)
		{
			lastWet[frequencyIndex] = 0.0;

			//float bandpassFreq =  powf(powf(2.0,1.0/102.0),frequencyIndex) * 20.0; // each octave split into 102 bands, based at 20hz
			float bandpassFreq =  powf(powf(2.0,2),frequencyIndex) * 20.0; // each octave split into 102 bands, based at 20hz
			lowpassFilter[frequencyIndex].setCutoff(bandpassFreq / engineGetSampleRate());
			highpassFilter[frequencyIndex].setCutoff(bandpassFreq / engineGetSampleRate());

			//NOTE: This is until UI can set delay time & feedback
			delayTimes[frequencyIndex] = 0.2 + delayOffset;
			feedbackArray[frequencyIndex] = 0.7;

			delayOffset = delayOffset + 0.1;
		}
	}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void EchoesThroughEternity::step() {
	// Get input to delay block
	float in = inputs[IN_INPUT].value;	
	float totalWet = 0.0;

	//Cycle through all frequencies
	for(int frequencyIndex = 0;frequencyIndex < FREQUENCY_BANDS;frequencyIndex++)
	{
		//TODO Apply bandpass filter to input
		lowpassFilter[frequencyIndex].process(in);
		float filteredIn = lowpassFilter[frequencyIndex].lowpass();
		highpassFilter[frequencyIndex].process(filteredIn);
		filteredIn = highpassFilter[frequencyIndex].highpass();


		float feedback = clampf(feedbackArray[frequencyIndex], 0.0, 1.0);
		float dry = filteredIn + lastWet[frequencyIndex] * feedback;

		// Compute delay time in seconds
		float delay = 1e-3 * powf(10.0 / 1e-3, clampf(delayTimes[frequencyIndex], 0.0, 1.0));
		// Number of delay samples
		float index = delay * engineGetSampleRate();

		// TODO This is a horrible digital delay algorithm. Rewrite later.

		// Push dry sample into history buffer
		if (!historyBuffer[frequencyIndex].full()) {
			historyBuffer[frequencyIndex].push(dry);
		}

		// How many samples do we need consume to catch up?
		float consume = index - historyBuffer[frequencyIndex].size();
		// printf("%f\t%d\t%f\n", index, historyBuffer.size(), consume);

		// printf("wanted: %f\tactual: %d\tdiff: %d\tratio: %f\n", index, historyBuffer.size(), consume, index / historyBuffer.size());
		if (outBuffer[frequencyIndex].empty()) {
			// Idk wtf I'm doing
			double ratio = 1.0;
			if (consume <= -16)
				ratio = 0.5;
			else if (consume >= 16)
				ratio = 2.0;

			// printf("%f\t%lf\n", consume, ratio);
			int inFrames = mini(historyBuffer[frequencyIndex].size(), 16);
			int outFrames = outBuffer[frequencyIndex].capacity();
			// printf(">\t%d\t%d\n", inFrames, outFrames);
			src.setRatioSmooth(ratio);
			src.process((const Frame<1>*)historyBuffer[frequencyIndex].startData(), &inFrames, (Frame<1>*)outBuffer[frequencyIndex].endData(), &outFrames);
			historyBuffer[frequencyIndex].startIncr(inFrames);
			outBuffer[frequencyIndex].endIncr(outFrames);
			// printf("<\t%d\t%d\n", inFrames, outFrames);
			// printf("====================================\n");
		}

		float wet = 0.0;
		if (!outBuffer[frequencyIndex].empty()) {
			wet = outBuffer[frequencyIndex].shift();
		}
		
		lastWet[frequencyIndex] = wet;
		totalWet = totalWet + wet;
	}
	float mix = clampf(params[MIX_PARAM].value + inputs[MIX_INPUT].value / 10.0, 0.0, 1.0);
	float out = crossf(in, totalWet, mix);
	outputs[OUT_OUTPUT].value = out;
}


EchoesThroughEternityWidget::EchoesThroughEternityWidget() {
	EchoesThroughEternity *module = new EchoesThroughEternity();
	setModule(module);
	box.size = Vec(12 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/EchoesThroughEternity.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParam<RoundBlackKnob>(Vec(67, 257), module, EchoesThroughEternity::MIX_PARAM, 0.0, 1.0, 0.5));

//NOTE: only IN_INPUTS cooridinates have been determined
	addInput(createInput<PJ301MPort>(Vec(53, 186), module, EchoesThroughEternity::DELAY_TIME_X));
	addInput(createInput<PJ301MPort>(Vec(53, 186), module, EchoesThroughEternity::DELAY_TIME_Y));
	addInput(createInput<PJ301MPort>(Vec(53, 186), module, EchoesThroughEternity::FEEDBACK_X));
	addInput(createInput<PJ301MPort>(Vec(53, 186), module, EchoesThroughEternity::FEEDBACK_Y));
	addInput(createInput<PJ301MPort>(Vec(53, 186), module, EchoesThroughEternity::MIX_INPUT));
	addInput(createInput<PJ301MPort>(Vec(33, 186), module, EchoesThroughEternity::IN_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(33, 275), module, EchoesThroughEternity::OUT_OUTPUT));
	
}
