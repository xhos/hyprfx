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

    // compute expanded box for effects that render beyond window bounds
    float scale  = data.actorScale;
    float extraW = data.box.width * (scale - 1.0f) * 0.5f;
    float extraH = data.box.height * (scale - 1.0f) * 0.5f;
    CBox  renderBox = {data.box.x - extraW, data.box.y - extraH, data.box.width * scale, data.box.height * scale};

    float x1 = (renderBox.x - PMONITOR->m_position.x) / PMONITOR->m_size.x;
    float y1 = (renderBox.y - PMONITOR->m_position.y) / PMONITOR->m_size.y;
    float x2 = (renderBox.x - PMONITOR->m_position.x + renderBox.width) / PMONITOR->m_size.x;
    float y2 = (renderBox.y - PMONITOR->m_position.y + renderBox.height) / PMONITOR->m_size.y;

    // quad bounds for UV computation in vertex shader
    glUniform4f(shader.uniformLocations[SHADER_GRADIENT], x1, y1, x2, y2);
    glUniform1f(shader.uniformLocations[SHADER_TIME], data.progress);
    glUniform2f(shader.uniformLocations[SHADER_FULL_SIZE], data.box.width, data.box.height);
    glUniform1f(shader.uniformLocations[SHADER_RADIUS], data.rounding);

    // bind window snapshot texture
    if (data.snapshotTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, data.snapshotTex);
        glUniform1i(shader.uniformLocations[SHADER_TEX], 0);
    }

    // bind shard texture (broken glass effect)
    if (data.shardTex) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, data.shardTex);
        GLint loc = glGetUniformLocation(shader.program, "shardTex");
        if (loc >= 0)
            glUniform1i(loc, 1);

        // set broken glass specific uniforms
        GLint seedLoc = glGetUniformLocation(shader.program, "seed");
        if (seedLoc >= 0)
            glUniform2f(seedLoc, 3.17f, 7.23f);

        GLint epicenterLoc = glGetUniformLocation(shader.program, "epicenter");
        if (epicenterLoc >= 0)
            glUniform2f(epicenterLoc, 0.5f, 0.5f);

        GLint shardScaleLoc = glGetUniformLocation(shader.program, "shardScale");
        if (shardScaleLoc >= 0)
            glUniform1f(shardScaleLoc, 1.5f);

        GLint blowForceLoc = glGetUniformLocation(shader.program, "blowForce");
        if (blowForceLoc >= 0)
            glUniform1f(blowForceLoc, 1.5f);

        GLint gravityLoc = glGetUniformLocation(shader.program, "gravity");
        if (gravityLoc >= 0)
            glUniform1f(gravityLoc, 3.0f);

        glActiveTexture(GL_TEXTURE0);
    }

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
