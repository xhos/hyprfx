#include "FxPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>

CFxPassElement::CFxPassElement(const CFxPassElement::SFxData& data_) : data(data_) {}

void CFxPassElement::draw(const CRegion& damage) {
    if (data.box.width <= 0 || data.box.height <= 0)
        return;

    auto* shader = data.shader;
    if (!shader || !shader->program)
        return;

    const auto PMONITOR = g_pHyprOpenGL->m_renderData.pMonitor.lock();
    if (!PMONITOR)
        return;

    // save GL state
    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    // projection
    CBox   monbox   = {0, 0, PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y};
    Mat3x3 matrix   = g_pHyprOpenGL->m_renderData.monitorProjection.projectBox(
        monbox, Math::wlTransformToHyprutils(Math::invertTransform(WL_OUTPUT_TRANSFORM_NORMAL)), monbox.rot);
    Mat3x3 glMatrix = g_pHyprOpenGL->m_renderData.projection.copy().multiply(matrix);

    g_pHyprOpenGL->blend(true);
    glUseProgram(shader->program);

    glMatrix.transpose();
    auto matArr = glMatrix.getMatrix();
    glUniformMatrix3fv(shader->proj, 1, GL_FALSE, matArr.data());

    // quad position in monitor-normalized coords
    float x1 = (data.box.x - PMONITOR->m_position.x) / PMONITOR->m_size.x;
    float y1 = (data.box.y - PMONITOR->m_position.y) / PMONITOR->m_size.y;
    float x2 = (data.box.x - PMONITOR->m_position.x + data.box.width) / PMONITOR->m_size.x;
    float y2 = (data.box.y - PMONITOR->m_position.y + data.box.height) / PMONITOR->m_size.y;

    glUniform4f(shader->quad, x1, y1, x2, y2);

    // niri-compatible uniforms
    glUniform1f(shader->niri_progress, data.progress);
    glUniform1f(shader->niri_clamped_progress, data.clampedProgress);
    glUniform1f(shader->niri_random_seed, data.randomSeed);
    glUniform2f(shader->niri_size, data.windowW, data.windowH);

    // niri_geo_to_tex: identity matrix (v_texcoord is already 0..1 geo coords, texture is 1:1)
    float identity[9] = {
        1.f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f,
    };
    glUniformMatrix3fv(shader->niri_geo_to_tex, 1, GL_FALSE, identity);

    // bind window snapshot texture
    if (data.snapshotTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, data.snapshotTex);
        glUniform1i(shader->niri_tex, 0);
    }

    float positions[] = {
        x1, y1, x2, y1, x1, y2, x2, y2,
    };

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, positions);
    glEnableVertexAttribArray(shader->posAttrib);

    for (auto& RECT : g_pHyprOpenGL->m_renderData.damage.getRects()) {
        g_pHyprOpenGL->scissor(&RECT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisableVertexAttribArray(shader->posAttrib);
    g_pHyprOpenGL->scissor(nullptr);

    glUseProgram(prevProgram);
}

bool CFxPassElement::needsLiveBlur() {
    return false;
}

bool CFxPassElement::needsPrecomputeBlur() {
    return false;
}
