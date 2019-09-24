#include "../FrozenWasteland.hpp"


struct FWPortInSmall : SvgPort 
{
    FWPortInSmall() 
    {
        setSvg( APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/SmallInPort.svg")));   		   
    }
};

struct FWPortInReallySmall : SvgPort 
{
    FWPortInReallySmall() 
    {
        setSvg( APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/ReallySmallInPort.svg")));   		   
    }
};


struct FWPortOutSmall : SvgPort 
{
    FWPortOutSmall() 
    {
        setSvg( APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/SmallInPort.svg"))); //Use different svg in future	   
    }
};