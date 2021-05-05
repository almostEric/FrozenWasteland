#include "../FrozenWasteland.hpp"

static const NVGcolor SCHEME_FW_TRANSPARENT = nvgRGBA(0x00, 0x00, 0x00, 0x00);
static const NVGcolor SCHEME_BLACK = nvgRGB(0x00, 0x00, 0x00);
static const NVGcolor SCHEME_WHITE = nvgRGB(0xff, 0xff, 0xff);
static const NVGcolor SCHEME_RED = nvgRGB(0xed, 0x2c, 0x24);
static const NVGcolor SCHEME_ORANGE = nvgRGB(0xf2, 0xb1, 0x20);
static const NVGcolor SCHEME_YELLOW = nvgRGB(0xf9, 0xdf, 0x1c);
static const NVGcolor SCHEME_GREEN = nvgRGB(0x90, 0xc7, 0x3e);
static const NVGcolor SCHEME_CYAN = nvgRGB(0x22, 0xe6, 0xef);
static const NVGcolor SCHEME_BLUE = nvgRGB(0x29, 0xb2, 0xef);
static const NVGcolor SCHEME_PURPLE = nvgRGB(0xd5, 0x2b, 0xed);
static const NVGcolor SCHEME_LIGHT_GRAY = nvgRGB(0xe6, 0xe6, 0xe6);
static const NVGcolor SCHEME_DARK_GRAY = nvgRGB(0x17, 0x17, 0x17);

////////////////////
// Knobs
////////////////////

struct BaseKnob : app::SvgKnob {

float *percentage = nullptr;
bool biDirectional = false;

public:
  BaseKnob() {
		// minAngle = -0.83*M_PI;
		// maxAngle = 0.83*M_PI;
		minAngle = -0.68*M_PI;
	    maxAngle = 0.68*M_PI;

	}

  void setSVG(std::shared_ptr<Svg> svg) {
    app::SvgKnob::setSvg(svg);
  }

  void draw(const DrawArgs &args) override {
    // /** shadow */
    // shadow.draw(args.vg);

    /** component */
    ParamWidget::draw(args);

	NVGcolor icol = nvgRGBAf(0.84f, 0.84f, 0.84f, 0.5);
    NVGcolor ocol = nvgRGBAf(0.1f, 0.1f, 0.1f, 0.5);
    NVGpaint paint = nvgLinearGradient(args.vg, 0, 0, box.size.x, box.size.y, icol, ocol);
    nvgBeginPath(args.vg);
    nvgFillPaint(args.vg, paint);
    nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, box.size.x / 2);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

	if (percentage == nullptr) {
      return;
    }

    float minAngle = -3.65;
    float maxAngle =  0.55;
	float midAngle = (minAngle+maxAngle) / 2.0;
    nvgBeginPath(args.vg);
    nvgStrokeColor(args.vg, nvgRGBA(0xc8, 0xa1, 0x29, 0xff));
    nvgStrokeWidth(args.vg, 2.0);
	if(!biDirectional) {
		float endAngle = ((maxAngle - minAngle) * *percentage) + minAngle;
		nvgArc(args.vg, box.size.x / 2, box.size.y / 2, box.size.x / 2 - 4.0, minAngle, endAngle, NVG_CW);
	} else {
		float endAngle = ((maxAngle - midAngle) * *percentage) + midAngle;
		nvgArc(args.vg, box.size.x / 2, box.size.y / 2, box.size.x / 2 - 4.0, midAngle, endAngle, endAngle > midAngle ? NVG_CW : NVG_CCW);
	}
    nvgStroke(args.vg);
  }
};

// struct LightKnob : BaseKnob {
//   LightKnob() {
//     minAngle = -0.68*M_PI;
//     maxAngle = 0.68*M_PI;

//     setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/component/knob.svg")));
//     shadow.setBox(box);
//     shadow.setSize(0.8);
//     shadow.setStrength(0.2);
//     shadow.setShadowPosition(2, 3.5);
//   }
// };

struct RoundFWKnob : BaseKnob {
	RoundFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundFWKnob.svg")));
	}
};

struct RoundFWSnapKnob : BaseKnob {
	RoundFWSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundFWKnob.svg")));
		snap = true;
	}
};


struct RoundSmallFWKnob : BaseKnob {
	RoundSmallFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundSmallFWKnob.svg")));
	}
};

struct RoundSmallFWSnapKnob : BaseKnob {
	RoundSmallFWSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundSmallFWKnob.svg")));
		snap = true;
	}
};

struct RoundReallySmallFWKnob : BaseKnob {
	RoundReallySmallFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundReallySmallFWKnob.svg")));
	}
};

struct RoundReallySmallFWSnapKnob : BaseKnob {
	RoundReallySmallFWSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundReallySmallFWKnob.svg")));
		snap = true;
	}
};

struct RoundExtremelySmallFWKnob : BaseKnob {
	RoundExtremelySmallFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundExtremelySmallFWKnob.svg")));
	}
};




struct RoundLargeFWKnob : BaseKnob {
	RoundLargeFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundLargeFWKnob.svg")));
	}
};

struct RoundLargeFWSnapKnob : BaseKnob {
	RoundLargeFWSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundLargeFWKnob.svg")));
		snap = true;
	}
};



struct RoundHugeFWKnob : BaseKnob {
	RoundHugeFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundHugeFWKnob.svg")));
	}
};

struct RoundHugeFWSnapKnob : BaseKnob {
	RoundHugeFWSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundHugeFWKnob.svg")));
		snap = true;
	}
};


////////////////////
// OK, Switches too
////////////////////


struct HCKSS : SvgSwitch {
	HCKSS() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/HCKSS_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/HCKSS_1.svg")));
	}
};
