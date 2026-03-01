#pragma once

#include <string>

// generated at build time from shaders/ and textures/
#include "shader_data.inc"

inline const std::string VERT(VERT_DATA, VERT_DATA + VERT_DATA_LEN);
inline const std::string FRAG(FRAG_DATA, FRAG_DATA + FRAG_DATA_LEN);
inline const std::string FRAG_BROKEN_GLASS(FRAG_BROKEN_GLASS_DATA, FRAG_BROKEN_GLASS_DATA + FRAG_BROKEN_GLASS_DATA_LEN);
inline const std::string FRAG_BLUR(FRAG_BLUR_DATA, FRAG_BLUR_DATA + FRAG_BLUR_DATA_LEN);
inline const std::string FRAG_GLIDE(FRAG_GLIDE_DATA, FRAG_GLIDE_DATA + FRAG_GLIDE_DATA_LEN);
