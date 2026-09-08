#include "ui/GLFullScreenAnimation.h"
#include "app.h"
#include <SkColorSpace.h>
// Vertex + fragment shaders for textured cube
static const char* vShader = R"(
#version 300 es
#ifdef GL_ES
precision mediump float;
#endif

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 uMVP;

out vec2 vUV;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV = aUV;
}

)";

static const char* fShader = R"(
#version 300 es
#ifdef GL_ES
precision mediump float;
#endif

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D tex;

void main() {
    fragColor = texture(tex, vUV);
}
)";

void CubeScreenAnimator::init(int w, int h, GrDirectContext* ctx) {
    width = w;
    height = h;
    grContext = ctx;
    startTime = std::chrono::steady_clock::now();
    setupGL();
    updateSurfaces();
}

void CubeScreenAnimator::setupGL() {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    float verts[] = {
        // x,    y,    z,     u, v
        -1, -1,  1,    0, 0,   1, -1,  1,   1, 0,   1,  1,  1,   1, 1,  -1,  1,  1,   0, 1,  // front
        -1, -1, -1,    1, 0,  -1,  1, -1,   1, 1,   1,  1, -1,   0, 1,   1, -1, -1,   0, 0,  // back
        -1,  1, -1,    0, 1,  -1,  1,  1,   1, 1,   1,  1,  1,   1, 0,   1,  1, -1,   0, 0,  // top
        -1, -1, -1,    0, 0,   1, -1, -1,   1, 0,   1, -1,  1,   1, 1,  -1, -1,  1,   0, 1,  // bottom
        -1, -1, -1,    0, 0,  -1, -1,  1,   1, 0,  -1,  1,  1,   1, 1,  -1,  1, -1,   0, 1,  // left
         1, -1,  1,    0, 0,   1, -1, -1,   1, 0,   1,  1, -1,   1, 1,   1,  1,  1,   0, 1   // right
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);

    GLint posLoc = glGetAttribLocation(shaderProgram, "aPos");
    GLint uvLoc = glGetAttribLocation(shaderProgram, "aUV");

    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(uvLoc);
    glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void CubeScreenAnimator::updateSurfaces() {
    glGenTextures(4, textures.data());
    for (int i = 0; i < 4; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        createSurfaceForTexture(i, textures[i]);
    }
}
void CubeScreenAnimator::createSurfaceForTexture(int index, int texId) {
    GrGLTextureInfo texInfo = {
     .fID = (GrGLuint)texId,
     .fTarget = GL_TEXTURE_2D,
     .fFormat = GL_RGBA8
    };

    GrBackendTexture backendTex = GrBackendTextures::MakeGL(
        width,
        height,
        skgpu::Mipmapped::kNo,
        texInfo
    );

    SkSurfaceProps props;
    surfaces[index] = SkSurfaces::WrapBackendTexture(
        grContext,
        backendTex,
        kBottomLeft_GrSurfaceOrigin,
        0,
        kRGBA_8888_SkColorType,
        nullptr,
        &props
    );


    if (surfaces[index]) {
        drawScreen(surfaces[index]->getCanvas(), index);
        // Updated method:
        if (auto dContext = GrAsDirectContext(surfaces[index]->getCanvas()->recordingContext())) {
            dContext->flushAndSubmit();
        }
    }
}

float CubeScreenAnimator::getRotationAngle() {
    float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - startTime).count();
    float cycle = fmodf(elapsed, 20.f);
    return 90.f * floor(cycle / 5.f) + 90.f * (fmodf(cycle, 5.f) / 5.f); // 5s per face
}

void CubeScreenAnimator::render(int w, int h) {
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    int activeFace = getFrontFace();

    if (surfaces[activeFace]) {
        SkCanvas* canvas = surfaces[activeFace]->getCanvas();
        drawAnimatedScreen(canvas, activeFace);
        if (auto dContext = GrAsDirectContext(surfaces[activeFace]->getCanvas()->recordingContext())) {
            dContext->flushAndSubmit();
        }
    }
   
    float angleDeg = getRotationAngle();
    float angleRad = angleDeg * 3.1415926f / 180.f;

    float cosA = cosf(angleRad), sinA = sinf(angleRad);
    float aspect = float(w) / float(h);
    float scale = 1.0f;

    float mvp[16] = {
         cosA * scale / aspect, 0, sinA, 0,
         0, scale, 0, 0,
        -sinA, 0, cosA * scale, -2.5f,
         0, 0, 0, 1
    };

    glUseProgram(shaderProgram);
    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp);

    glBindVertexArray(vao);

    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
    }

    glBindVertexArray(0);
}

GLuint CubeScreenAnimator::createShaderProgram() {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        return shader;
        };
    GLuint vs = compile(GL_VERTEX_SHADER, vShader);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fShader);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void CubeScreenAnimator::drawScreen(SkCanvas* canvas, int index) {
    SkPaint paint;
    paint.setColor(SkColorSetRGB(60 + index * 40, 100, 200));
    canvas->clear(paint.getColor());

    SkFont font;
    font.setSize(64);
    SkPaint textPaint;
    textPaint.setColor(SK_ColorWHITE);

    std::string msg = "Track " + std::to_string(index + 1);
    canvas->drawSimpleText(msg.c_str(), msg.size(), SkTextEncoding::kUTF8,
        50, 100, font, textPaint);
}
void CubeScreenAnimator::drawAnimatedScreen(SkCanvas* canvas, int index) {
    float t = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - startTime).count();

    canvas->clear(SkColorSetRGB(0, 0, 0));
    SkPaint paint;
    SkFont font;
    font.setSize(64);
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);

    switch (index) {
    case 0:
        paint.setColor(SkColorSetRGB(255, 0, 0));
        canvas->drawCircle(200 + 100 * sinf(t), 200, 40, paint);
        break;
    case 1:
        paint.setColor(SkColorSetRGB(0, 255, 0));
        canvas->drawRect(SkRect::MakeXYWH(100, 100 + 50 * cosf(t), 200, 100), paint);
        break;
    case 2:
        paint.setColor(SkColorSetRGB(0, 0, 255));
        canvas->drawLine(0, 0, 400 * sinf(t), 400 * cosf(t), paint);
        break;
    case 3:
        paint.setColor(SkColorSetRGB(255, 255, 0));
        canvas->drawSimpleText("Zooming", 7, SkTextEncoding::kUTF8,
            100 + 40 * cosf(t), 100, font, paint);
        break;
    }
}

