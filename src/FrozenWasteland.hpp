#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

//struct EchoesThroughEternityWidget : ModuleWidget {
//	EchoesThroughEternityWidget();
//};

struct BPMLFOWidget : ModuleWidget {
	BPMLFOWidget();
};

struct DamonLilliardWidget : ModuleWidget {
	DamonLilliardWidget();
};

struct LissajousLFOWidget : ModuleWidget {
	LissajousLFOWidget();
};

struct MrBlueSkyWidget : ModuleWidget {
	MrBlueSkyWidget();
};

struct PhasedLockedLoopWidget : ModuleWidget {
	PhasedLockedLoopWidget();
};

struct QuadEuclideanRhythmWidget : ModuleWidget {
	QuadEuclideanRhythmWidget();
};

struct QuadGolombRulerRhythmWidget : ModuleWidget {
	QuadGolombRulerRhythmWidget();
};

struct QuantussyCellWidget : ModuleWidget {
	QuantussyCellWidget();
};

struct SeriouslySlowLFOWidget : ModuleWidget {
	SeriouslySlowLFOWidget();
};

//Can't believe I did this.
struct CDCSeriouslySlowLFOWidget : ModuleWidget {
	CDCSeriouslySlowLFOWidget();
};
