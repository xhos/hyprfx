#include "FxPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include "globals.hpp"

CFxPassElement::CFxPassElement(const CFxPassElement::SFxData& data_) : data(data_) {}

void CFxPassElement::draw(const CRegion& damage) {
    if (data.box.width <= 0 || data.box.height <= 0)
        return;

    auto& shader = g_pGlobalState->shader;
    if (!shader.program)
        return;

    const auto PMONITOR = g_pHyprOpenGL->m_renderData.pMonitor.lock();
    if (!PMONITOR)
        return;

    // save GL state we're about to change
    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    // projection
    CBox   monbox   = {0, 0, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.x, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.y};
    Mat3x3 matrix   = g_pHyprOpenGL->m_renderData.monitorProjection.projectBox(monbox, Math::wlTransformToHyprutils(Math::invertTransform(WL_OUTPUT_TRANSFORM_NORMAL)), monbox.rot);
    Mat3x3 glMatrix = g_pHyprOpenGL->m_renderData.projection.copy().multiply(matrix);

    g_pHyprOpenGL->blend(true);
    glUseProgram(shader.program);

    glMatrix.transpose();
    g_pGlobalState->shader.setUniformMatrix3fv(SHADER_PROJ, 1, GL_FALSE, glMatrix.getMatrix());

    float t     = data.progress;
    float alpha = (1.0f - t) * (1.0f - t);
    glUniform4f(shader.uniformLocations[SHADER_COLOR], 1.0f * alpha, 0.0f, 0.5f * alpha, alpha);

    float x1 = (data.box.x - PMONITOR->m_position.x) / PMONITOR->m_size.x;
    float y1 = (data.box.y - PMONITOR->m_position.y) / PMONITOR->m_size.y;
    float x2 = (data.box.x - PMONITOR->m_position.x + data.box.width) / PMONITOR->m_size.x;
    float y2 = (data.box.y - PMONITOR->m_position.y + data.box.height) / PMONITOR->m_size.y;

    float positions[] = {
        x1, y1, x2, y1, x1, y2, x2, y2,
    };

    glVertexAttribPointer(shader.uniformLocations[SHADER_POS_ATTRIB], 2, GL_FLOAT, GL_FALSE, 0, positions);
    glEnableVertexAttribArray(shader.uniformLocations[SHADER_POS_ATTRIB]);

    for (auto& RECT : g_pHyprOpenGL->m_renderData.damage.getRects()) {
        g_pHyprOpenGL->scissor(&RECT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisableVertexAttribArray(shader.uniformLocations[SHADER_POS_ATTRIB]);
    g_pHyprOpenGL->scissor(nullptr);

    glUseProgram(prevProgram);
}

bool CFxPassElement::needsLiveBlur() {
    return false;
}

bool CFxPassElement::needsPrecomputeBlur() {
    return false;
}
