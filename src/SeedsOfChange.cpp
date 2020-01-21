#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
//#include <dsp/digital.hpp>

#include <sstream>
#include <iomanip>

#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */



#define NBOUT 4


struct SeedsOfChange : Module {
	enum ParamIds {
		SEED_PARAM,
		RESEED_PARAM,
		DISTRIBUTION_PARAM,
		MULTIPLY_1_PARAM,
		OFFSET_1_PARAM = MULTIPLY_1_PARAM + NBOUT,
		GATE_PROBABILITY_1_PARAM = OFFSET_1_PARAM + NBOUT,
		MULTIPLY_1_CV_ATTENUVERTER = GATE_PROBABILITY_1_PARAM + NBOUT,
		OFFSET_1_CV_ATTENUVERTER = MULTIPLY_1_CV_ATTENUVERTER + NBOUT,
		GATE_PROBABILITY_1_CV_ATTENUVERTER = OFFSET_1_CV_ATTENUVERTER + NBOUT,
		NUM_PARAMS = GATE_PROBABILITY_1_CV_ATTENUVERTER + NBOUT
	};
	enum InputIds {
		SEED_INPUT,
		RESEED_INPUT,
		CLOCK_INPUT,
		DISTRIBUTION_INPUT,
		MULTIPLY_1_INPUT, 
		OFFSET_1_INPUT = MULTIPLY_1_INPUT + NBOUT, 
		GATE_PROBABILITY_1_INPUT = OFFSET_1_INPUT + NBOUT,
		NUM_INPUTS = GATE_PROBABILITY_1_INPUT + NBOUT
	};
	enum OutputIds {
		CV_1_OUTPUT,
		GATE_1_OUTPUT = CV_1_OUTPUT + NBOUT,
		NUM_OUTPUTS = GATE_1_OUTPUT + NBOUT
	};
	enum LightIds {
		DISTRIBUTION_GAUSSIAN_LIGHT,
		SEED_LOADED_LIGHT,
		NUM_LIGHTS = SEED_LOADED_LIGHT + 2,
	};


	float outbuffer[NBOUT * 2];


	// Expander
	float consumerMessage[4] = {};// this module must read from here
	float producerMessage[4] = {};// mother will write into here
	int seed;
	float clockValue,reseedValue,distributionValue;


	dsp::SchmittTrigger reseedTrigger,clockTrigger,distributionModeTrigger; 

	bool gaussianMode = false;

	SeedsOfChange() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SeedsOfChange::SEED_PARAM, 0.0, 9999.0, 0.0,"Seed");
		configParam(SeedsOfChange::RESEED_PARAM, 0.0, 1.0, 0.0);
		configParam(SeedsOfChange::DISTRIBUTION_PARAM, 0.0, 1.0, 0.0,"Distribution");
		for (int i=0; i<NBOUT; i++) {
			configParam(SeedsOfChange::MULTIPLY_1_PARAM + i, 0.0f, 10.0f, 10.0f, "Multiply");			
			configParam(SeedsOfChange::OFFSET_1_PARAM + i, -10.0f, 10.0f, 0.0f,"Offset");			
			configParam(SeedsOfChange::MULTIPLY_1_CV_ATTENUVERTER + i, -1.0, 1.0, 0.0,"Multiply CV Attenuverter","%",0,100);
			configParam(SeedsOfChange::OFFSET_1_CV_ATTENUVERTER + i, -1.0, 1.0, 0.0,"Offset CV Attenuverter","%",0,100);
			configParam(SeedsOfChange::GATE_PROBABILITY_1_PARAM + i, 0.0, 1.0, 0.0,"Gate Probability","%",0,100);
			configParam(SeedsOfChange::GATE_PROBABILITY_1_CV_ATTENUVERTER + i, -1.0, 1.0, 0.0,"Gate Probability CV Attenuverter","%",0,100);
		}

		rightExpander.producerMessage = producerMessage;
		rightExpander.consumerMessage = consumerMessage;

	}
	unsigned long mt[N]; /* the array for the state vector  */
	int mti=N+1; /* mti==N+1 means mt[N] is not initialized */
	void init_genrand(unsigned long s);
	unsigned long genrand_int32();
	double genrand_real();
	int latest_seed = 0;
	float normal_number();

	void process(const ProcessArgs &args) override {
	
		if (distributionModeTrigger.process(params[DISTRIBUTION_PARAM].getValue())) {
			gaussianMode = !gaussianMode;
		}

		if (inputs[DISTRIBUTION_INPUT].isConnected()) {
			gaussianMode = inputs[DISTRIBUTION_INPUT].getVoltage()  > 5.0;  
		}
		
		lights[DISTRIBUTION_GAUSSIAN_LIGHT].value = gaussianMode;
		
        float reseedInput = inputs[RESEED_INPUT].getVoltage();
		reseedInput += params[RESEED_PARAM].getValue(); 		

		seed = inputs[SEED_INPUT].isConnected() ? inputs[SEED_INPUT].getVoltage()*999.9 : params[SEED_PARAM].getValue();
        if (reseedTrigger.process(reseedInput) ) {
			init_genrand((unsigned long)(seed));
        } 
		lights[SEED_LOADED_LIGHT].value = seed == latest_seed;
		lights[SEED_LOADED_LIGHT+1].value = seed != latest_seed;

		if( inputs[CLOCK_INPUT].active ) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].value) ) {
				for (int i=0; i<NBOUT; i++) {
					float mult=params[MULTIPLY_1_PARAM+i].value;
					float off=params[OFFSET_1_PARAM+i].value;
					if (inputs[MULTIPLY_1_INPUT + i].active) {
						mult = mult + (inputs[MULTIPLY_1_INPUT + i].value / 10.0f * params[MULTIPLY_1_CV_ATTENUVERTER + i].value);
					}
					mult = clamp(mult,0.0,10.0);
					if (inputs[OFFSET_1_INPUT + i].active) {
						off = off + (inputs[OFFSET_1_INPUT + i].value * params[OFFSET_1_CV_ATTENUVERTER + i].value);
					}

					float initialRandomNumber = gaussianMode ? normal_number() : genrand_real();					
					
					outbuffer[i] = clamp((float)(initialRandomNumber * mult + off),-10.0f, 10.0f);

					

					float prob = clamp(params[GATE_PROBABILITY_1_PARAM + i].value + (inputs[GATE_PROBABILITY_1_INPUT + i].active ? inputs[GATE_PROBABILITY_1_INPUT + i].value / 10.0f * params[GATE_PROBABILITY_1_CV_ATTENUVERTER + i].value : 0.0),0.0f,1.0f);
					
					outbuffer[i+NBOUT] = genrand_real() < prob ? 10.0 : 0;
				}
			} 
		}

		for (int i=0; i<NBOUT; i++) {
			outputs[CV_1_OUTPUT+i].value = outbuffer[i];
			outputs[GATE_1_OUTPUT+i].value = outbuffer[i+NBOUT] ? inputs[CLOCK_INPUT].value : 0;
		}	

		//Set Expander Info
		if(rightExpander.module && (rightExpander.module->model == modelSeedsOfChangeCVExpander || rightExpander.module->model == modelSeedsOfChangeGateExpander)) {	

			//Send outputs to slaves if present		
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			
			messageToExpander[0] = latest_seed; 
			messageToExpander[1] = inputs[CLOCK_INPUT].getVoltage(); 
			messageToExpander[2] = reseedInput; 
			messageToExpander[3] = gaussianMode; 
			rightExpander.module->leftExpander.messageFlipRequested = true;						
		}					
	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void SeedsOfChange::onReset() {
	for (int i=0; i<NBOUT; i++) {
		outbuffer[i] = 0;
		outbuffer[i+NBOUT] = 0;
	}
	reseedTrigger.reset();
	clockTrigger.reset();
}

/* initializes mt[N] with a seed */
void SeedsOfChange::init_genrand(unsigned long s)
{
	
	s = s>9999?9999:s;
	latest_seed = s;
    mt[0]= s & 0xffffffffUL;
    for (mti=1; mti<N; mti++) {
        mt[mti] = 
	    (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        mt[mti] &= 0xffffffffUL;
        /* for >32 bit machines */
    }
}
/* generates a random number on [0,0xffffffff]-interval */
unsigned long SeedsOfChange::genrand_int32()
{
    unsigned long y;
    static unsigned long mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    if (mti >= N) { /* generate N words at one time */
        int kk;

        if (mti == N+1)   /* if init_genrand() has not been called, */
            init_genrand(5489UL); /* a default initial seed is used */

        for (kk=0;kk<N-M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }
  
    y = mt[mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}
/* generates a random number on [0,1)-real-interval */
double SeedsOfChange::genrand_real()
{
    return genrand_int32()*(1.0/4294967296.0); 
    /* divided by 2^32 */
}
float SeedsOfChange::normal_number() {
	const double x1 = .68;
	const double x2 = .92;
	double x = genrand_real();
	double y = genrand_real();
	double s = (y<.5) ? .5 : -.5;
	if (x<x1) {
		return y*.333*s + .5;
	} else if (x<x2) {
		return (y*.333+.333)*s + .5;		
	} else {
		return (y*.333+.666)*s + .5;
	}
}


struct SeedsOfChangeSeedDisplay : TransparentWidget {
	SeedsOfChange *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	SeedsOfChangeSeedDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

	void drawSeed(const DrawArgs &args, Vec pos, int seed) {
		nvgFontSize(args.vg, 14);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " %i", seed);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

		drawSeed(args, Vec(0, 0), module->latest_seed);
	}
};

struct SeedsOfChangeWidget : ModuleWidget {
	SeedsOfChangeWidget(SeedsOfChange *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SeedsOfChange.svg")));


		{
			SeedsOfChangeSeedDisplay *display = new SeedsOfChangeSeedDisplay();
			display->module = module;
			display->box.pos = Vec(96, 44);
			display->box.size = Vec(box.size.x-31, 51);
			addChild(display);
		}

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));





        addParam(createParam<RoundReallySmallFWKnob>(Vec(28,31), module, SeedsOfChange::SEED_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(4, 33), module, SeedsOfChange::SEED_INPUT));
		addChild(createLight<LargeLight<GreenRedLight>>(Vec(100, 33), module, SeedsOfChange::SEED_LOADED_LIGHT));

		addInput(createInput<FWPortInSmall>(Vec(4, 63), module, SeedsOfChange::CLOCK_INPUT));

        addParam(createParam<TL1105>(Vec(100, 94), module, SeedsOfChange::RESEED_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(80, 93), module, SeedsOfChange::RESEED_INPUT));

		addParam(createParam<LEDButton>(Vec(25, 92), module, SeedsOfChange::DISTRIBUTION_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(26.5, 93.5), module, SeedsOfChange::DISTRIBUTION_GAUSSIAN_LIGHT));
		addInput(createInput<FWPortInSmall>(Vec(4, 93), module, SeedsOfChange::DISTRIBUTION_INPUT));


		for (int i=0; i<NBOUT; i++) {
			addParam(createParam<RoundReallySmallFWKnob>(Vec(4,125 + i * 32), module, SeedsOfChange::MULTIPLY_1_PARAM + i));			
			addParam(createParam<RoundExtremelySmallFWKnob>(Vec(27, 140 + i*32), module, SeedsOfChange::MULTIPLY_1_CV_ATTENUVERTER + i));
			addInput(createInput<FWPortInReallySmall>(Vec(28, 126 + i * 32), module, SeedsOfChange::MULTIPLY_1_INPUT + i));						
			addParam(createParam<RoundReallySmallFWKnob>(Vec(50,125 + i * 32), module, SeedsOfChange::OFFSET_1_PARAM + i));			
			addParam(createParam<RoundExtremelySmallFWKnob>(Vec(72, 140 + i*32), module, SeedsOfChange::OFFSET_1_CV_ATTENUVERTER + i));
			addInput(createInput<FWPortInReallySmall>(Vec(73, 126 + i * 32), module, SeedsOfChange::OFFSET_1_INPUT + i));
			addOutput(createOutput<FWPortInSmall>(Vec(97, 126 + i * 32),  module, SeedsOfChange::CV_1_OUTPUT+i));

			addParam(createParam<RoundReallySmallFWKnob>(Vec(4, 264 + i*24), module, SeedsOfChange::GATE_PROBABILITY_1_PARAM + i));
			addInput(createInput<FWPortInReallySmall>(Vec(30, 268 + i*24), module, SeedsOfChange::GATE_PROBABILITY_1_INPUT + i));			
			addParam(createParam<RoundExtremelySmallFWKnob>(Vec(48, 266 + i*24), module, SeedsOfChange::GATE_PROBABILITY_1_CV_ATTENUVERTER + i));
			addOutput(createOutput<FWPortOutSmall>(Vec(97, 265 + i*24),  module, SeedsOfChange::GATE_1_OUTPUT + i));
		}
	}
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelSeedsOfChange = createModel<SeedsOfChange, SeedsOfChangeWidget>("SeedsOfChange");
