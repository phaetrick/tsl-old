//
// Created by pr on 04.07.25.
//

#ifndef GRAINSTORM_WINDOW_ANIM_H
#define GRAINSTORM_WINDOW_ANIM_H
#include <SkBitmap.h>
#include <SkImage.h>
#include <vector>

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#else
#endif
namespace tsl::graphics{
    struct ShaderUniforms {
        GLint model;
        GLint projection;
        GLint overallAlpha;
        GLint time;
        GLint explodeTime;
        GLint explosionStrength;
        GLint randomOffset;
        GLint ourTexture;
    };

    class LogoAssets {
    public:
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLuint texture = 0;
        GLuint shader = 0;
        int width = 0, height = 0;
        ShaderUniforms uniforms{};
        float orthoProjection[16]{};
        ~LogoAssets() {
            if (vao) glDeleteVertexArrays(1, &vao);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (ebo) glDeleteBuffers(1, &ebo);
            if (texture) glDeleteTextures(1, &texture);
            if (shader) glDeleteProgram(shader);
        }
    };

    GLuint CreateTextureFromPNG(const uint8_t* pngData, size_t pngSize);
    GLuint LoadTextureFromSkImage(const sk_sp<SkImage>& skImage);
    GLuint LoadShader(GLenum shaderType, const char* pSource);
    GLuint CreateShaderProgram(const char* pVertexSource, const char* pFragmentSource);
    ShaderUniforms GetLogoShaderUniformLocations(GLuint program);
    void ComputeOrthoProjection(float* proj, int w, int h);
    GLuint CreateLogoQuadVAO();
    LogoAssets PrepareLogoAssets(sk_sp<SkImage> icon, int w, int h, const char* vertexShaderSrc, const char* fragmentShaderSrc);
    void DrawLogoFrame(const LogoAssets& a, float elapsedSeconds);
    extern const char *explosionFragmentShader;
    extern const char *waveFragmentShader;
    extern const char *waveFragmentShader2;
// --- Simple Shader Program ---
    // (Replace with your actual shader source or loading mechanism)
    extern const char *vertexShaderSource;
    extern const char *testShader;
    // Compile & link helper
    extern GLuint compileShader(GLenum type, const char* src);

    extern GLuint createProgram(const char* vs, const char* fs);


    extern void checkShaderError(GLuint shader, GLenum type, const char* name);

    extern void checkProgramError(GLuint program, const char* name);

    class LogoExplosionEffect {
    public:
        LogoExplosionEffect(sk_sp<SkImage> image, int screenWidth, int screenHeight);
        ~LogoExplosionEffect();

        void render(float timeSeconds);

    private:
        GLuint shaderProgram = 0;
        GLuint texture = 0;
        GLuint vao = 0, vbo = 0, ebo = 0;
        int tileCountX = 10;
        int tileCountY = 10;
        int screenW = 0, screenH = 0;

        struct Uniforms {
            GLint projection;
            GLint time;
            GLint texture;
        } uniforms{};

        void initShader();
        void uploadTexture(const sk_sp<SkImage>& image);
        void createTiledGeometry();
        void computeProjection(float* out) const;

        static const char* vertexShaderSrc();
        static const char* fragmentShaderSrc();
    };
    class LogoSwirler {
    public:
        LogoSwirler(const sk_sp<SkImage>& image, int screenWidth, int screenHeight);

        ~LogoSwirler();

        void render(float progress);

    private:
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLuint shaderProgram = 0;
        GLuint texture = 0;
        GLint uProjection = -1, uProgress = -1, uTexture = -1, uFadeout = -1, uSpins = -1;
        int screenW = 0, screenH = 0;
        int indexCount = 0;
        float projection[16] = {0};
        const int tilesX = {static_cast<int>(10 + 50 * (rand() % 100) / 100.0f)};
        const int tilesY = tilesX;
        const float spins = {2.f + 8.f * (rand() % 100) / 100.0f};

        GLint uLogoPlaneDims = -1; // New uniform location
        float logoPlaneWidth = 0.0f; // Store calculated width
        float logoPlaneHeight = 0.0f; // Store calculated height

        static GLuint compile(GLenum type, const char* src) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            return s;
        }

        void setupShader();

        void setupTiles(const sk_sp<SkImage>& image);
        void computeProjection();
    };

    // Slides the logo in from off-screen right, "impact" squeezes it to a stop,
    // rebounds to normal size, holds, then squeezes again (as if winding up),
    // launches past normal size, and flies off to the right while fading out.
    class LogoSlingshot {
    public:
        LogoSlingshot(const sk_sp<SkImage>& image, int screenWidth, int screenHeight);
        ~LogoSlingshot();

        // t: seconds since the animation started. Plays the scroll-in / impact-squeeze /
        // rebound sequence, then holds at rest indefinitely for any t past that.
        void renderIntro(float t);

        // t: seconds since the exit was triggered. Plays the anticipation squeeze,
        // launch overshoot, and fly-off-with-fade. Returns true once fully faded out.
        bool renderExit(float t);

    private:
        GLuint vao = 0, texture = 0, shaderProgram = 0;
        GLint uModel = -1, uProjection = -1, uTexture = -1, uAlpha = -1;
        float projection[16] = {0};
        int screenW = 0, screenH = 0;

        void setupShader();
        void computeProjection();
        void draw(float tx, float sx, float sy, float alpha);
    };

    // VOLTAIC splash: a real lightning strike that leaves the V behind.
    //
    // A dim stepped leader creeps down from the top edge, then the return stroke
    // ignites the channel from the bottom UP (which is the actual physics: the
    // faint descent always precedes the bright ascent), the screen blows out, and
    // the charge races out of the mark's apex along both arms of the V. It settles
    // over-exposed -> rest, with one subsequent stroke. The exit winds up,
    // discharges back out of the top of the screen, and the V drains into its apex.
    //
    // Everything is emissive on black, so the whole thing draws with additive
    // blending (GL_ONE, GL_ONE) and never needs a sorted alpha pass.
    class LogoLightning {
    public:
        LogoLightning(const sk_sp<SkImage>& image, int screenWidth, int screenHeight);
        ~LogoLightning();

        // t: seconds since the animation started. Past the settle it holds at rest
        // indefinitely (breathing slightly), i.e. until the caller switches to
        // renderExit — same contract as LogoSlingshot::renderIntro.
        void renderIntro(float t);

        // t: seconds since the exit was triggered. Returns true once fully gone.
        bool renderExit(float t);

    private:
        // A jagged channel: a polyline generated by midpoint displacement, plus
        // branches that hang off it. `cum` is cumulative arc length, so a range of
        // the channel can be lit by arc fraction rather than by vertex index —
        // which is what lets the return stroke run up it at a constant speed.
        struct Branch {
            std::vector<float> px, py, cum;
            float at = 0.f;      // arc fraction of the parent where it splits
            float bright = 0.5f;
        };
        struct Bolt {
            std::vector<float> px, py, cum;
            std::vector<Branch> branches;
        };

        // What one frame of the animation looks like. renderIntro/renderExit fill
        // this in and hand it to submit(); no GL happens until then.
        struct Frame {
            float flashWhite = 0.f, flashWarm = 0.f;
            float logoAlpha = 0.f, logoBright = 1.f, logoScale = 1.f;
            float reveal = 0.f, revealSoft = 0.06f, frontBand = 0.f;
            struct Channel { const Bolt* bolt; float from, to, width, alpha; };
            Channel channels[3];
            int channelCount = 0;
        };

        GLuint quadVao = 0, texture = 0;
        GLuint logoProgram = 0, boltProgram = 0, washProgram = 0;
        GLuint boltVao = 0, boltVbo = 0;
        GLint uLogoModel = -1, uLogoProj = -1, uLogoTex = -1, uLogoAlpha = -1,
              uLogoBright = -1, uLogoReveal = -1, uLogoSoft = -1, uLogoBand = -1;
        GLint uBoltProj = -1;
        GLint uWashModel = -1, uWashProj = -1, uWashCenter = -1, uWashRadius = -1,
              uWashColor = -1, uWashStopMid = -1, uWashA0 = -1, uWashAMid = -1, uWashA1 = -1;
        float projection[16] = {0};
        int screenW = 0, screenH = 0;
        float halfW = 1.f, halfH = 1.f;   // view half-extents in world units
        uint32_t seed = 0;
        Bolt mainBolt{}, subsequent{};
        std::vector<float> stripScratch;  // reused every frame, never reallocated

        void setupShaders();
        void buildBolts();
        void submit(const Frame& f);
        void drawChannel(const Bolt& b, float from, float to, float width, float alpha);
        void drawWash(float cx, float cy, float radius, const float rgb[3],
                      float stopMid, float a0, float aMid, float a1);
    };

    class ImGuiShapeRenderer {
    public:
// Structure to hold vertex data (position and color)
        struct VertexPC {
            float x, y; // Position
            float r, g, b, a; // Color
        };
        // --- Constructor & Destructor ---
        ImGuiShapeRenderer();

        ~ImGuiShapeRenderer();

        // --- Initialization (Call once after OpenGL context is created) ---
        void init();

        // --- Rendering (Call inside your rendering loop) ---
        void render(float w, float h);


    private:
        GLuint shaderProgram;
        GLuint vao;
        GLuint vbo;

        GLint uProjectionLoc;
        float projectionMatrix[16]{}; // Column-major for OpenGL

        // --- Private Helper Methods (Implemented inline) ---

        static GLuint compileShader(GLenum type, const char* src) {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);
            checkShaderError(shader, type, (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"));
            return shader;
        }

        static GLuint createProgram(GLuint vertexShader, GLuint fragmentShader) {
            GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glLinkProgram(program);
            checkProgramError(program, "MAIN_PROGRAM");
            return program;
        }

        static void checkShaderError(GLuint shader, GLenum type, const char* name) {
            GLint success;
            GLchar infoLog[1024];
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                LOGE("ERROR::PROGRAM::%s::LINKING_FAILED %s", name,infoLog);
            }
        }

        static void checkProgramError(GLuint program, const char* name) {
            GLint success;
            GLchar infoLog[1024];
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(program, 1024, NULL, infoLog);
                LOGE("ERROR::PROGRAM::%s::LINKING_FAILED %s", name,infoLog);
            }
        }

        void updateOrthographicProjection(float w, float h);

        static void generateFilledCircleVertices(std::vector<VertexPC>& vertices, float cx, float cy, float radius, const GLfloat color[4], int segments);

        static void generateStrokedArcVertices(std::vector<VertexPC>& vertices, float cx, float cy, float radius, float startAngleRad, float endAngleRad, float thickness, const GLfloat color[4], int segments);
    };




}

#endif //GRAINSTORM_WINDOW_ANIM_H
