#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

class CFxPassElement : public IPassElement {
  public:
    struct SFxData {
        CBox   box; // global coords
        float  progress    = 0;
        GLuint snapshotTex = 0;
        float  rounding    = 0;
        GLuint shardTex    = 0;
        float  actorScale  = 1.0f; // 1.0 = no expansion, 2.0 = 2x quad for broken glass
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
