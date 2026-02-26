#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

class CFxPassElement : public IPassElement {
  public:
    struct SFxData {
        CBox  box;   // where to render (monitor-local coords)
        float alpha = 1.0f;
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
