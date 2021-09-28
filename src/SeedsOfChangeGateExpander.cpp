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



#define NBOUT 12


struct SeedsOfChangeGateExpander : Module {
	enum ParamIds {
		GATE_PROBABILITY_1_PARAM,
		GATE_PROBABILITY_1_CV_ATTENUVERTER = GATE_PROBABILITY_1_PARAM + NBOUT,
		GATE_MODE_PARAM = GATE_PROBABILITY_1_CV_ATTENUVERTER+ NBOUT,
		NUM_PARAMS = GATE_MODE_PARAM + NBOUT

	};
	enum InputIds {
		GATE_PROBABILITY_1_INPUT,
		NUM_INPUTS = GATE_PROBABILITY_1_INPUT + NBOUT
	};
	enum OutputIds {
		GATE_1_OUTPUT,
		NUM_OUTPUTS = GATE_1_OUTPUT + NBOUT
	};
	enum LightIds {
		GATE_MODE_LIGHT,
		NUM_LIGHTS = GATE_MODE_LIGHT + NBOUT
	};

	float outbuffer[NBOUT];

	// Expander
	float consumerMessage[4] = {};// this module must read from here
	float producerMessage[4] = {};// mother will write into here

	
	dsp::SchmittTrigger resetTrigger,clockTrigger,distributionModeTrigger,gateModeTrigger[NBOUT]; 
	dsp::PulseGenerator gatePulse[NBOUT];

	bool gaussianMode = false;

	//percentages
	float probabilityPercentage[NBOUT] = {0};


	SeedsOfChangeGateExpander() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i=0; i<NBOUT; i++) {
			configParam(SeedsOfChangeGateExpander::GATE_PROBABILITY_1_PARAM + i, 0.0, 1.0, 0.0,"Gate Probability","%",0,100);
			configParam(SeedsOfChangeGateExpander::GATE_PROBABILITY_1_CV_ATTENUVERTER + i, -1.0, 1.0, 0.0,"Gate Probability CV Attenuverter","%",0,100);
		}

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;

	}
	unsigned long mt[N]; /* the array for the state vector  */
	int mti=N+1; /* mti==N+1 means mt[N] is not initialized */
	void init_genrand(unsigned long s);
	unsigned long genrand_int32();
	double genrand_real();
	int latest_seed = 0;
	float resetInput, clockInput;
	float normal_number();
	bool gateMode[NBOUT] = {false};


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		for(int i=0;i<NBOUT;i++) {
			char buf[100];
			char notebuf[100];
			strcpy(buf, "gateMode-");
			sprintf(notebuf, "%i", i);
			strcat(buf, notebuf);
			json_object_set_new(rootJ, buf, json_integer((bool) gateMode[i]));
		}
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {
		for(int i=0;i<NBOUT;i++) {
			char buf[100];
			char notebuf[100];
			strcpy(buf, "gateMode-");
			sprintf(notebuf, "%i", i);
			strcat(buf, notebuf);
			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				gateMode[i] = json_integer_value(sumJ);
			}
		}		
	}


	void process(const ProcessArgs &args) override {
	
		bool motherPresent = (leftExpander.module && (leftExpander.module->model == modelSeedsOfChange || leftExpander.module->model == modelSeedsOfChangeCVExpander || leftExpander.module->model == modelSeedsOfChangeGateExpander));
		if (motherPresent) {
			// To Mother

			leftExpander.module->rightExpander.messageFlipRequested = true;

			// From Mother	
			float *messagesFromMother = (float*)leftExpander.consumerMessage;		
			latest_seed = messagesFromMother[0]; 
			clockInput = messagesFromMother[1]; 
			resetInput = messagesFromMother[2]; 
			gaussianMode = messagesFromMother[3]; 					
		}

		//Set Expander Info
		if(rightExpander.module && (rightExpander.module->model == modelSeedsOfChangeCVExpander || rightExpander.module->model == modelSeedsOfChangeGateExpander)) {	

			//Send outputs to slaves if present		
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			
			messageToExpander[0] = latest_seed; 
			messageToExpander[1] = clockInput;
			messageToExpander[2] = resetInput; 
			messageToExpander[3] = gaussianMode; 
			rightExpander.module->leftExpander.messageFlipRequested = true;						
		}	

        if (resetTrigger.process(resetInput) ) {
            init_genrand((unsigned long)(latest_seed));
        } 

		if (clockTrigger.process(clockInput)) {
			for (int i=0; i<NBOUT; i++) {
				float prob = clamp(params[GATE_PROBABILITY_1_PARAM + i].value + (inputs[GATE_PROBABILITY_1_INPUT + i].active ? inputs[GATE_PROBABILITY_1_INPUT + i].value / 10.0f * params[GATE_PROBABILITY_1_CV_ATTENUVERTER + i].value : 0.0),0.0f,1.0f);
				probabilityPercentage[i] = prob;
				outbuffer[i] = genrand_real() < prob ? 10.0 : 0;
				if(outbuffer[i]) {
					gatePulse[i].trigger();
				}
			}
		} 
		
		for (int i=0; i<NBOUT; i++) {
			if (gateModeTrigger[i].process(params[GATE_MODE_PARAM+i].getValue())) {
				gateMode[i] = !gateMode[i];
    	    } 
			lights[GATE_MODE_LIGHT+i].value = gateMode[i];

			float gateValue;
			if(gateMode[i]) { // True is trigger mode
				gateValue = gatePulse[i].process(1.0 / args.sampleRate) ? 10.0 : 0;
			} else {
				gateValue = outbuffer[i] ? clockInput : 0;;
			}

			outputs[GATE_1_OUTPUT+i].value = gateValue;
		}								
	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void SeedsOfChangeGateExpander::onReset() {
	for (int i=0; i<NBOUT; i++) {
		outbuffer[i] = 0;
	}
	resetTrigger.reset();
	clockTrigger.reset();
}

/* initializes mt[N] with a seed */
void SeedsOfChangeGateExpander::init_genrand(unsigned long s)
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
unsigned long SeedsOfChangeGateExpander::genrand_int32()
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
double SeedsOfChangeGateExpander::genrand_real()
{
    return genrand_int32()*(1.0/4294967296.0); 
    /* divided by 2^32 */
}
float SeedsOfChangeGateExpander::normal_number() {
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


struct SeedsOfChangeGateExpanderSeedDisplay : TransparentWidget {
	SeedsOfChangeGateExpander *module;
	int frame = 0;

	SeedsOfChangeGateExpanderSeedDisplay() {
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

	}
};

struct SeedsOfChangeGateExpanderWidget : ModuleWidget {
	SeedsOfChangeGateExpanderWidget(SeedsOfChangeGateExpander *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SeedsOfChangeGateExpander.svg")));


		{
			SeedsOfChangeGateExpanderSeedDisplay *display = new SeedsOfChangeGateExpanderSeedDisplay();
			display->module = module;
			display->box.pos = Vec(57, 46);
			display->box.size = Vec(box.size.x-31, 51);
			addChild(display);
		}

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		for (int i=0; i<NBOUT; i++) {
			ParamWidget* probabilityParam = createParam<RoundReallySmallFWKnob>(Vec(4, 44 + i*26.5), module, SeedsOfChangeGateExpander::GATE_PROBABILITY_1_PARAM + i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(probabilityParam)->percentage = &module->probabilityPercentage[i];
			}
			addParam(probabilityParam);							
			addParam(createParam<RoundExtremelySmallFWKnob>(Vec(42, 45 + i*26.5), module, SeedsOfChangeGateExpander::GATE_PROBABILITY_1_CV_ATTENUVERTER + i));
			addInput(createInput<FWPortInReallySmall>(Vec(28, 47 + i*26.5), module, SeedsOfChangeGateExpander::GATE_PROBABILITY_1_INPUT + i));				
			addParam(createParam<LEDButton>(Vec(59, 45 + i*26.5), module, SeedsOfChangeGateExpander::GATE_MODE_PARAM+i));
			addChild(createLight<LargeLight<BlueLight>>(Vec(60.5, 46.5 + i*26.5), module, SeedsOfChangeGateExpander::GATE_MODE_LIGHT+i));

			addOutput(createOutput<FWPortOutSmall>(Vec(79, 45 + i*26.5),  module, SeedsOfChangeGateExpander::GATE_1_OUTPUT + i));
		}
	}
};


// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelSeedsOfChangeGateExpander = createModel<SeedsOfChangeGateExpander, SeedsOfChangeGateExpanderWidget>("SeedsOfChangeGateExpander");
