#include "FxPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "globals.hpp"

CFxPassElement::CFxPassElement(const CFxPassElement::SFxData& data_) : data(data_) {
    ;
}

void CFxPassElement::draw(const CRegion& damage) {
    if (data.box.width <= 0 || data.box.height <= 0)
        return;

    g_pHyprOpenGL->renderRect(data.box, CHyprColor{1.0, 0.0, 0.5, data.alpha}, {});
}

bool CFxPassElement::needsLiveBlur() {
    return false;
}

bool CFxPassElement::needsPrecomputeBlur() {
    return false;
}
