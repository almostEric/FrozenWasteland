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


struct RoundFWKnob : RoundKnob {
	RoundFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundFWKnob.svg")));
	}
};

struct RoundSmallFWKnob : RoundKnob {
	RoundSmallFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundSmallFWKnob.svg")));
	}
};

struct RoundLargeFWKnob : RoundKnob {
	RoundLargeFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundLargeFWKnob.svg")));
	}
};

struct RoundHugeFWKnob : RoundKnob {
	RoundHugeFWKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/RoundHugeFWKnob.svg")));
	}
};