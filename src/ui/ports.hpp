#include "../FrozenWasteland.hpp"


struct FWPortInSmall : SvgPort 
{
    FWPortInSmall() 
    {
        setSvg( APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/SmallInPort.svg")));   		   
    }
};