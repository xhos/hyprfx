#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>
#include "globals.hpp"

class CFxPassElement : public IPassElement {
  public:
    struct SFxData {
        CBox             box;
        float            progress        = 0;
        float            clampedProgress = 0;
        GLuint           snapshotTex     = 0;
        float            randomSeed      = 0;
        float            windowW         = 0;
        float            windowH         = 0;
        SCompiledShader* shader          = nullptr;
    };

    CFxPassElement(const SFxData& data_);
    virtual ~CFxPassElement() = default;

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CFxPassElement";
    }

  private:
    SFxData data;
};
