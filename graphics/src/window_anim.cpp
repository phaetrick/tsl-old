//
// Created by pr on 04.07.25.
//
#include "defines.h"
#include "logger.h"
#include "window_anim.h"
#include <SkImage.h>
#include <SkData.h>
#include <SkStream.h>
#include <include/codec/SkCodec.h>
#include <SkBitmap.h>
#include <random>
// --- 2. Projection Matrix (`projectionLocation`) ---
// This is your orthographic projection matrix. You likely set this up once
// when the screen dimensions (w, h) are known and it doesn't change per frame
// unless the screen resizes.
// float orthoProjection[16] = { ... }; // As defined previously, considering aspect ratio
// Example:
// float orthoProjection[16];
// if (w != 0 && h != 0) {
//     float aspect = (float)w / (float)h;
//     if (aspect > 1.0f) { // Wider
//         orthoProjection[0] = 1.0f / aspect; orthoProjection[5] = 1.0f;
//     } else { // Taller or square
//         orthoProjection[0] = 1.0f;          orthoProjection[5] = aspect;
//     }
//     orthoProjection[10] = 1.0f; orthoProjection[15] = 1.0f; // Standard Z and W for basic ortho
//     // Fill other elements as 0.0f or ensure your full matrix setup is correct
//     // A common simple orthographic matrix for [-1,1] view volume, adjusted for aspect:
//     // If right = 1, left = -1, top = 1, bottom = -1, near = -1, far = 1
//     // X_scale  0       0       0
//     // 0        Y_scale 0       0
//     // 0        0       Z_scale Z_translate
//     // 0        0       0       1
//     // For orthoProjection in your case (assuming quad is -0.5 to 0.5):
//     // You might scale it to fit the screen appropriately. If your quad vertices
//     // are already in NDC-like space adjusted for where you want the logo,
//     // then orthoProjection could even be an identity matrix initially, or
//     // more commonly an ortho matrix that maps your desired world units to NDC.
//     // Let's use the aspect-corrected one from previous examples, assuming your
//     // quad is defined in a space like [-0.5, 0.5] and you want it centered.
// }
// (Make sure orthoProjection is correctly initialized based on w, h)

using namespace tsl::graphics;
// --- Header ---

// --- Utility Functions ---
tsl::graphics::ShaderUniforms tsl::graphics::GetLogoShaderUniformLocations(GLuint program) {
    ShaderUniforms u{};
    u.model = glGetUniformLocation(program, "model");
    u.projection = glGetUniformLocation(program, "projection");
    u.overallAlpha = glGetUniformLocation(program, "overallAlpha");
    u.time = glGetUniformLocation(program, "time");
    u.explodeTime = glGetUniformLocation(program, "explodeTime");
    u.explosionStrength = glGetUniformLocation(program, "explosionStrength");
    u.randomOffset = glGetUniformLocation(program, "randomOffset");
    u.ourTexture = glGetUniformLocation(program, "ourTexture");
    return u;
}

void tsl::graphics::ComputeOrthoProjection(float* proj, int w, int h) {
    float Z_NEAR = -1.0f, Z_FAR = 1.0f;
    float R, L, T, B;
    if (w >= h) {
        R = (float)w / (float)h;
        L = -R;
        T = 1.0f;
        B = -1.0f;
    } else {
        R = 1.0f;
        L = -1.0f;
        T = (float)h / (float)w;
        B = -T;
    }
    proj[0] = 2.0f / (R - L);
    proj[4] = 0.0f;
    proj[8] = 0.0f;
    proj[12] = -(R + L) / (R - L);
    proj[1] = 0.0f;
    proj[5] = 2.0f / (T - B);
    proj[9] = 0.0f;
    proj[13] = -(T + B) / (T - B);
    proj[2] = 0.0f;
    proj[6] = 0.0f;
    proj[10] = -2.0f / (Z_FAR - Z_NEAR);
    proj[14] = -(Z_FAR + Z_NEAR) / (Z_FAR - Z_NEAR);
    proj[3] = 0.0f;
    proj[7] = 0.0f;
    proj[11] = 0.0f;
    proj[15] = 1.0f;
}

GLuint tsl::graphics::CreateLogoQuadVAO() {
    float vertices[] = {
            -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
            0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 0.0f
    };
    unsigned int indices[] = {0, 1, 2, 2, 3, 0};
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return VAO;
}

tsl::graphics::LogoAssets tsl::graphics::PrepareLogoAssets(sk_sp<SkImage> icon, int w, int h,
                             const char* vertexShaderSrc, const char* fragmentShaderSrc) {
    tsl::graphics::LogoAssets assets;
    assets.shader = CreateShaderProgram(vertexShaderSrc, fragmentShaderSrc);
    if (assets.shader == 0) return {};

    assets.texture = tsl::graphics::LoadTextureFromSkImage(icon);
    if (assets.texture == 0) {
        glDeleteProgram(assets.shader);
        return {};
    }

    assets.vao = CreateLogoQuadVAO();
    assets.uniforms = GetLogoShaderUniformLocations(assets.shader);
    ComputeOrthoProjection(assets.orthoProjection, w, h);
    LOGD("Shader: %u, Texture: %u, VAO: %u", assets.shader, assets.texture, assets.vao);
    return assets;
}

// --- Rendering Function ---
void tsl::graphics::DrawLogoFrame(const tsl::graphics::LogoAssets& a, float elapsedSeconds) {

    const float identity[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
    };

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(a.shader);

    glUniformMatrix4fv(a.uniforms.model, 1, GL_FALSE, identity);
    glUniformMatrix4fv(a.uniforms.projection, 1, GL_FALSE, a.orthoProjection);
    glUniform1f(a.uniforms.overallAlpha, fminf(1.0f, elapsedSeconds));
    glUniform1f(a.uniforms.time, elapsedSeconds);
    glUniform1f(a.uniforms.explodeTime, 4.0f);
    glUniform1f(a.uniforms.explosionStrength, 1.5f);
    glUniform2f(a.uniforms.randomOffset, (rand() % 1000) / 1000.0f, (rand() % 1000) / 1000.0f);
    glUniform1i(a.uniforms.ourTexture, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, a.texture);
    glBindVertexArray(a.vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}


const char *tsl::graphics::vertexShaderSource =
        "#version 300 es\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 vTexCoord;\n"
        "uniform mat4 model;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    gl_Position = projection * model * vec4(aPos, 1.0);\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";



const char *tsl::graphics::waveFragmentShader = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;

out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float overallAlpha;
uniform float time;
uniform vec2 resolution; // You have this uniform, but it's not used in the provided code. Ensure it's needed or remove it.
uniform vec2 randomOffset;

// Wave Parameters
const float waveAmplitude = 0.015; // << VERY SMALL
const float waveFrequency = 5.0;
const float waveSpeed = 16.0;
const float waveAmplitude2 = 0.01; // << VERY SMALL
const float waveFrequency2 = 10.0;
const float waveSpeed2 = -1.2;

// Simple repeatable hash
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth interpolated 2D noise (Value Noise)
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
           (c - a)* u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
}

void main() {
    vec2 distortedTexCoord = vTexCoord;

    // --- Wave Calculation ---
    float displacement = sin(vTexCoord.y * waveFrequency + time * waveSpeed) * waveAmplitude;
    displacement += cos(vTexCoord.x * waveFrequency2 - time * waveSpeed2) * waveAmplitude2;

    // --- Add value noise (stable and mild) ---
    float noiseVal = noise(vTexCoord * 10.0 + randomOffset + vec2(time * 0.2));
    displacement *= 0.9 + 0.1 * noiseVal;

    distortedTexCoord.x += displacement;

    // --- Radial Wave ---
    vec2 toCenter = vec2(0.5) - vTexCoord;
    float distToCenter = length(toCenter);
    float radialWave = sin(distToCenter * waveFrequency - time * waveSpeed) * waveAmplitude;
    radialWave *= (1.0 - smoothstep(0.0, 0.5, distToCenter));
    distortedTexCoord += normalize(toCenter) * radialWave;

    // Clamp to safe texcoords
    distortedTexCoord = clamp(distortedTexCoord, 0.0, 1.0);

    // --- Edge fade (original texcoords, not distorted) ---
    float edgeFactor = smoothstep(0.0, 0.1, vTexCoord.x) * (1.0 - smoothstep(0.9, 1.0, vTexCoord.x));
    edgeFactor *= smoothstep(0.0, 0.1, vTexCoord.y) * (1.0 - smoothstep(0.9, 1.0, vTexCoord.y));

    float finalAlpha = edgeFactor * overallAlpha;

    vec4 texColor = texture(ourTexture, distortedTexCoord);
    FragColor = vec4(texColor.rgb, texColor.a * finalAlpha);
})glsl";

const char *tsl::graphics::waveFragmentShader2 = R"glsl(
        #version 300 es
        precision mediump float;

        in vec2 vTexCoord;
        out vec4 FragColor;

        uniform sampler2D inputTexture;
        uniform float time;
        uniform vec2 randomOffset;

// Simple repeatable hash
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth interpolated 2D noise (Value Noise)
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
           (c - a)* u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
}

        void main() {
            vec2 uv = vTexCoord;

            // Animate with waves + noise
            float disp = sin(uv.y * 10.0 + time) * 0.01;
            disp += noise(uv * 10.0 + randomOffset + vec2(time * 0.5)) * 0.01;

            uv.x += disp;

            vec4 color = texture(inputTexture, uv);
            FragColor = color;
        }
    )glsl";
const char *tsl::graphics::explosionFragmentShader = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float overallAlpha;
uniform float time;
uniform float explodeTime;
uniform float explosionStrength;

// --- Parameters ---
const float waveAmplitude = 0.015;
const float waveFrequency = 15.0;
const float waveSpeed = 16.0;
const float waveAmplitude2 = 0.01;
const float waveFrequency2 = 10.0;
const float waveSpeed2 = -1.2;

// --- 2D Rotation ---
vec2 rotate2D(vec2 pt, float angle) {
float s = sin(angle);
float c = cos(angle);
return mat2(c, -s, s, c) * pt;
}

void main() {
    vec2 distortedTexCoord = vTexCoord;

    // --- Wave distortion ---
    float displacement = sin(vTexCoord.y * waveFrequency + time * waveSpeed) * waveAmplitude;
    displacement += cos(vTexCoord.x * waveFrequency2 - time * waveSpeed2) * waveAmplitude2;
    distortedTexCoord.x += displacement;

    // --- Radial distortion ---
    vec2 toCenter = vec2(0.5) - vTexCoord;
    float distToCenter = length(toCenter);
    float radialWave = sin(distToCenter * waveFrequency - time * waveSpeed) * waveAmplitude;
    radialWave *= (1.0 - smoothstep(0.0, 0.5, distToCenter));
    distortedTexCoord += normalize(toCenter) * radialWave;

    // --- Explosion ---
    float explodeFactor = clamp(time - explodeTime, 0.0, 1.0);
    vec2 explodeDir = normalize(vTexCoord - vec2(0.5));
    distortedTexCoord += explodeDir * explodeFactor * explosionStrength;

    // --- Per-pixel rotation ---
    float angleSeed = dot(vTexCoord, vec2(12.9898, 78.233)); // cheap per-pixel seed
    float random = fract(sin(angleSeed) * 43758.5453);
    float angle = explodeFactor * random * 6.2831; // up to 2π (360°)
    distortedTexCoord = rotate2D(distortedTexCoord - vec2(0.5), angle) + vec2(0.5);

    // --- Discard off-bounds ---
    if (any(lessThan(distortedTexCoord, vec2(0.0))) ||
        any(greaterThan(distortedTexCoord, vec2(1.0)))) {
        discard;
    }

    // --- Sample + Alpha Fade ---
    vec4 texColor = texture(ourTexture, distortedTexCoord);
    float edgeFade = smoothstep(0.0, 0.1, vTexCoord.x) * (1.0 - smoothstep(0.9, 1.0, vTexCoord.x));
    edgeFade *= smoothstep(0.0, 0.1, vTexCoord.y) * (1.0 - smoothstep(0.9, 1.0, vTexCoord.y));
    float fadeOut = 1.0 - explodeFactor;
    float finalAlpha = overallAlpha * edgeFade * fadeOut;

    FragColor = vec4(texColor.rgb, finalAlpha);
})glsl";

const char *tsl::graphics::testShader = R"glsl(#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    FragColor = vec4(vTexCoord.xy, 0.0, 1.0); // Top right should be yellowish, bottom-left black
})glsl";


GLuint tsl::graphics::CreateTextureFromPNG(const uint8_t* pngData, size_t pngSize) {
    if (!pngData || pngSize == 0) {
        LOGE("Invalid PNG input data.");
        return 0;
    }

    // Load image using Skia
    sk_sp<SkData> data = SkData::MakeWithoutCopy(pngData, pngSize);
    auto stream = std::make_unique<SkMemoryStream>(data);
    auto codec = SkCodec::MakeFromStream(std::move(stream));
    if (!codec) {
        LOGE("SkCodec creation failed.");
        return 0;
    }

    // Decode image as RGBA (premultiplied)
    SkImageInfo info = codec->getInfo()
            .makeColorType(kRGBA_8888_SkColorType)
            .makeAlphaType(kPremul_SkAlphaType);

    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(info)) {
        LOGE("Bitmap allocation failed.");
        return 0;
    }

    if (codec->getPixels(info, bitmap.getPixels(), bitmap.rowBytes()) != SkCodec::kSuccess) {
        LOGE("Image decoding failed.");
        return 0;
    }

    // Upload to OpenGL ES
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // VERY IMPORTANT: Align rows properly!
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // Default is 4, but Skia bitmaps might not be aligned to that.

    // Upload texture to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bitmap.width(), bitmap.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bitmap.getPixels());

    // Set filtering and wrap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // GL_NEAREST if no smoothing
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGE("glTexImage2D failed with error: 0x%04X", err);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    return textureID;
}

// Helper function to upload SkImage data to an OpenGL texture
GLuint tsl::graphics::LoadTextureFromSkImage(const sk_sp<SkImage>& skImage) {
    if (!skImage) {
        LOGE("LoadTextureFromSkImage: skImage is null.");
        return 0;
    }

    // Ensure the image is raster-backed (CPU pixels) or read it back.
    // Forcing a read-back for simplicity here.
    // More advanced interop might involve GrBackendTexture if Skia is GPU-backed.
    SkBitmap bitmap;
    if (!skImage->asLegacyBitmap(&bitmap)) {
        // If asLegacyBitmap fails, try to read pixels directly.
        // This is often needed if the SkImage is texture-backed.
        SkImageInfo info = SkImageInfo::Make(skImage->width(), skImage->height(),
                                             kRGBA_8888_SkColorType, // Common target format
                                             kPremul_SkAlphaType, skImage->refColorSpace());
        if (!bitmap.tryAllocPixels(info)) {
            LOGE("LoadTextureFromSkImage: Failed to allocate pixels for bitmap.");
            return 0;
        }
        if (!skImage->readPixels(bitmap.pixmap(), 0, 0)) {
            LOGE("LoadTextureFromSkImage: Failed to read pixels from SkImage.");
            return 0;
        }
    }

    // We expect RGBA_8888 for easy upload with GL_RGBA and GL_UNSIGNED_BYTE
    if (bitmap.colorType() != kRGBA_8888_SkColorType) {
        LOGE("LoadTextureFromSkImage: Bitmap is not RGBA_8888. Actual format: %d. Conversion might be needed.", bitmap.colorType());
        // You might need to convert the bitmap to RGBA_8888 here if it's a different format
        // For simplicity, this example assumes it's already compatible or you handle conversion.
        // SkBitmap convertedBitmap;
        // SkImageInfo targetInfo = SkImageInfo::Make(bitmap.width(), bitmap.height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType);
        // if (bitmap.canCopyTo(kRGBA_8888_SkColorType) && convertedBitmap.tryAllocPixels(targetInfo)) {
        //     if (bitmap.readPixels(convertedBitmap.pixmap(), 0, 0)) {
        //         bitmap = convertedBitmap; // Use the converted bitmap
        //     } else {
        //        LOGE("Failed to convert bitmap to RGBA_8888.");
        //        return 0;
        //     }
        // } else {
        //    LOGE("Cannot convert bitmap to RGBA_8888.");
        //    return 0;
        // }
    }


    const void* pixelData = bitmap.getPixels();
    if (!pixelData) {
        LOGE("LoadTextureFromSkImage: Could not get pixel data from bitmap.");
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    if (glGetError() != GL_NO_ERROR) {
        LOGE("LoadTextureFromSkImage: glGenTextures failed.");
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Use mipmaps for minification
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels
    // Assuming RGBA_8888_SkColorType maps to GL_RGBA and GL_UNSIGNED_BYTE
    // Skia stores pixels premultiplied by alpha. OpenGL's default is non-premultiplied.
    // If you need to handle premultiplication in the shader or unpremultiply before upload,
    // that's an additional step. For simple display, it might look okay, but for blending,
    // it's important.
    // For now, we'll proceed assuming the visual difference is acceptable or handled in shader.
    glPixelStorei(GL_UNPACK_ALIGNMENT, bitmap.bytesPerPixel()); // Crucial if not 4
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width(), bitmap.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelData);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGE("LoadTextureFromSkImage: glTexImage2D failed with error 0x%x. Width: %d, Height: %d, BPP: %d",
             error, bitmap.width(), bitmap.height(), bitmap.bytesPerPixel());
        glDeleteTextures(1, &textureID);
        return 0;
    }

    glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps

    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture

    LOGI("Texture loaded successfully from SkImage. ID: %d, Size: %dx%d", textureID, bitmap.width(), bitmap.height());
    return textureID;
}

GLuint tsl::graphics::LoadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n", shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint tsl::graphics::CreateShaderProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        LOGE("Failed to load vertex shader");
        return 0;
    }

    GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        LOGE("Failed to load fragment shader");
        glDeleteShader(vertexShader); // Clean up vertex shader
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        // You can add glGetError checks here if needed
        glAttachShader(program, fragmentShader);
        // You can add glGetError checks here if needed

        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    } else {
        LOGE("Failed to create shader program.");
    }

    // Shaders are no longer needed after linking
    glDetachShader(program, vertexShader); // Optional, but good practice
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
class LogoExploderOK {
public:
    LogoExploderOK(const sk_sp<SkImage> &image, int screenWidth, int screenHeight)
            : screenW(screenWidth), screenH(screenHeight) {
        setupShader();
        setupTiles(image);
        computeProjection();
    }

    void render(float timeSeconds) {
        glUseProgram(shaderProgram);
        glUniform1f(uTime, timeSeconds);
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(uTexture, 0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }

private:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint shaderProgram = 0;
    GLuint texture = 0;
    GLint uProjection = -1, uTime = -1, uTexture = -1;
    int screenW = 0, screenH = 0;
    int indexCount = 0;
    float projection[16] = {0};
    const int tilesX = 22;
    const int tilesY = 22;

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    }

    void setupShader() {
        const char* vs = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec2 aCenter;
layout(location = 3) in vec2 aDir;
layout(location = 4) in float aAngleSeed;

uniform float time;
uniform mat4 uProjection;

out vec2 vTexCoord;

// --- Utility: 3D Rotation Matrices ---
mat3 rotateX(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat3(1, 0, 0, 0, c, -s, 0, s, c);
}
mat3 rotateY(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat3(c, 0, s, 0, 1, 0, -s, 0, c);
}
mat3 rotateZ(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat3(c, -s, 0, s, c, 0, 0, 0, 1);
}

// --- Easing Function ---
float easeOut(float t) {
    return 1.0 - pow(1.0 - t, 3.0);
}

// --- Pseudo-random Function (based on tile center) ---
float random(vec2 seed) {
    return fract(sin(dot(seed, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    // Compute randomized explosion timing
    float offset = random(aCenter) * 0.1; // delay up to 0.5s
    float speed = mix(0.8, 1.3, random(aCenter + 7.0)); // speed 0.8–1.3

    float rawT = clamp((time - 1.0 - offset) * speed, 0.0, 1.0);
    float eased = easeOut(rawT);
    float travel = eased * 4.0;

    float angle = aAngleSeed * 6.2831 * eased; // random angle, 0–2π

    vec3 p = vec3(aPos - aCenter, 0.0);
    p = rotateZ(angle) * rotateY(angle * 0.7) * rotateX(angle * 1.3) * p;
    p.xy += aCenter + aDir * travel;

    gl_Position = uProjection * vec4(p.xy, 0.0, 1.0);
    vTexCoord = aTex;
}
        )glsl";

        const char* fs = R"glsl(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        uniform sampler2D uTexture;
        out vec4 FragColor;

        void main() {
            vec4 color = texture(uTexture, vTexCoord);
            FragColor = vec4(color.rgb, color.a);
        }
        )glsl";

        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, v);
        glAttachShader(shaderProgram, f);
        glLinkProgram(shaderProgram);
        // Shaders are no longer needed after linking
        glDetachShader(shaderProgram, v); // Optional, but good practice
        glDetachShader(shaderProgram, f);
        glDeleteShader(v);
        glDeleteShader(f);

        uProjection = glGetUniformLocation(shaderProgram, "uProjection");
        uTime = glGetUniformLocation(shaderProgram, "time");
        uTexture = glGetUniformLocation(shaderProgram, "uTexture");
    }

    void setupTiles(const sk_sp<SkImage>& image) {
        int imgW = image->width();
        int imgH = image->height();

        SkPixmap pixmap;
        image->peekPixels(&pixmap);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        float tileW = 1.0f / tilesX;
        float tileH = 1.0f / tilesY;

        for (int y = 0; y < tilesY; ++y) {
            for (int x = 0; x < tilesX; ++x) {
                float u0 = x * tileW;
                float v0 = y * tileH;
                float u1 = u0 + tileW;
                float v1 = v0 + tileH;

                float cx = (u0 + u1) * 0.5f;
                float cy = (v0 + v1) * 0.5f;
                float tx = cx - 0.5f;
                float ty = cy - 0.5f;
                float len = sqrtf(tx * tx + ty * ty) + 1e-6f;
                tx /= len;
                ty /= len;
                float angleSeed = sinf(cx * 12.0f + cy * 78.0f);

                float px0 = u0 - 0.5f;
                float py0 = v0 - 0.5f;
                float px1 = u1 - 0.5f;
                float py1 = v1 - 0.5f;

                unsigned int base = vertices.size() / 9;
                vertices.insert(vertices.end(), {
                        px0, py0, u0, v0, cx, cy, tx, ty, angleSeed,
                        px1, py0, u1, v0, cx, cy, tx, ty, angleSeed,
                        px1, py1, u1, v1, cx, cy, tx, ty, angleSeed,
                        px0, py1, u0, v1, cx, cy, tx, ty, angleSeed
                });
                indices.insert(indices.end(), {
                        base + 0, base + 1, base + 2,
                        base + 2, base + 3, base + 0
                });
            }
        }

        indexCount = indices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        size_t stride = 9 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(4);
    }

    void computeProjection() {
        float aspect = static_cast<float>(screenW) / screenH;
        float L, R, B, T;
        if (aspect >= 1.0f) {
            L = -aspect; R = aspect;
            B = -1.0f;   T = 1.0f;
        } else {
            L = -1.0f;   R = 1.0f;
            B = -1.0f / aspect; T = 1.0f / aspect;
        }
        float N = -1.0f, F = 1.0f;
        projection[0] = 2.0f / (R - L); projection[1] = 0; projection[2] = 0; projection[3] = 0;
        projection[4] = 0; projection[5] = 2.0f / (T - B); projection[6] = 0; projection[7] = 0;
        projection[8] = 0; projection[9] = 0; projection[10] = -2.0f / (F - N); projection[11] = 0;
        projection[12] = -(R + L) / (R - L); projection[13] = -(T + B) / (T - B); projection[14] = -(F + N) / (F - N); projection[15] = 1.0f;
    }

    const char* vsSwirl = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec2 aCenter;
layout(location = 3) in vec2 aDir;
layout(location = 4) in float aAngleSeed;

uniform float time;
uniform mat4 uProjection;

out vec2 vTexCoord;

float easeOut(float t) {
    return 1.0 - pow(1.0 - t, 2.0);
}

float random(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    float startDelay = random(aCenter) * 0.5;
    float t = clamp((time - 1.0 - startDelay), 0.0, 1.0);
    float eased = easeOut(t);

    float radius = eased * 2.5; // how far tiles move outward
    float swirlSpeed = 6.0;     // number of swirl turns
    float angle = swirlSpeed * eased * 6.2831 + aAngleSeed; // swirl rotation

    vec2 swirlOffset = vec2(cos(angle), sin(angle)) * radius;

    vec3 p = vec3(aPos - aCenter, 0.0);
    p.xy += swirlOffset + aCenter;

    gl_Position = uProjection * vec4(p.xy, 0.0, 1.0);
    vTexCoord = aTex;
}
        )glsl";

};


class LogoSwirlerOk {
public:
    LogoSwirlerOk(const sk_sp<SkImage>& image, int screenWidth, int screenHeight)
            : screenW(screenWidth), screenH(screenHeight) {
        setupShader();
        setupTiles(image);
        computeProjection();
    }

    void render(float timeSeconds, float fadeout = 0.0) {
        glUseProgram(shaderProgram);
        glUniform1f(uTime, timeSeconds);
        glUniform1f(uFadeout, fadeout);
        glUniform1f(uSpins, spins);

        glUniform2f(uLogoPlaneDims, logoPlaneWidth, logoPlaneHeight);
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(uTexture, 0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }

private:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint shaderProgram = 0;
    GLuint texture = 0;
    GLint uProjection = -1, uTime = -1, uTexture = -1, uFadeout = -1, uSpins = -1;
    int screenW = 0, screenH = 0;
    int indexCount = 0;
    float projection[16] = {0};
    const int tilesX = {static_cast<int>(8 + 10 * (rand() % 100) / 100.0f)};
    const int tilesY = tilesX;
    const float spins = {2.f + 3.f * (rand() % 100) / 100.0f};

    GLint uLogoPlaneDims = -1; // New uniform location
    float logoPlaneWidth = 0.0f; // Store calculated width
    float logoPlaneHeight = 0.0f; // Store calculated height

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    }

    void setupShader() {
        const char* vs = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec2 aCenter;
layout(location = 3) in float aDelay;

uniform float time;
uniform float fadeout;
uniform float spins;
uniform mat4 uProjection;
uniform vec2 uLogoPlaneDims; // e.g., vec2(1.0, 1.0) if square logo

out vec2 vTexCoord;

float easeOutCubic(float t) {
    return 1.0 - pow(1.0 - t, 3.0);
}

void main() {
    float t = time;
    if(t < 1.0) t = clamp((1.0 - t) - aDelay, 0.0, 1.0);
    else if (fadeout == 0.0) t = 0.0;
    else t = clamp(fadeout - aDelay, 0.0, 1.0);
    float eased = easeOutCubic(t);

    // Spiral origin: top-left corner of centered logo
    vec2 spiralOrigin = vec2(-uLogoPlaneDims.x * 0.5, uLogoPlaneDims.y * 0.5);
    // Direction vector from spiral origin to current vertex
    vec2 fromOrigin = aPos - spiralOrigin;
    float dist = length(fromOrigin);
    float angle = atan(fromOrigin.y, fromOrigin.x);

    // Spiral outward: increase distance over time
float spiralRadius = dist * (1.0 + pow(eased, 2.5) * 3.0);
float spiralAngle = angle + pow(eased, 3.0) * 6.2831 * 2.0;
    // Compute final position via spiral
    vec2 spiralOffset = vec2(cos(spiralAngle), sin(spiralAngle)) * spiralRadius;
    vec2 spiralPos = spiralOrigin + spiralOffset;

    // Don't scale/shrink anymore — logo tiles stay full size
    gl_Position = uProjection * vec4(spiralPos, 0.0, 1.0);
    vTexCoord = aTex;
}

        )glsl";

        const char* fs = R"glsl(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        uniform sampler2D uTexture;
        out vec4 FragColor;
        void main() {
            vec4 color = texture(uTexture, vTexCoord);
            if (color.a < 0.01) discard;
            FragColor = color;
        }
        )glsl";

        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, v);
        glAttachShader(shaderProgram, f);
        glLinkProgram(shaderProgram);
        glDeleteShader(v);
        glDeleteShader(f);

        uProjection = glGetUniformLocation(shaderProgram, "uProjection");
        uTime = glGetUniformLocation(shaderProgram, "time");
        uTexture = glGetUniformLocation(shaderProgram, "uTexture");
        uLogoPlaneDims = glGetUniformLocation(shaderProgram, "uLogoPlaneDims");
        uFadeout = glGetUniformLocation(shaderProgram, "fadeout");
        uSpins = glGetUniformLocation(shaderProgram, "spins");

    }

    void setupTiles(const sk_sp<SkImage>& image) {
        int imgW = image->width();
        int imgH = image->height();

        // Calculate aspect ratio of the image
        float imageAspect = static_cast<float>(imgW) / imgH;

        SkPixmap pixmap;
        image->peekPixels(&pixmap);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        float tileW = 1.0f / tilesX;
        float tileH = 1.0f / tilesY;

        // Determine the scaling factors for the logo to fit within the screen while maintaining aspect ratio
        // We want the logo to be centered and fit the screen without distortion.
        // Let's make the logo's height fill the normalized [-1, 1] range, and adjust width based on image aspect.
        // Then scale these to fit the screen's aspect ratio via the projection matrix.
        float logoPlaneWidth = imageAspect;
        float logoPlaneHeight = 1.0f; // Standardized to 1 unit in the local space for height

        // Adjust these if the image is wider than the screen aspect ratio, to fit width instead of height
        float screenAspect = static_cast<float>(screenW) / screenH;
        if (imageAspect > screenAspect) { // Image is wider than the screen, so fit to width
            logoPlaneWidth = screenAspect;
            logoPlaneHeight = screenAspect / imageAspect;
        } else { // Image is taller or similar aspect to screen, so fit to height
            logoPlaneWidth = imageAspect * (1.0f / screenAspect);
            logoPlaneHeight = 1.0f;
        }
        this->logoPlaneWidth = logoPlaneWidth; this->logoPlaneHeight = logoPlaneHeight;

        for (int y = 0; y < tilesY; ++y) {
            for (int x = 0; x < tilesX; ++x) {
                float u0 = x * tileW, v0 = y * tileH;
                float u1 = u0 + tileW, v1 = v0 + tileH;

                // Calculate tile center in the logo's scaled coordinate system
                // These will range from -logoPlaneWidth/2 to logoPlaneWidth/2 for x
                // and -logoPlaneHeight/2 to logoPlaneHeight/2 for y
                float cx = (u0 + u1) * 0.5f * logoPlaneWidth - (logoPlaneWidth * 0.5f);
                float cy = (v0 + v1) * 0.5f * logoPlaneHeight - (logoPlaneHeight * 0.5f);

                float delay = (float(x + y) + static_cast<float>(rand() % 100) / 100.0f) / (tilesX + tilesY);

                // Calculate vertex positions within the logo's scaled coordinate system
                float px0 = u0 * logoPlaneWidth - (logoPlaneWidth * 0.5f);
                float py0 = v0 * logoPlaneHeight - (logoPlaneHeight * 0.5f);
                float px1 = u1 * logoPlaneWidth - (logoPlaneWidth * 0.5f);
                float py1 = v1 * logoPlaneHeight - (logoPlaneHeight * 0.5f);

                unsigned int base = vertices.size() / 7; // Corrected stride for new vertex data layout
                vertices.insert(vertices.end(), {
                        px0, py0, u0, v0, cx, cy, delay,
                        px1, py0, u1, v0, cx, cy, delay,
                        px1, py1, u1, v1, cx, cy, delay,
                        px0, py1, u0, v1, cx, cy, delay
                });

                indices.insert(indices.end(), {
                        base + 0, base + 1, base + 2,
                        base + 2, base + 3, base + 0
                });
            }
        }

        indexCount = indices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        size_t stride = 7 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
    }
    void computeProjection() {
        float aspect = static_cast<float>(screenW) / screenH;
        // We now assume our geometry is defined such that its aspect ratio matches
        // the image's intended aspect ratio relative to the screen.
        // The projection should simply map the normalized device coordinates to the screen.
        // The previous setup in setupTiles now ensures the logo itself is correctly proportioned.
        // L, R, B, T should define the visible range in our normalized coordinate system,
        // which effectively aligns with the screen's aspect ratio.

        // If the geometry is defined in a [-1, 1] x [-1, 1] square, and we want to fit it to the screen:
        float L = -aspect;
        float R = aspect;
        float B = -1.0f;
        float T = 1.0f;

        // This setup correctly maps the normalized device coordinates [-1,1] in Y and [-aspect, aspect] in X to the screen.
        // The previous changes in setupTiles make sure the logo geometry *within* this normalized space
        // already respects its own aspect ratio.

        float N = -1.0f;
        float F = 1.0f;

        projection[0] = 2.0f / (R - L);  projection[1] = 0;               projection[2] = 0;               projection[3] = 0;
        projection[4] = 0;               projection[5] = 2.0f / (T - B);  projection[6] = 0;               projection[7] = 0;
        projection[8] = 0;               projection[9] = 0;               projection[10] = -2.0f / (F - N);projection[11] = 0;
        projection[12] = -(R + L) / (R - L); projection[13] = -(T + B) / (T - B); projection[14] = -(F + N) / (F - N); projection[15] = 1.0f;
    }
};
class LogoSwirlerAddZNotOk {
public:
    LogoSwirlerAddZNotOk(const sk_sp<SkImage>& image, int screenWidth, int screenHeight)
            : screenW(screenWidth), screenH(screenHeight) {
        setupShader();
        setupTiles(image);
        computeProjection();
    }
// Inside LogoSwirler class, or as a free function

    void render(float progress) {
        glUseProgram(shaderProgram);
        glUniform1f(uProgress, progress);
        glUniform1f(uSpins, spins);

        glUniform2f(uLogoPlaneDims, logoPlaneWidth, logoPlaneHeight);
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(uTexture, 0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }

private:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint shaderProgram = 0;
    GLuint texture = 0;
    GLint uProjection = -1, uProgress = -1, uTexture = -1, uSpins = -1;
    int screenW = 0, screenH = 0;
    int indexCount = 0;
    float projection[16] = {0};
    const int tilesX = {static_cast<int>(8 + 10 * (rand() % 100) / 100.0f)};
    const int tilesY = tilesX;
    const float spins = {2.f + 3.f * (rand() % 100) / 100.0f};

    GLint uLogoPlaneDims = -1; // New uniform location
    float logoPlaneWidth = 0.0f; // Store calculated width
    float logoPlaneHeight = 0.0f; // Store calculated height

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    }

    void setupShader() {
        const char* vs = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec2 aCenter;
layout(location = 3) in float aDelay; // Used for staggered effect

uniform float animationProgress;
uniform float spins;
uniform mat4 uProjection;
uniform vec2 uLogoPlaneDims; // e.g., vec2(1.0, 1.0) if square logo

out vec2 vTexCoord;
out float vAlpha; // Pass alpha to fragment shader for fading

// Easing function for smoother transitions
float easeInOutCubic(float t) {
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

void main() {
    float easedProgress = easeInOutCubic(animationProgress);

    // --- Tunnel Effect Parameters (Adjust these values for desired look) ---
    float tunnelMaxDepth = 15.0; // How far back the tunnel goes (in Z-units)
    float tunnelMaxRadiusFactor = 3.0; // Max horizontal/vertical spread (multiplier of original dist)
    float minSpiralMagnitude = 0.001; // Avoid collapse to single point when distFromOrigin2D is tiny

    // Calculate current Z position in the tunnel
    // When easedProgress = 0, currentZ = 0 (logo at origin)
    // When easedProgress = 1, currentZ = -tunnelMaxDepth (deep into tunnel)
    float currentZ = -easedProgress * tunnelMaxDepth;

    // The spiral origin is the center of the logo/tunnel (0,0,Z)
    vec3 spiralOrigin3D = vec3(0.0, 0.0, currentZ);

    // Initial 2D position and angle of the vertex relative to the logo's center (0,0)
    vec2 vecFromOrigin2D = aPos; // aPos should be centered around (0,0) from C++
    float distFromOrigin2D = length(vecFromOrigin2D);
    float angleFromOrigin2D = atan(vecFromOrigin2D.y, vecFromOrigin2D.x);

    // Calculate the current magnitude (radius) for the spiral effect
    // When easedProgress = 0, currentSpiralMagnitude = distFromOrigin2D (original logo shape)
    // When easedProgress = 1, currentSpiralMagnitude = distFromOrigin2D * tunnelMaxRadiusFactor (expanded)
    float currentSpiralMagnitude = mix(distFromOrigin2D, distFromOrigin2D * tunnelMaxRadiusFactor, easedProgress);
    currentSpiralMagnitude = max(currentSpiralMagnitude, minSpiralMagnitude); // Ensure it doesn't collapse to zero

    // Calculate the current spiral angle based on progress and initial angle
    float currentSpiralAngle = angleFromOrigin2D + easedProgress * 6.2831853 * 2.0; // 2*PI constant

    // Calculate the spiraled XY position relative to the current tunnel Z-axis center
    vec2 spiraledXY = vec2(cos(currentSpiralAngle), sin(currentSpiralAngle)) * currentSpiralMagnitude;

    // Combine into final 3D position in object space
    // Tiles retain their size/shape by adding the vertex's offset from its tile's center (aPos - aCenter).
    vec3 finalObjectPos = spiralOrigin3D + vec3(spiraledXY, 0.0) + vec3(aPos - aCenter, 0.0);

    // Final position for projection
    gl_Position = uProjection * vec4(finalObjectPos, 1.0);

    // Pass texture coordinates to fragment shader
    vTexCoord = aTex;

    // --- Alpha for fading ---

    vAlpha = 1.0;// - easedProgress; // Linear fade out
    // You might want a different easing for alpha, or clamp it.
    // E.g., float fadeStart = 0.7; // Start fading only after 70% progress
    // if (easedProgress > fadeStart) {
    //     vAlpha = 1.0 - ((easedProgress - fadeStart) / (1.0 - fadeStart));
    // } else {
    //     vAlpha = 1.0;
    // }
    // clamp(vAlpha, 0.0, 1.0); // Ensure it's within valid range
}
)glsl";

        const char* fs = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
in float vAlpha; // New input from vertex shader

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);

    // Discard transparent pixels (optional, but good for logos with alpha)
    if (texColor.a < 0.01) {
        discard;
    }

    // Apply the alpha from the vertex shader to the final color
    FragColor = vec4(texColor.rgb, texColor.a * vAlpha);
})glsl";

        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, v);
        glAttachShader(shaderProgram, f);
        glLinkProgram(shaderProgram);
        glDeleteShader(v);
        glDeleteShader(f);

        uProjection = glGetUniformLocation(shaderProgram, "uProjection");
        uProgress = glGetUniformLocation(shaderProgram, "animationProgress");
        uTexture = glGetUniformLocation(shaderProgram, "uTexture");
        uLogoPlaneDims = glGetUniformLocation(shaderProgram, "uLogoPlaneDims");
        uSpins = glGetUniformLocation(shaderProgram, "spins");

    }

    void setupTiles(const sk_sp<SkImage>& image) {
        int imgW = image->width();
        int imgH = image->height();

        float imageAspect = static_cast<float>(imgW) / imgH;

        // Define the logo's dimensions in its object space:
        // Let the height of the logo be 2.0 units in object space, centered at 0.
        // And width scaled by its aspect ratio.
        this->logoPlaneHeight = 2.0f; // Logo occupies a height of 2 units from -1.0 to 1.0
        this->logoPlaneWidth = imageAspect * this->logoPlaneHeight; // Width adjusted for aspect ratio


        SkPixmap pixmap;
        image->peekPixels(&pixmap);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        float tileU = 1.0f / tilesX; // Width of a tile in U-coordinates (0 to 1)
        float tileV = 1.0f / tilesY; // Height of a tile in V-coordinates (0 to 1)


        for (int y = 0; y < tilesY; ++y) {
            for (int x = 0; x < tilesX; ++x) {
                float u0 = x * tileU;
                float v0 = y * tileV;
                float u1 = u0 + tileU;
                float v1 = v0 + tileV;

                // Calculate vertex positions (px, py) in the logo's object space.
                // Map U/V (0-1 range) to the logo's object space (-logoPlaneWidth/2 to logoPlaneWidth/2, etc.)
                float px0 = (u0 - 0.5f) * this->logoPlaneWidth;
                float py0 = (v0 - 0.5f) * this->logoPlaneHeight;
                float px1 = (u1 - 0.5f) * this->logoPlaneWidth;
                float py1 = (v1 - 0.5f) * this->logoPlaneHeight;

                // Calculate tile center (cx, cy) in the logo's object space.
                float cx = (u0 + tileU * 0.5f - 0.5f) * this->logoPlaneWidth;
                float cy = (v0 + tileV * 0.5f - 0.5f) * this->logoPlaneHeight;

                float delay = (float(x + y) /*+ static_cast<float>(rand() % 100) / 100.0f*/) / (tilesX + tilesY);

                unsigned int base = vertices.size() / 7;
                vertices.insert(vertices.end(), {
                        px0, py0, u0, v0, cx, cy, delay, // Bottom-left
                        px1, py0, u1, v0, cx, cy, delay, // Bottom-right
                        px1, py1, u1, v1, cx, cy, delay, // Top-right
                        px0, py1, u0, v1, cx, cy, delay  // Top-left
                });

                indices.insert(indices.end(), {
                        base + 0, base + 1, base + 2,
                        base + 2, base + 3, base + 0
                });
            }
        }
        indexCount = indices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        size_t stride = 7 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
    }
    void computeProjection() {
        float screenAspect = static_cast<float>(screenW) / screenH;

        // --- Perspective Projection Parameters ---
        float fovY_degrees = 60.0f; // Vertical Field of View in degrees. Adjust this for wider/narrower tunnel.
        float fovY_radians = fovY_degrees * (3.1415926535f / 180.0f);; // Convert to radians
        // If you don't have glm::radians, do: float fovY_radians = fovY_degrees * (3.1415926535f / 180.0f);

        float nearPlane = 0.1f;  // Near clipping plane. Objects closer than this are clipped.
        float farPlane = 100.0f; // Far clipping plane. Objects farther than this are clipped.

        // Manual Perspective Projection Matrix Calculation
        // (This is a common OpenGL-style perspective matrix)

        float f = 1.0f / tan(fovY_radians / 2.0f); // 'f' is effectively 1/tan(half_FOV)

        // Column-major order (as OpenGL expects for glUniformMatrix4fv)
        projection[0] = f / screenAspect;   projection[1] = 0.0f;             projection[2] = 0.0f;                      projection[3] = 0.0f;
        projection[4] = 0.0f;               projection[5] = f;                projection[6] = 0.0f;                      projection[7] = 0.0f;
        projection[8] = 0.0f;               projection[9] = 0.0f;             projection[10] = (farPlane + nearPlane) / (nearPlane - farPlane); projection[11] = -1.0f;
        projection[12] = 0.0f;              projection[13] = 0.0f;             projection[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane); projection[15] = 0.0f;

        // Note on GLM: If you had GLM, you'd just do:
        // glm::mat4 proj = glm::perspective(fovY_radians, screenAspect, nearPlane, farPlane);
        // memcpy(projection, glm::value_ptr(proj), sizeof(float) * 16);
    }
};
#include <string>
#include <vector>
#include <map>
#include <cmath>     // For std::cos, std::sin
#include <iostream>  // For std::cerr (error output)
#include <cstddef>   // For offsetof

// --- Glyph Data Structure ---
// This struct holds the necessary information for a single character (glyph)
// extracted from a font file (typically using a library like FreeType).
struct GlyphInfo {
    float uv[4];     // Normalized texture coordinates (u0, v0, u1, v1) within the font atlas.
    // uv[0]=u_min, uv[1]=v_min, uv[2]=u_max, uv[3]=v_max
    float width;     // Width of the glyph's bitmap in pixels.
    float height;    // Height of the glyph's bitmap in pixels.
    float xOffset;   // Horizontal offset from the cursor position to the glyph's left edge.
    float yOffset;   // Vertical offset from the cursor position (baseline) to the glyph's top edge.
    float xAdvance;  // How much to advance the cursor horizontally for the next character.
};

// --- Vertex Data Structure for a Character Quad ---
// Each character will be rendered as two triangles (a quad).
// Each vertex of this quad needs a position and texture coordinates.
struct FontVertex {
    float x, y; // Position (in pixel coordinates, which will be transformed by the projection matrix)
    float u, v; // Texture coordinates (normalized, from 0.0 to 1.0)
};


// The FontRenderer class encapsulates all OpenGL-related logic for drawing text.
class FontRenderer {
public:
    // --- Constructor ---
    FontRenderer()
            : shaderProgram(0), vao(0), vbo(0), fontAtlasTexture(0), atlasW(0), atlasH(0),
              uProjectionLoc(-1), uTextureLoc(-1), uTextColorLoc(-1) {
        // Initialize projection matrix to identity or zeros
        for(int i = 0; i < 16; ++i) projectionMatrix[i] = 0.0f;
    }

    // --- Destructor ---
    // Cleans up OpenGL resources when the FontRenderer object is destroyed.
    ~FontRenderer() {
        if (shaderProgram) glDeleteProgram(shaderProgram);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        // Note: fontAtlasTexture is NOT deleted here. It's expected to be managed
        // externally, as it's passed in during init(). If FontRenderer *created*
        // the texture, it should then be responsible for deleting it.
    }

    // --- Initialization Method ---
    // Call this once after your OpenGL context has been created and is current.
    //
    // Parameters:
    //   fontAtlasTextureId: The OpenGL texture ID (GLuint) of your pre-generated font atlas.
    //   atlasWidth, atlasHeight: The dimensions of your font atlas texture in pixels.
    //   glyphsData: A map containing GlyphInfo for each character you want to render.
    //               This data is typically generated by a font parsing library (e.g., FreeType).
    void init(GLuint fontAtlasTextureId, int atlasWidth, int atlasHeight, const std::map<char, GlyphInfo>& glyphsData) {
        fontAtlasTexture = fontAtlasTextureId;
        atlasW = atlasWidth;
        atlasH = atlasHeight;
        glyphs = glyphsData; // Copy glyph data for internal use

        // --- Shader Source Code ---
        // Vertex Shader: Transforms vertex positions and passes texture coordinates to the fragment shader.
        const char* VERTEX_SHADER_SRC = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;      // Vertex position (x, y)
layout(location = 1) in vec2 aTexCoord; // Texture coordinates (u, v)

uniform mat4 uProjection;               // Orthographic projection matrix

out vec2 vTexCoord;                     // Output texture coordinates to fragment shader

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0); // Z is 0.0 for 2D text
    vTexCoord = aTexCoord;
}
)glsl";

        // Fragment Shader: Samples the font atlas texture and applies the text color.
        // It assumes the font atlas contains grayscale glyphs where the red channel
        // holds the alpha (opacity) information.
        const char* FRAGMENT_SHADER_SRC = R"glsl(#version 300 es
precision mediump float; // Medium precision for floats

in vec2 vTexCoord;             // Interpolated texture coordinates from vertex shader
uniform sampler2D uTexture;    // The font atlas texture
uniform vec4 uTextColor;       // Uniform color for the text (RGBA)

out vec4 FragColor;            // Final fragment color output

void main() {
    // Sample the red channel of the texture. This assumes grayscale glyphs
    // where the intensity (red channel) directly corresponds to alpha.
    float alpha = texture(uTexture, vTexCoord).r;

    // Discard fragments that are almost completely transparent to avoid rendering wasted pixels.
    if (alpha < 0.001) {
        discard;
    }

    // Multiply the text color's alpha by the sampled glyph alpha.
    // The RGB components come directly from uTextColor.
    FragColor = vec4(uTextColor.rgb, uTextColor.a * alpha);
}
)glsl";

        // --- OpenGL Setup: Compile Shaders and Create Program ---
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
        shaderProgram = createProgram(vertexShader, fragmentShader);
        // Delete shader objects once linked into the program (they are no longer needed).
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // --- Get Uniform Locations ---
        // Store the locations of shader uniforms so we can update them later.
        uProjectionLoc = glGetUniformLocation(shaderProgram, "uProjection");
        uTextureLoc = glGetUniformLocation(shaderProgram, "uTexture");
        uTextColorLoc = glGetUniformLocation(shaderProgram, "uTextColor");

        if (uProjectionLoc == -1 || uTextureLoc == -1 || uTextColorLoc == -1) {
            std::cerr << "Warning: One or more font shader uniforms not found (uProjection, uTexture, uTextColor)!" << std::endl;
        }

        // --- Setup VAO and VBO ---
        // VAO (Vertex Array Object): Stores configuration for vertex attributes.
        // VBO (Vertex Buffer Object): Stores the actual vertex data.
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        // Bind the VAO first, then bind and configure its associated VBO.
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Define vertex attributes for position (location 0) and texture coordinates (location 1).
        // The last parameter `(void*)offsetof(FontVertex, member)` calculates the byte offset
        // of a member within the struct, allowing OpenGL to correctly interpret the data layout.
        // Position (aPos: 2 floats)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (void*)offsetof(FontVertex, x));
        glEnableVertexAttribArray(0);
        // Texture coordinates (aTexCoord: 2 floats)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (void*)offsetof(FontVertex, u));
        glEnableVertexAttribArray(1);

        // Unbind the VAO and VBO to avoid accidental modification.
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // --- OpenGL State Configuration for Text Rendering ---
        // Enable blending for transparency (required for text with alpha).
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Standard alpha blending.
        // Disable depth testing, as 2D text is usually drawn as an overlay.
        glDisable(GL_DEPTH_TEST);
    }

    // --- Rendering Method ---
    // Renders a string of text to the screen.
    //
    // Parameters:
    //   text: The string (word or phrase) to render.
    //   x, y: The top-left pixel coordinates where the text should start.
    //   scale: A scaling factor applied to the font. 1.0 uses the original glyph size.
    //   color: An array of 4 floats (RGBA) representing the desired text color.
    void renderText(const std::string& text, float x, float y, float scale, const float color[4]) {
        // Basic checks to ensure the renderer is initialized and has glyph data.
        if (!shaderProgram || !vao || !vbo || glyphs.empty()) {
            std::cerr << "Error: FontRenderer not initialized or glyph data missing. Cannot render text." << std::endl;
            return;
        }

        // Get current framebuffer dimensions from the viewport.
        // This is used to set up the orthographic projection matrix correctly.
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        float screenW = static_cast<float>(viewport[2]);
        float screenH = static_cast<float>(viewport[3]);

        // Update the orthographic projection matrix based on current screen dimensions.
        updateOrthographicProjection(screenW, screenH);

        // Activate the shader program and set its uniforms.
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projectionMatrix);
        glUniform1i(uTextureLoc, 0); // Tell shader uTexture corresponds to GL_TEXTURE0
        glUniform4fv(uTextColorLoc, 1, color); // Set the text color

        // Bind the font atlas texture to texture unit 0.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontAtlasTexture);

        // Bind the VAO and VBO before updating and drawing vertex data.
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        std::vector<FontVertex> vertices; // Temporary storage for current text's vertices
        float currentX = x; // Current cursor position for drawing glyphs
        float currentY = y; // Current cursor position for drawing glyphs (assumed to be top of text box)

        // Iterate through each character in the input string
        for (char c : text) {
            auto it = glyphs.find(c);
            if (it == glyphs.end()) {
                // If glyph data for this character is not found, skip it.
                // You could add a placeholder character or log a warning here.
                continue;
            }
            const GlyphInfo& glyph = it->second;

            // Calculate the position and dimensions of the character's quad on screen.
            // glyph.xOffset and glyph.yOffset adjust the glyph's bitmap relative to the cursor.
            float xpos = currentX + glyph.xOffset * scale;
            // The ypos calculation depends on how your yOffset is defined.
            // If yOffset is from baseline to top of glyph (common FreeType behavior),
            // then `ypos = currentY - glyph.yOffset * scale;` places the top correctly.
            // If yOffset is already from top-left of line to top-left of glyph, then simpler.
            // Assuming ypos is the top edge of the character's quad.
            float ypos = currentY + glyph.yOffset * scale; // Assuming yOffset is from top of line to top of glyph

            float w = glyph.width * scale;  // Scaled width
            float h = glyph.height * scale; // Scaled height

            // Add the 6 vertices (two triangles) for the character's quad to the vector.
            // The winding order matters for culling (if enabled), typically counter-clockwise.
            //
            // Quad corners:
            // (xpos, ypos) -------- (xpos+w, ypos)      (TL) ------ (TR)
            //   |                        |                |            |
            // (xpos, ypos+h) ----- (xpos+w, ypos+h)      (BL) ------ (BR)
            //
            // Texture coordinates (assuming standard image origin top-left, v increases downwards)
            // UV_TL = (glyph.uv[0], glyph.uv[1])
            // UV_TR = (glyph.uv[2], glyph.uv[1])
            // UV_BL = (glyph.uv[0], glyph.uv[3])
            // UV_BR = (glyph.uv[2], glyph.uv[3])

            // Triangle 1: Top-Left, Bottom-Left, Bottom-Right
            vertices.push_back({xpos,     ypos,     glyph.uv[0], glyph.uv[1]}); // TL
            vertices.push_back({xpos,     ypos + h, glyph.uv[0], glyph.uv[3]}); // BL
            vertices.push_back({xpos + w, ypos + h, glyph.uv[2], glyph.uv[3]}); // BR

            // Triangle 2: Top-Left, Bottom-Right, Top-Right
            vertices.push_back({xpos,     ypos,     glyph.uv[0], glyph.uv[1]}); // TL
            vertices.push_back({xpos + w, ypos + h, glyph.uv[2], glyph.uv[3]}); // BR
            vertices.push_back({xpos + w, ypos,     glyph.uv[2], glyph.uv[1]}); // TR

            // Advance the cursor for the next character based on xAdvance.
            currentX += glyph.xAdvance * scale;
        }

        // If there are vertices to draw (i.e., the string was not empty or all chars skipped)
        if (!vertices.empty()) {
            // Upload the generated vertex data to the VBO.
            // GL_DYNAMIC_DRAW indicates data will change frequently.
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(FontVertex), vertices.data(), GL_DYNAMIC_DRAW);
            // Draw all the character quads as individual triangles.
            glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        }

        // Unbind OpenGL objects to leave a clean state.
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

private:
    // --- Private Member Variables ---
    GLuint shaderProgram;       // OpenGL shader program ID
    GLuint vao;                 // Vertex Array Object ID
    GLuint vbo;                 // Vertex Buffer Object ID
    GLuint fontAtlasTexture;    // ID of the font atlas texture
    int atlasW, atlasH;         // Dimensions of the font atlas texture

    GLint uProjectionLoc;       // Location of the projection matrix uniform
    GLint uTextureLoc;          // Location of the texture sampler uniform
    GLint uTextColorLoc;        // Location of the text color uniform
    float projectionMatrix[16]; // Storage for the orthographic projection matrix

    std::map<char, GlyphInfo> glyphs; // Map storing GlyphInfo for each character

    // --- Private Static Helper Functions (for OpenGL setup and error checking) ---
    // These are static because they don't depend on any specific instance of FontRenderer.
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
            std::cerr << "ERROR::SHADER::" << name << "::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
    }

    static void checkProgramError(GLuint program, const char* name) {
        GLint success;
        GLchar infoLog[1024];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM::" << name << "::LINKING_FAILED\n" << infoLog << std::endl;
        }
    }

    // --- Private Member Function ---
    // Updates the orthographic projection matrix.
    // This matrix maps pixel coordinates (from 0 to screenWidth/screenHeight)
    // into OpenGL's Normalized Device Coordinates (-1.0 to 1.0).
    void updateOrthographicProjection(float screenWidth, float screenHeight) {
        float L = 0.0f;          // Left edge of the projection volume
        float R = screenWidth;   // Right edge
        float B = screenHeight;  // Bottom edge (assuming Y increases downwards for screen coords)
        float T = 0.0f;          // Top edge
        float N = -1.0f;         // Near clipping plane (for Z)
        float F = 1.0f;          // Far clipping plane (for Z)

        // Populate the 4x4 projection matrix in column-major order (as OpenGL expects).
        // This is a standard formula for an orthographic projection matrix.
        projectionMatrix[0] = 2.0f / (R - L);   projectionMatrix[1] = 0.0f;             projectionMatrix[2] = 0.0f;              projectionMatrix[3] = 0.0f;
        projectionMatrix[4] = 0.0f;             projectionMatrix[5] = 2.0f / (T - B);   projectionMatrix[6] = 0.0f;              projectionMatrix[7] = 0.0f;
        projectionMatrix[8] = 0.0f;             projectionMatrix[9] = 0.0f;             projectionMatrix[10] = -2.0f / (F - N);  projectionMatrix[11] = 0.0f;
        projectionMatrix[12] = -(R + L) / (R - L); projectionMatrix[13] = -(T + B) / (T - B); projectionMatrix[14] = -(F + N) / (F - N); projectionMatrix[15] = 1.0f;
    }
};
/*
#ifndef FREETYPE_LOADER_H
#define FREETYPE_LOADER_H

// Core FreeType headers
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H // For FT_Glyph_Metrics

// OpenGL headers (make sure this matches your environment, GLES3 for Android/iOS)
#include <GLES3/gl3.h>

#include <string>
#include <vector>
#include <map>
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For error output

// Re-include GlyphInfo struct from FontRenderer.h if not already included
// Assuming FontRenderer.h is included where this class is used, but for standalone example:
// #ifndef GLYPH_INFO_STRUCT_DEFINED
// #define GLYPH_INFO_STRUCT_DEFINED
struct GlyphInfo {
    float uv[4];     // normalized texture coordinates (x0, y0, x1, y1)
    float width;     // width of the glyph bitmap
    float height;    // height of the glyph bitmap
    float xOffset;   // x offset from cursor to glyph's top-left
    float yOffset;   // y offset from cursor to glyph's top-left (from baseline to top)
    float xAdvance;  // x advance for next cursor position
};
// #endif // GLYPH_INFO_STRUCT_DEFINED

// Structure to hold the output data
struct FontData {
    GLuint fontAtlasTextureId = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;
    std::map<char, GlyphInfo> glyphs;
};

class FreeTypeLoader {
public:
    FreeTypeLoader() : ftLibrary(nullptr), ftFace(nullptr) {}

    ~FreeTypeLoader() {
        if (ftFace) {
            FT_Done_Face(ftFace);
        }
        if (ftLibrary) {
            FT_Done_FreeType(ftLibrary);
        }
    }
   // NEW loadFont signature: Accepts font data in memory (char* or unsigned char*) and its size.
    FontData loadFont(const unsigned char* fontData, long dataSize, int pixelHeight, const std::string& charsToLoad = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~") {
        FontData data;
        FT_Error error;

        // 1. Initialize FreeType library
        error = FT_Init_FreeType(&ftLibrary);
        if (error) {
            throw std::runtime_error("Could not init FreeType library: " + std::to_string(error));
        }

        // 2. Load font face from memory instead of file path
        // FT_New_Memory_Face takes FT_Byte* for font_base and FT_Long for font_size
        error = FT_New_Memory_Face(ftLibrary, fontData, dataSize, 0, &ftFace);
        if (error == FT_Err_Unknown_File_Format) {
            throw std::runtime_error("Unsupported font format for in-memory data.");
        } else if (error) {
            throw std::runtime_error("Could not load font from memory: " + std::to_string(error));
        }

        // (The rest of the loadFont method remains the same as before)
        // 3. Set font pixel size
        error = FT_Set_Pixel_Sizes(ftFace, 0, pixelHeight);
        if (error) {
            throw std::runtime_error("Could not set font pixel size: " + std::to_string(error));
        }

        // --- Calculate Atlas Dimensions & Collect Glyph Data ---
        int atlasMaxDim = 1024;
        int currentX = 1;
        int currentY = 1;
        int rowHeight = 0;

        std::vector<unsigned char> atlasPixels(atlasMaxDim * atlasMaxDim, 0);

        if (pixelHeight > atlasMaxDim / 4) {
            atlasMaxDim = pixelHeight * 16;
            if (atlasMaxDim < 1024) atlasMaxDim = 1024;
            atlasPixels.assign(atlasMaxDim * atlasMaxDim, 0);
        }

        for (char c : charsToLoad) {
            error = FT_Load_Char(ftFace, c, FT_LOAD_RENDER);
            if (error) {
                std::cerr << "Warning: Could not load glyph for char '" << c << "': " << error << std::endl;
                continue;
            }

            FT_GlyphSlot glyph = ftFace->glyph;
            FT_Bitmap bitmap = glyph->bitmap;

            if (currentX + bitmap.width + 1 >= atlasMaxDim) {
                currentX = 1;
                currentY += rowHeight + 1;
                rowHeight = 0;
            }

            if (currentY + bitmap.rows + 1 >= atlasMaxDim) {
                std::cerr << "Error: Font atlas size " << atlasMaxDim << "x" << atlasMaxDim
                          << " too small for all characters. Consider increasing atlasMaxDim." << std::endl;
                break;
            }

            for (unsigned int y = 0; y < bitmap.rows; ++y) {
                for (unsigned int x = 0; x < bitmap.width; ++x) {
                    atlasPixels[(currentY + y) * atlasMaxDim + (currentX + x)] = bitmap.buffer[y * bitmap.width + x];
                }
            }

            GlyphInfo gi;
            gi.uv[0] = static_cast<float>(currentX) / atlasMaxDim;
            gi.uv[1] = static_cast<float>(currentY) / atlasMaxDim;
            gi.uv[2] = static_cast<float>(currentX + bitmap.width) / atlasMaxDim;
            gi.uv[3] = static_cast<float>(currentY + bitmap.rows) / atlasMaxDim;

            gi.width = static_cast<float>(bitmap.width);
            gi.height = static_cast<float>(bitmap.rows);

            gi.xOffset = static_cast<float>(glyph->bitmap_left);
            gi.yOffset = static_cast<float>(pixelHeight - glyph->bitmap_top);

            gi.xAdvance = static_cast<float>(glyph->advance.x >> 6);

            data.glyphs[c] = gi;

            currentX += bitmap.width + 1;
            rowHeight = std::max(rowHeight, static_cast<int>(bitmap.rows));
        }

        data.atlasWidth = atlasMaxDim;
        data.atlasHeight = atlasMaxDim;

        // 4. Create OpenGL Texture and Upload Data
        glGenTextures(1, &data.fontAtlasTextureId);
        glBindTexture(GL_TEXTURE_2D, data.fontAtlasTextureId);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, data.atlasWidth, data.atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasPixels.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        glBindTexture(GL_TEXTURE_2D, 0);

        return data;
    }

    // Loads a font and generates the OpenGL texture atlas and glyph data.
    // fontFilePath: Path to your .ttf or .otf font file.
    // pixelHeight: The desired pixel height for the font (e.g., 48 for a 48px font).
    // charsToLoad: A string containing all characters you want to load (e.g., " !\"#$%...~").
    //              It's good practice to iterate through common ASCII characters.
    FontData loadFont(const std::string& fontFilePath, int pixelHeight, const std::string& charsToLoad = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~") {
        FontData data;
        FT_Error error;

        // 1. Initialize FreeType library
        error = FT_Init_FreeType(&ftLibrary);
        if (error) {
            throw std::runtime_error("Could not init FreeType library: " + std::to_string(error));
        }

        // 2. Load font face
        error = FT_New_Face(ftLibrary, fontFilePath.c_str(), 0, &ftFace);
        if (error == FT_Err_Unknown_File_Format) {
            throw std::runtime_error("Unsupported font format for: " + fontFilePath);
        } else if (error) {
            throw std::runtime_error("Could not open font file: " + fontFilePath + " error: " + std::to_string(error));
        }

        // 3. Set font pixel size
        error = FT_Set_Pixel_Sizes(ftFace, 0, pixelHeight);
        if (error) {
            throw std::runtime_error("Could not set font pixel size: " + std::to_string(error));
        }

        // --- Calculate Atlas Dimensions & Collect Glyph Data ---
        // This is a simple packing algorithm (row-by-row). More advanced algorithms
        // exist (e.g., max-rects, binary tree) for better space utilization.
        int atlasMaxDim = 1024; // Start with a reasonable max dimension for the atlas
        int currentX = 1; // Start from 1 to give a small border
        int currentY = 1;
        int rowHeight = 0;

        // Store pixel data for the atlas
        std::vector<unsigned char> atlasPixels(atlasMaxDim * atlasMaxDim, 0); // Initialize with 0 (transparent/black)

        // Adjust for max dimension based on actual usage, in case 1024 is too small for all glyphs
        // This is a crude adjustment, a proper packer would resize or fail gracefully.
        if (pixelHeight > atlasMaxDim / 4) { // If a single character takes too much space, increase atlas size
            atlasMaxDim = pixelHeight * 16; // Heuristic, might need adjustment
            if (atlasMaxDim < 1024) atlasMaxDim = 1024; // Minimum reasonable size
            atlasPixels.assign(atlasMaxDim * atlasMaxDim, 0); // Resize and clear
        }


        // Iterate through characters to load glyphs and pack them
        for (char c : charsToLoad) {
            // Load character glyph
            error = FT_Load_Char(ftFace, c, FT_LOAD_RENDER); // FT_LOAD_RENDER renders the glyph to a bitmap
            if (error) {
                std::cerr << "Warning: Could not load glyph for char '" << c << "': " << error << std::endl;
                continue;
            }

            FT_GlyphSlot glyph = ftFace->glyph;
            FT_Bitmap bitmap = glyph->bitmap;

            // Check if character bitmap fits in current row, if not, move to next row
            if (currentX + bitmap.width + 1 >= atlasMaxDim) {
                currentX = 1; // Reset X to left
                currentY += rowHeight + 1; // Move Y down by max row height + border
                rowHeight = 0; // Reset row height
            }

            // Check if we ran out of space in the atlas
            if (currentY + bitmap.rows + 1 >= atlasMaxDim) {
                std::cerr << "Error: Font atlas size " << atlasMaxDim << "x" << atlasMaxDim
                          << " too small for all characters. Consider increasing atlasMaxDim." << std::endl;
                // You might throw an exception here or try to resize the atlas and re-pack.
                // For this example, we'll stop packing and proceed with partial data.
                break;
            }

            // Copy glyph bitmap to atlas texture data
            for (unsigned int y = 0; y < bitmap.rows; ++y) {
                for (unsigned int x = 0; x < bitmap.width; ++x) {
                    atlasPixels[(currentY + y) * atlasMaxDim + (currentX + x)] = bitmap.buffer[y * bitmap.width + x];
                }
            }

            // Store GlyphInfo
            GlyphInfo gi;
            // UV coordinates (normalized)
            gi.uv[0] = static_cast<float>(currentX) / atlasMaxDim;               // u_min
            gi.uv[1] = static_cast<float>(currentY) / atlasMaxDim;               // v_min
            gi.uv[2] = static_cast<float>(currentX + bitmap.width) / atlasMaxDim; // u_max
            gi.uv[3] = static_cast<float>(currentY + bitmap.rows) / atlasMaxDim; // v_max

            // Glyph dimensions in pixels
            gi.width = static_cast<float>(bitmap.width);
            gi.height = static_cast<float>(bitmap.rows);

            // Offsets from FreeType metrics
            // bitmap_left is the horizontal distance from the current cursor position to the left border of the glyph bitmap.
            gi.xOffset = static_cast<float>(glyph->bitmap_left);
            // bitmap_top is the vertical distance from the horizontal baseline to the top border of the glyph bitmap.
            // Note: In OpenGL texture coordinates, Y increases downwards, so we typically want this to represent
            // the distance from the *top of the line* to the *top of the glyph*.
            // A common heuristic is (pixelHeight - glyph->bitmap_top).
            gi.yOffset = static_cast<float>(pixelHeight - glyph->bitmap_top); // Adjust for top-origin

            // Advance for next character (in 26.6 fixed-point format, convert to float)
            gi.xAdvance = static_cast<float>(glyph->advance.x >> 6); // >> 6 converts from 26.6 fixed-point to pixels

            data.glyphs[c] = gi;

            // Update currentX and rowHeight for packing
            currentX += bitmap.width + 1; // Add 1px border
            rowHeight = std::max(rowHeight, static_cast<int>(bitmap.rows));
        }

        // Update atlas dimensions in FontData struct (to actual used dimensions, or max dim)
        data.atlasWidth = atlasMaxDim;
        data.atlasHeight = atlasMaxDim; // Assuming square atlas for simplicity

        // 4. Create OpenGL Texture and Upload Data
        glGenTextures(1, &data.fontAtlasTextureId);
        glBindTexture(GL_TEXTURE_2D, data.fontAtlasTextureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Use linear filtering for smooth scaling, especially important for bitmap fonts
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload the pixel data.
        // GL_R8 is a single-channel 8-bit red component texture. This is good for alpha masks.
        // For OpenGL ES, GL_ALPHA might also be used in older versions, but GL_RED is more standard now.
        // The format should match bitmap.pixel_mode (usually FT_PIXEL_MODE_GRAY for single channel).
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction, very common for font bitmaps
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, data.atlasWidth, data.atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasPixels.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // Re-enable default byte-alignment

        glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture

        // FreeType resources are automatically cleaned up by destructor when `ftLibrary` and `ftFace` go out of scope.
        // Or you can explicitly call FT_Done_Face(ftFace); FT_Done_FreeType(ftLibrary); here if not in a class destructor.

        return data;
    }

private:
    FT_Library ftLibrary;
    FT_Face ftFace;
};
// Global or member variables
FontRenderer* myFontRenderer = nullptr;
FontData loadedFontData; // Structure to hold the font atlas and glyphs

// In your application's OpenGL initialization function (after context creation):
void setupRendering() {
    // ... (other OpenGL setup) ...

    FreeTypeLoader fontLoader;
    try {
        // Adjust "path/to/your/font.ttf" to your actual font file path.
        // You can download free fonts from Google Fonts or similar.
        // Example characters to load: typical ASCII printable characters.
        loadedFontData = fontLoader.loadFont("path/to/your/font.ttf", 48,
                                              " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");

        myFontRenderer = new FontRenderer();
        myFontRenderer->init(loadedFontData.fontAtlasTextureId,
                             loadedFontData.atlasWidth,
                             loadedFontData.fontAtlasTextureId, // Typo in original `init`? Should be atlasHeight.
                             loadedFontData.glyphs);
        // Corrected:
        myFontRenderer->init(loadedFontData.fontAtlasTextureId,
                             loadedFontData.atlasWidth,
                             loadedFontData.atlasHeight, // This should be atlasHeight
                             loadedFontData.glyphs);


        std::cout << "Font loaded successfully! Atlas size: "
                  << loadedFontData.atlasWidth << "x" << loadedFontData.atlasHeight << std::endl;

    } catch (const std::runtime_error& e) {
        std::cerr << "Failed to load font: " << e.what() << std::endl;
        // Handle error, maybe exit or use a fallback.
    }
}

// In your application's rendering loop:
void renderFrame() {
    // ... (clear screen) ...

    if (myFontRenderer) {
        float textColor[4] = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow
        myFontRenderer->renderText("Hello FreeType!", 50.0f, 50.0f, 1.0f, textColor);

        float blueColor[4] = {0.2f, 0.2f, 1.0f, 1.0f}; // Blue
        myFontRenderer->renderText("This is scaled text!", 50.0f, 100.0f, 1.5f, blueColor);

        float redColor[4] = {1.0f, 0.2f, 0.2f, 1.0f}; // Red
        myFontRenderer->renderText("Another line. 12345", 50.0f, 150.0f, 0.8f, redColor);
    }

    // ... (swap buffers) ...
}

// In your application's cleanup/shutdown:
void cleanup() {
    if (myFontRenderer) {
        delete myFontRenderer;
        myFontRenderer = nullptr;
    }
    // Note: The OpenGL texture ID (loadedFontData.fontAtlasTextureId) should be deleted
    // when you're sure it's no longer needed, typically after myFontRenderer is deleted.
    if (loadedFontData.fontAtlasTextureId != 0) {
        glDeleteTextures(1, &loadedFontData.fontAtlasTextureId);
        loadedFontData.fontAtlasTextureId = 0;
    }
}
#endif // FREETYPE_LOADER_H
 */

LogoExplosionEffect::LogoExplosionEffect(sk_sp<SkImage> image, int screenWidth, int screenHeight)
        : screenW(screenWidth), screenH(screenHeight) {
    initShader();
    uploadTexture(std::move(image));
    createTiledGeometry();
}

LogoExplosionEffect::~LogoExplosionEffect() {
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

void LogoExplosionEffect::render(float timeSeconds) {
    glUseProgram(shaderProgram);
    float proj[16];
    computeProjection(proj);
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, proj);
    glUniform1f(uniforms.time, timeSeconds);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(uniforms.texture, 0);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, tileCountX * tileCountY * 6, GL_UNSIGNED_INT, 0);
}
// Compile & link helper
GLuint tsl::graphics::compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    return success ? shader : 0;
}

GLuint tsl::graphics::createProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    return program;
}
void tsl::graphics::checkShaderError(GLuint shader, GLenum type, const char* name) {
    GLint success;
    GLchar infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        LOGE("ERROR::PROGRAM::%s::LINKING_FAILED %s", name,infoLog);
    }
}

void tsl::graphics::checkProgramError(GLuint program, const char* name) {
    GLint success;
    GLchar infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        LOGE("ERROR::PROGRAM::%s::LINKING_FAILED %s", name,infoLog);
    }
}




void LogoExplosionEffect::initShader() {
    shaderProgram = createProgram(vertexShaderSrc(), fragmentShaderSrc());
    GLint success;
    glGetShaderiv(shaderProgram, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shaderProgram, 512, nullptr, log);
        LOGE("Shader Compile Error: %s", log);
    }
    uniforms.projection = glGetUniformLocation(shaderProgram, "projection");
    uniforms.time = glGetUniformLocation(shaderProgram, "time");
    uniforms.texture = glGetUniformLocation(shaderProgram, "ourTexture");
}

void LogoExplosionEffect::uploadTexture(const sk_sp<SkImage>& image) {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    SkPixmap pixmap;
    image->peekPixels(&pixmap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap.width(), pixmap.height(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixmap.addr());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void LogoExplosionEffect::createTiledGeometry() {
    const float dx = 1.0f / tileCountX;
    const float dy = 1.0f / tileCountY;
    std::vector<float> verts;
    std::vector<int> indices;

    for (int y = 0; y < tileCountY; ++y) {
        for (int x = 0; x < tileCountX; ++x) {
            float x0 = -1.0f + 2.0f * x * dx;
            float y0 = -1.0f + 2.0f * y * dy;
            float x1 = -1.0f + 2.0f * (x + 1) * dx;
            float y1 = -1.0f + 2.0f * (y + 1) * dy;
            float u0 = x * dx;
            float v0 = y * dy;
            float u1 = (x + 1) * dx;
            float v1 = (y + 1) * dy;

            int base = verts.size() / 5;
            verts.insert(verts.end(), {
                    x0, y0, u0, v1,
                    x1, y0, u1, v1,
                    x1, y1, u1, v0,
                    x0, y1, u0, v0
            });
            indices.insert(indices.end(), {
                    base, base+1, base+2,
                    base+2, base+3, base
            });
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void LogoExplosionEffect::computeProjection(float* m) const {
    float aspect = (float)screenW / screenH;
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f / aspect;
    m[5] = 1.0f;
    m[10] = -1.0f;
    m[15] = 1.0f;
}

// ---------------- Shader Sources ----------------

const char* LogoExplosionEffect::vertexShaderSrc() {
    return R"glsl(#version 300 es
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        uniform mat4 projection;
        out vec2 vTexCoord;
        void main() {
            vTexCoord = aTexCoord;
            gl_Position = projection * vec4(aPos, 0.0, 1.0);
        }
    )glsl";
}

const char* LogoExplosionEffect::fragmentShaderSrc() {
    return R"glsl(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform sampler2D ourTexture;
        uniform float time;

        void main() {
            vec2 dir = normalize(vTexCoord - 0.5);
            float dist = length(vTexCoord - 0.5);
            float explode = clamp(time - 1.0, 0.0, 1.0);
            float move = explode * 2.0 * dist;
            vec2 uv = vTexCoord + dir * move;

            // Rotate tile
            float angle = explode * dist * 6.28;
            float c = cos(angle);
            float s = sin(angle);
            uv = mat2(c, -s, s, c) * (uv - 0.5) + 0.5;

            if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) discard;
            FragColor = texture(ourTexture, uv);
        }
    )glsl";
}

    LogoSwirler::LogoSwirler(const sk_sp<SkImage>& image, int screenWidth, int screenHeight)
            : screenW(screenWidth), screenH(screenHeight) {
        setupShader();
        setupTiles(image);
        computeProjection();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

LogoSwirler::~LogoSwirler() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
        if (texture) glDeleteTextures(1, &texture);
        if (shaderProgram) glDeleteProgram(shaderProgram);
    }

    void LogoSwirler::render(float progress) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniform1f(uProgress, progress);
        glUniform1f(uSpins, spins);

        glUniform2f(uLogoPlaneDims, logoPlaneWidth, logoPlaneHeight);
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(uTexture, 0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }

    void LogoSwirler::setupShader() {
        const char* vs = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec2 aCenter;
layout(location = 3) in float aDelay;

uniform float animationProgress;
uniform float spins;
uniform mat4 uProjection;
uniform vec2 uLogoPlaneDims; // e.g., vec2(1.0, 1.0) if square logo

out vec2 vTexCoord;
out float vDelay;
out float vProgress;

float easeOutCubic(float t) {
    return 1.0 - pow(1.0 - t, 3.0);
}
float easeOutBack(float t) {
    float s = 1.70158;
    return 1.0 + (--t) * t * ((s + 1.0) * t + s);
}

void main() {
    vDelay = aDelay;
    vProgress = animationProgress;
    float prog = clamp(animationProgress - aDelay, 0.0, 1.0);
    float eased = easeOutCubic(prog);

    // Spiral outward from original position
    vec2 fromCenter = aCenter;
    float dist = length(fromCenter);
    float angle = atan(fromCenter.y, fromCenter.x);

    float spiralRadius = prog  * 1.5;
    float spiralAngle = angle + eased * 6.2831 * spins;

    vec2 spiralOffset = vec2(cos(spiralAngle), sin(spiralAngle)) * spiralRadius;
    vec2 finalPos = aCenter + spiralOffset + (aPos - aCenter);

    // Moves from z = -2.0 toward z = 0.0
    float zDepth = -2.0 + 2.0 * eased;
    gl_Position = uProjection * vec4(finalPos, zDepth, 1.0);

    vTexCoord = aTex;
}


        )glsl";

        const char* fs = R"glsl(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        in float vDelay;
        in float vProgress;
        uniform sampler2D uTexture;
        uniform float animationProgress;
        out vec4 FragColor;
        void main() {
float fade = smoothstep(0.0, 1.0, clamp((vProgress - vDelay) * 2.0, 0.0, 1.0));
vec4 color = texture(uTexture, vTexCoord);
            if(color.a < 0.1) discard;
FragColor = vec4(color.rgb, color.a * (1.0 - fade));
        }
        )glsl";

        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, v);
        glAttachShader(shaderProgram, f);
        glLinkProgram(shaderProgram);
        glDeleteShader(v);
        glDeleteShader(f);

        uProjection = glGetUniformLocation(shaderProgram, "uProjection");
        uProgress = glGetUniformLocation(shaderProgram, "animationProgress");
        uTexture = glGetUniformLocation(shaderProgram, "uTexture");
        uLogoPlaneDims = glGetUniformLocation(shaderProgram, "uLogoPlaneDims");
        uSpins = glGetUniformLocation(shaderProgram, "spins");

    }

    void LogoSwirler::setupTiles(const sk_sp<SkImage>& image) {
        int imgW = image->width();
        int imgH = image->height();

        // Calculate aspect ratio of the image
        float imageAspect = static_cast<float>(imgW) / imgH;

        SkPixmap pixmap;
        image->peekPixels(&pixmap);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        float tileW = 1.0f / tilesX;
        float tileH = 1.0f / tilesY;

        // Determine the scaling factors for the logo to fit within the screen while maintaining aspect ratio
        // We want the logo to be centered and fit the screen without distortion.
        // Let's make the logo's height fill the normalized [-1, 1] range, and adjust width based on image aspect.
        // Then scale these to fit the screen's aspect ratio via the projection matrix.
        float logoPlaneWidth = imageAspect;
        float logoPlaneHeight = 1.0f; // Standardized to 1 unit in the local space for height

        // Adjust these if the image is wider than the screen aspect ratio, to fit width instead of height
        float screenAspect = static_cast<float>(screenW) / screenH;
        // Fit inside screen (no cropping)
        float scale;
        if (screenAspect > imageAspect) {
            // Screen is wider → limit by height
            scale = 1.0f;
        } else {
            // Screen is taller → limit by width
            scale = screenAspect / imageAspect;
        }

        logoPlaneWidth  *= scale;
        logoPlaneHeight *= scale;
        this->logoPlaneWidth = logoPlaneWidth; this->logoPlaneHeight = logoPlaneHeight;
        struct TileIndex {
            unsigned int i0, i1, i2, i3, i4, i5;
        };

        std::vector<TileIndex> tileIndices;
        for (int y = 0; y < tilesY; ++y) {
            for (int x = 0; x < tilesX; ++x) {
                float u0 = x * tileW, v0 = y * tileH;
                float u1 = u0 + tileW, v1 = v0 + tileH;

                // Calculate tile center in the logo's scaled coordinate system
                // These will range from -logoPlaneWidth/2 to logoPlaneWidth/2 for x
                // and -logoPlaneHeight/2 to logoPlaneHeight/2 for y
                float cx = (u0 + u1) * 0.5f * logoPlaneWidth - (logoPlaneWidth * 0.5f);
                float cy = (v0 + v1) * 0.5f * logoPlaneHeight - (logoPlaneHeight * 0.5f);

                float delay = (float(x + y) + static_cast<float>(rand() % 100) / 100.0f) / (tilesX + tilesY);

                // Calculate vertex positions within the logo's scaled coordinate system
                float px0 = u0 * logoPlaneWidth - (logoPlaneWidth * 0.5f);
                float py0 = v0 * logoPlaneHeight - (logoPlaneHeight * 0.5f);
                float px1 = u1 * logoPlaneWidth - (logoPlaneWidth * 0.5f);
                float py1 = v1 * logoPlaneHeight - (logoPlaneHeight * 0.5f);

                unsigned int base = vertices.size() / 7; // Corrected stride for new vertex data layout
                vertices.insert(vertices.end(), {
                        px0, py0, u0, v0, cx, cy, delay,
                        px1, py0, u1, v0, cx, cy, delay,
                        px1, py1, u1, v1, cx, cy, delay,
                        px0, py1, u0, v1, cx, cy, delay
                });
                tileIndices.push_back({base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
                /*
                indices.insert(indices.end(), {
                        base + 0, base + 1, base + 2,
                        base + 2, base + 3, base + 0
                });*/
            }
        }
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(tileIndices.begin(), tileIndices.end(), g);

// Flatten into final index buffer
        for (const auto& t : tileIndices) {
            indices.insert(indices.end(), {t.i0, t.i1, t.i2, t.i3, t.i4, t.i5});
        }
        indexCount = indices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        size_t stride = 7 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
    }
    void LogoSwirler::computeProjection() {
        float fov = 60.0f * (3.14159265f / 180.0f); // 60 degree field of view
        float N = 1.0f;   // NEAR
        float F = -1.0f;
        float aspect = static_cast<float>(screenW) / screenH;
        float f = 1.0f / tan(fov * 0.5f);

        projection[0] = f / aspect;
        projection[1] = 0;
        projection[2] = 0;
        projection[3] = 0;

        projection[4] = 0;
        projection[5] = f;
        projection[6] = 0;
        projection[7] = 0;

        projection[8] = 0;
        projection[9] = 0;
        projection[10] = -2.0f / (F - N);  // becomes positive
        projection[11] = -1.0f;

        projection[12] = 0;
        projection[13] = 0;
        projection[14] = -(F + N) / (F - N);
        projection[15] = 0;
    }

// ── LogoSlingshot ────────────────────────────────────────────────────────────
// Scroll-in-and-impact-squeeze intro, hold, then squeeze-and-launch exit.
// Reuses the generic textured-quad pipeline (CreateLogoQuadVAO / vertexShaderSource)
// rather than the per-tile machinery LogoSwirler needs, since this is a single
// quad transform (translate + non-uniform scale) driven entirely from the CPU side.
namespace {
    float easeOutCubic(float t) { return 1.0f - powf(1.0f - t, 3.0f); }
    float easeInCubic(float t) { return t * t * t; }
    // Analytic position of a mass under a Hooke's-law spring force starting
    // from rest (x(t) = 1 - cos(t * pi/2)): zero velocity at t=0, continuously
    // accelerating, maximum velocity at t=1 — the real physics of an object
    // released by a stretched elastic band, not just an arbitrary ease curve.
    float easeInSine(float t) { return 1.0f - cosf(t * 1.57079632679f); }
    // Overshoots past 1 around the middle of the range, lands exactly on 1 at t=1.
    float easeOutBack(float t) {
        const float s = 1.70158f;
        float tm1 = t - 1.0f;
        return 1.0f + tm1 * tm1 * ((s + 1.0f) * tm1 + s);
    }
    float lerp(float a, float b, float t) { return a + (b - a) * t; }
    float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    const char* slingshotFragmentShader = R"glsl(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform sampler2D ourTexture;
        uniform float overallAlpha;
        void main() {
            vec4 color = texture(ourTexture, vTexCoord);
            if (color.a < 0.01) discard;
            FragColor = vec4(color.rgb, color.a * overallAlpha);
        }
    )glsl";
}

LogoSlingshot::LogoSlingshot(const sk_sp<SkImage>& image, int screenWidth, int screenHeight)
        : screenW(screenWidth), screenH(screenHeight) {
    setupShader();
    texture = LoadTextureFromSkImage(image);
    vao = CreateLogoQuadVAO();
    computeProjection();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

LogoSlingshot::~LogoSlingshot() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (texture) glDeleteTextures(1, &texture);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void LogoSlingshot::setupShader() {
    shaderProgram = CreateShaderProgram(vertexShaderSource, slingshotFragmentShader);
    uModel = glGetUniformLocation(shaderProgram, "model");
    uProjection = glGetUniformLocation(shaderProgram, "projection");
    uTexture = glGetUniformLocation(shaderProgram, "ourTexture");
    uAlpha = glGetUniformLocation(shaderProgram, "overallAlpha");
}

void LogoSlingshot::computeProjection() {
    ComputeOrthoProjection(projection, screenW, screenH);
}

void LogoSlingshot::draw(float tx, float sx, float sy, float alpha) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Column-major: scale, then translate.
    const float model[16] = {
            sx, 0,  0, 0,
            0,  sy, 0, 0,
            0,  0,  1, 0,
            tx, 0,  0, 1
    };

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uModel, 1, GL_FALSE, model);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, projection);
    glUniform1f(uAlpha, alpha);
    glUniform1i(uTexture, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void LogoSlingshot::renderIntro(float t) {
    float tx = 0.0f, sx = 1.0f, sy = 1.0f;
    const float arriveEnd = 0.24f, reboundEnd = 0.40f;
    // Mirrors the exit's quick snap fade (same 0.02s duration), but inverted:
    // fading IN goes 0 -> 1 (not the fade-out's 1 -> 0), and it sits right at
    // the very start so it snaps into existence just as it begins scrolling
    // in, rather than a lingering fade layered over the whole move-in.
    const float fadeInEnd = 0.02f;

    float alpha = clamp01(t / fadeInEnd);

    if (t < arriveEnd) {
        // Move-in and squash driven by the exact same eased progress value,
        // so they're locked in phase rather than merely sharing a time
        // window — both land together instead of stop-then-squash.
        float p = easeOutCubic(clamp01(t / arriveEnd));
        tx = lerp(1.4f, 0.0f, p);
        sx = lerp(1.0f, 0.4f, p);
        sy = lerp(1.0f, 1.5f, p);
    } else if (t < reboundEnd) {
        // Once stopped, rebound back to normal size with a springy overshoot.
        float eased = easeOutBack(clamp01((t - arriveEnd) / (reboundEnd - arriveEnd)));
        sx = lerp(0.4f, 1.0f, eased);
        sy = lerp(1.5f, 1.0f, eased);
    }
    // Past reboundEnd: holds at rest (tx=0, sx=sy=1) for as long as
    // renderIntro keeps being called, i.e. until the caller switches to renderExit.

    draw(tx, sx, sy, alpha);
}

bool LogoSlingshot::renderExit(float t) {
    float tx = 0.0f, sx = 1.0f, sy = 1.0f, alpha = 1.0f;
    // The elastic's own snap-back is brief compared to the flight it launches:
    // shape settles quickly (expandDuration) while position keeps
    // accelerating for the whole flight. fadeStart/fadeEnd sit right at the
    // very end, close together, so it's a quick snap out of existence rather
    // than a lingering fade — with a small buffer before flightEnd so it's
    // already gone, not still visible, when the animation stops.
    // Flight compressed to a third of its previous duration (0.75s -> 0.25s)
    // for a roughly 3x higher exit speed throughout, including at the end
    // where easeInSine's acceleration peaks. Other windows scaled down with it.
    const float squeezeEnd = 0.15f, expandDuration = 0.04f, flightEnd = 0.40f;
    const float fadeStart = 0.37f, fadeEnd = 0.39f;

    if (t < squeezeEnd) {
        // Anticipation: wind up with squash-and-stretch (X and Y moving in
        // opposite directions) and pull back to the right, standing still,
        // like a slingshot being drawn back before it releases to the left.
        float eased = easeOutCubic(clamp01(t / squeezeEnd));
        sx = lerp(1.0f, 0.4f, eased);
        sy = lerp(1.0f, 1.5f, eased);
        tx = lerp(0.0f, 0.12f, eased);
    } else if (t < flightEnd) {
        // Release: modeled as real elastic-launch physics. Position
        // continuously accelerates for the entire flight (easeInSine over
        // the full duration — starting at rest right at release, reaching
        // top speed only right at the end, like a mass released by a
        // stretched elastic band). The squash-release snaps back to shape
        // over its own much shorter window, then holds, since the elastic's
        // physical snap-back is quick, not something that lingers for the
        // whole flight. Travels well past the visible frustum (-3.0 clears
        // even portrait aspect ratios).
        float moveP = easeInSine(clamp01((t - squeezeEnd) / (flightEnd - squeezeEnd)));
        tx = lerp(0.12f, -3.0f, moveP);

        float shapeP = easeInSine(clamp01((t - squeezeEnd) / expandDuration));
        sx = lerp(0.4f, 1.15f, shapeP);
        sy = lerp(1.5f, 1.0f, shapeP);

        alpha = 1.0f - clamp01((t - fadeStart) / (fadeEnd - fadeStart));
    } else {
        alpha = 0.0f;
    }

    draw(tx, sx, sy, alpha);
    return t >= flightEnd;
}

// ── LogoLightning ────────────────────────────────────────────────────────────
// See window_anim.h for the sequence. Every constant below is the value the
// animation was approved at; they are grouped rather than inlined because the
// timing IS the design, and it is much easier to re-tune as a block.
namespace {
    // Mark geometry, measured off the baked 512x512 VOLTAIC PNG. The quad maps
    // [-0.5,0.5] -> uv [0,1], so these are simultaneously quad-local world
    // coordinates and the texture uv the fragment shader wipes from.
    // Apex = the lowest point of the V's white core: pixel (255.5, 393) of 512.
    constexpr float LL_APEX_X = 0.0f;
    constexpr float LL_APEX_Y = -0.2676f;
    // Distance from the apex to the farthest ink pixel: the reveal radius that
    // covers the whole mark, glow included.
    constexpr float LL_REVEAL_MAX = 0.80f;
    // Half-width of a channel's triangle strip, as a multiple of its nominal
    // width. The fragment falloff constants are matched to this.
    constexpr float LL_STRIP_HALF = 4.0f;

    // Intro timeline, seconds.
    constexpr float LI_LEADER_START = 0.05f;  // stepped leader starts creeping down
    constexpr float LI_STRIKE       = 0.20f;  // return stroke ignites the channel
    constexpr float LI_RETURN_RISE  = 0.035f; // how long illumination takes to run up it
    constexpr float LI_BOLT_FADE    = 0.26f;  // channel decay after the stroke
    constexpr float LI_REVEAL_DUR   = 0.15f;  // charge racing out of the apex
    constexpr float LI_FLASH_FAST   = 0.042f; // white blowout decay
    // The warm residual has to die FAST. Tuned at 0.28s first and the screen was
    // still milky grey at 0.35s, which reads as a washed-out render rather than
    // as a dark screen recovering from a flash.
    constexpr float LI_FLASH_SLOW   = 0.12f;
    constexpr float LI_SETTLE_END   = 0.80f;  // over-bright + scale back to rest
    constexpr float LI_RESTRIKE     = 0.62f;  // subsequent stroke
    constexpr float LI_RESTRIKE_DUR = 0.22f;

    // Exit timeline, seconds. The total deliberately matches the 0.40s the
    // slingshot exit used, so the caller's hand-off timing is unchanged.
    constexpr float LE_CHARGE_END  = 0.09f;
    constexpr float LE_FLASH_END   = 0.14f;
    constexpr float LE_RETRACT_END = 0.31f;
    constexpr float LE_END         = 0.40f;

    const float LL_WARM_RGB[3]  = {1.0f, 0.592f, 0.0f};
    const float LL_FLASH_RGB[3] = {1.0f, 0.988f, 0.957f};

    struct LlRnd {
        uint32_t s;
        float operator()() {
            s = s * 1664525u + 1013904223u;
            return (float)(s >> 8) / 16777216.0f;
        }
    };

    // Deterministic in t, so the flicker does not change with frame rate and a
    // held frame does not shimmer differently depending on when it was drawn.
    float llHash01(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return (float)(x & 0xffffffU) / 16777216.0f;
    }

    const char* lightningLogoFragment = R"glsl(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform sampler2D ourTexture;
        uniform float overallAlpha;
        uniform float uBright;
        uniform float uReveal;
        uniform float uSoft;
        uniform float uBand;
        const vec2 APEX = vec2(0.499, 0.768);
        void main() {
            vec4 tex = texture(ourTexture, vTexCoord);
            if (tex.a < 0.004) discard;
            // Radial wipe out of the apex. Because the V is symmetric about it,
            // one radius fills both arms at once and evenly -- no second mask.
            float d = distance(vTexCoord, APEX);
            float m = 1.0 - smoothstep(uReveal - uSoft, uReveal, d);
            // The splash is additive over black, so premultiply here and let the
            // blend do the compositing.
            vec3 emissive = tex.rgb * tex.a;
            vec3 col = emissive * uBright * m;
            if (uBand > 0.001) {
                // Hot leading edge, modulated by the mark's own intensity rather
                // than forced to white: its alpha reaches far out into a soft
                // halo, and painting that flat white turns the front into a hard
                // wedge with the mark's falloff thrown away.
                float x = (d - uReveal) / uBand;
                float band = exp(-x * x * 4.0) * step(d, uReveal + uBand);
                col += emissive * band * 2.0;
            }
            FragColor = vec4(col * overallAlpha, 1.0);
        }
    )glsl";

    const char* lightningBoltVertex = R"glsl(#version 300 es
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aAcrossAlpha;
        uniform mat4 projection;
        out float vAcross;
        out float vAlpha;
        void main() {
            vAcross = aAcrossAlpha.x;
            vAlpha = aAcrossAlpha.y;
            gl_Position = projection * vec4(aPos, 0.0, 1.0);
        }
    )glsl";

    // Core and halo in one pass: an analytic falloff across the strip width,
    // replacing the four stacked strokes the design was prototyped with.
    // exp(-d*d*44) is ~0.5 at d=0.125, i.e. at half the nominal width, which is
    // what makes the core read as the same weight as the mark's own stroke.
    const char* lightningBoltFragment = R"glsl(#version 300 es
        precision mediump float;
        in float vAcross;
        in float vAlpha;
        out vec4 FragColor;
        void main() {
            float d = abs(vAcross);
            float core = exp(-d * d * 44.0);
            float halo = exp(-d * d * 3.2);
            vec3 col = vec3(1.0, 0.992, 0.961) * core
                     + vec3(1.0, 0.592, 0.0) * halo * 0.55;
            FragColor = vec4(col * vAlpha, 1.0);
        }
    )glsl";

    // Needs world position, which the shared vertex shader does not hand out.
    const char* lightningWashVertex = R"glsl(#version 300 es
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        uniform mat4 model;
        uniform mat4 projection;
        out vec2 vWorld;
        void main() {
            vec4 wp = model * vec4(aPos, 1.0);
            vWorld = wp.xy;
            gl_Position = projection * wp;
        }
    )glsl";

    // Two-segment radial ramp. The blowout uses it with an edge term that falls
    // off faster than the centre, so as it decays the light visibly retreats
    // toward the strike instead of leaving a flat grey fog over the screen.
    const char* lightningWashFragment = R"glsl(#version 300 es
        precision mediump float;
        in vec2 vWorld;
        out vec4 FragColor;
        uniform vec2 uCenter;
        uniform float uRadius;
        uniform vec3 uColor;
        uniform float uStopMid;
        uniform float uA0;
        uniform float uAMid;
        uniform float uA1;
        void main() {
            float s = clamp(distance(vWorld, uCenter) / uRadius, 0.0, 1.0);
            float a = s < uStopMid
                    ? mix(uA0, uAMid, s / uStopMid)
                    : mix(uAMid, uA1, (s - uStopMid) / (1.0 - uStopMid));
            FragColor = vec4(uColor * a, 1.0);
        }
    )glsl";

    void llArcLengths(const std::vector<float>& px, const std::vector<float>& py,
                      std::vector<float>& cum) {
        cum.assign(px.size(), 0.0f);
        for (size_t i = 1; i < px.size(); ++i)
            cum[i] = cum[i - 1] + hypotf(px[i] - px[i - 1], py[i] - py[i - 1]);
    }

    // Midpoint displacement: each new midpoint is pushed along the segment
    // normal by an amount proportional to the segment length, so the
    // displacement halves every level for free and the channel stays
    // self-similar at every scale -- which is what makes it read as lightning
    // rather than as a zigzag.
    void llSubdivide(std::vector<float>& px, std::vector<float>& py,
                     LlRnd& rnd, float roughness, int levels) {
        std::vector<float> nx, ny;
        for (int l = 0; l < levels; ++l) {
            nx.clear(); ny.clear();
            nx.reserve(px.size() * 2);
            ny.reserve(py.size() * 2);
            for (size_t i = 0; i + 1 < px.size(); ++i) {
                const float ax = px[i], ay = py[i], bx = px[i + 1], by = py[i + 1];
                const float dx = bx - ax, dy = by - ay;
                const float len = hypotf(dx, dy);
                if (len <= 1e-6f) { nx.push_back(ax); ny.push_back(ay); continue; }
                const float onx = -dy / len, ony = dx / len;
                const float off = (rnd() * 2.0f - 1.0f) * roughness * len;
                nx.push_back(ax);
                ny.push_back(ay);
                nx.push_back((ax + bx) * 0.5f + onx * off);
                ny.push_back((ay + by) * 0.5f + ony * off);
            }
            nx.push_back(px.back());
            ny.push_back(py.back());
            px.swap(nx);
            py.swap(ny);
        }
    }
}

LogoLightning::LogoLightning(const sk_sp<SkImage>& image, int screenWidth, int screenHeight)
        : screenW(screenWidth), screenH(screenHeight) {
    // Matches ComputeOrthoProjection's view extents, so world units here are the
    // units the projection expects.
    if (screenW >= screenH) {
        halfW = (float)screenW / (float)screenH;
        halfH = 1.0f;
    } else {
        halfW = 1.0f;
        halfH = (float)screenH / (float)screenW;
    }

    setupShaders();
    texture = LoadTextureFromSkImage(image);
    quadVao = CreateLogoQuadVAO();
    ComputeOrthoProjection(projection, screenW, screenH);

    std::random_device rd;
    seed = (uint32_t)rd();
    buildBolts();

    glGenVertexArrays(1, &boltVao);
    glGenBuffers(1, &boltVbo);
    glBindVertexArray(boltVao);
    glBindBuffer(GL_ARRAY_BUFFER, boltVbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

LogoLightning::~LogoLightning() {
    if (quadVao) glDeleteVertexArrays(1, &quadVao);
    if (boltVao) glDeleteVertexArrays(1, &boltVao);
    if (boltVbo) glDeleteBuffers(1, &boltVbo);
    if (texture) glDeleteTextures(1, &texture);
    if (logoProgram) glDeleteProgram(logoProgram);
    if (boltProgram) glDeleteProgram(boltProgram);
    if (washProgram) glDeleteProgram(washProgram);
}

void LogoLightning::setupShaders() {
    logoProgram = CreateShaderProgram(vertexShaderSource, lightningLogoFragment);
    uLogoModel  = glGetUniformLocation(logoProgram, "model");
    uLogoProj   = glGetUniformLocation(logoProgram, "projection");
    uLogoTex    = glGetUniformLocation(logoProgram, "ourTexture");
    uLogoAlpha  = glGetUniformLocation(logoProgram, "overallAlpha");
    uLogoBright = glGetUniformLocation(logoProgram, "uBright");
    uLogoReveal = glGetUniformLocation(logoProgram, "uReveal");
    uLogoSoft   = glGetUniformLocation(logoProgram, "uSoft");
    uLogoBand   = glGetUniformLocation(logoProgram, "uBand");

    boltProgram = CreateShaderProgram(lightningBoltVertex, lightningBoltFragment);
    uBoltProj = glGetUniformLocation(boltProgram, "projection");

    washProgram  = CreateShaderProgram(lightningWashVertex, lightningWashFragment);
    uWashModel   = glGetUniformLocation(washProgram, "model");
    uWashProj    = glGetUniformLocation(washProgram, "projection");
    uWashCenter  = glGetUniformLocation(washProgram, "uCenter");
    uWashRadius  = glGetUniformLocation(washProgram, "uRadius");
    uWashColor   = glGetUniformLocation(washProgram, "uColor");
    uWashStopMid = glGetUniformLocation(washProgram, "uStopMid");
    uWashA0      = glGetUniformLocation(washProgram, "uA0");
    uWashAMid    = glGetUniformLocation(washProgram, "uAMid");
    uWashA1      = glGetUniformLocation(washProgram, "uA1");
}

void LogoLightning::buildBolts() {
    LlRnd rnd{seed ^ 0x9e3779b9u};

    auto make = [&](Bolt& b, float x0, float y0, float roughness, int levels, int forks) {
        b.px = {x0, LL_APEX_X};
        b.py = {y0, LL_APEX_Y};
        b.branches.clear();
        llSubdivide(b.px, b.py, rnd, roughness, levels);
        llArcLengths(b.px, b.py, b.cum);
        const float total = b.cum.back();

        for (int f = 0; f < forks; ++f) {
            // Split somewhere in the middle of the channel, never at the ends.
            const float where = lerp(0.15f, 0.8f, rnd());
            int idx = (int)lroundf(where * (float)(b.px.size() - 1));
            if (idx < 1) idx = 1;
            if (idx > (int)b.px.size() - 2) idx = (int)b.px.size() - 2;

            float tx = b.px[idx] - b.px[idx - 1];
            float ty = b.py[idx] - b.py[idx - 1];
            const float tl = hypotf(tx, ty);
            if (tl <= 1e-6f) continue;
            tx /= tl; ty /= tl;

            // Rotate the parent tangent by 25-55 degrees, either side.
            const float ang = (0.44f + rnd() * 0.52f) * (rnd() < 0.5f ? -1.0f : 1.0f);
            const float ca = cosf(ang), sa = sinf(ang);
            const float dx = tx * ca - ty * sa, dy = tx * sa + ty * ca;
            // Taken as an arc fraction, not an index fraction, because that is
            // what drawChannel gates the branch against.
            const float at = total > 0.0f ? b.cum[idx] / total : 0.0f;
            // Branches get shorter the further down the channel they split off.
            const float len = total * lerp(0.16f, 0.42f, rnd()) * (1.0f - at * 0.5f);

            Branch br;
            br.px = {b.px[idx], b.px[idx] + dx * len};
            br.py = {b.py[idx], b.py[idx] + dy * len};
            llSubdivide(br.px, br.py, rnd, roughness * 1.15f, levels - 2);
            llArcLengths(br.px, br.py, br.cum);
            br.at = at;
            br.bright = lerp(0.28f, 0.55f, rnd());
            b.branches.push_back(std::move(br));
        }
    };

    // Enters from the top edge, offset from centre so it never reads as a
    // symmetric "beam", and terminates exactly on the mark's apex -- the strike
    // becomes the logo rather than landing near it.
    make(mainBolt, lerp(-0.42f, 0.42f, rnd()), halfH * 1.05f, 0.30f, 6,
         2 + (int)(rnd() * 3.0f));
    make(subsequent, lerp(-0.35f, 0.35f, rnd()), halfH * 1.05f, 0.24f, 5, 2);
}

// Emits the arc-length range [from,to] of a channel (plus any branch the
// illumination front has already passed) as quads, then draws them in one call.
void LogoLightning::drawChannel(const Bolt& b, float from, float to,
                                float width, float alpha) {
    if (to <= from || alpha <= 0.002f) return;
    stripScratch.clear();

    const float hw = width * LL_STRIP_HALF;

    auto emit = [&](const std::vector<float>& px, const std::vector<float>& py,
                    const std::vector<float>& cum, float a0, float a1, float amp) {
        const float total = cum.back();
        if (total <= 0.0f || a1 <= a0) return;
        const float la = a0 * total, lb = a1 * total;
        for (size_t i = 0; i + 1 < px.size(); ++i) {
            const float s0 = cum[i], s1 = cum[i + 1];
            if (s1 < la || s0 > lb) continue;
            const float seg = s1 - s0;
            if (seg <= 1e-6f) continue;
            const float t0 = clamp01((std::max(s0, la) - s0) / seg);
            const float t1 = clamp01((std::min(s1, lb) - s0) / seg);
            float ax = lerp(px[i], px[i + 1], t0), ay = lerp(py[i], py[i + 1], t0);
            float bx = lerp(px[i], px[i + 1], t1), by = lerp(py[i], py[i + 1], t1);

            float dx = bx - ax, dy = by - ay;
            const float len = hypotf(dx, dy);
            if (len <= 1e-6f) continue;
            dx /= len; dy /= len;
            // Extend both ends along the segment: a cheap round-cap stand-in
            // that also fills the wedge a sharp joint would otherwise leave open.
            const float ext = hw * 0.6f;
            ax -= dx * ext; ay -= dy * ext;
            bx += dx * ext; by += dy * ext;
            const float nx = -dy * hw, ny = dx * hw;

            const float quad[4][4] = {
                    {ax - nx, ay - ny, -1.0f, amp},
                    {bx - nx, by - ny, -1.0f, amp},
                    {bx + nx, by + ny,  1.0f, amp},
                    {ax + nx, ay + ny,  1.0f, amp},
            };
            const int order[6] = {0, 1, 2, 2, 3, 0};
            for (int k : order)
                stripScratch.insert(stripScratch.end(), quad[k], quad[k] + 4);
        }
    };

    emit(b.px, b.py, b.cum, from, to, alpha);
    for (const Branch& br : b.branches) {
        // A branch only lights up once the front has passed its split point.
        if (br.at < from || br.at > to) continue;
        const float reach = clamp01((to - br.at) / 0.25f);
        emit(br.px, br.py, br.cum, 0.0f, reach, alpha * br.bright);
    }
    if (stripScratch.empty()) return;

    glUseProgram(boltProgram);
    glUniformMatrix4fv(uBoltProj, 1, GL_FALSE, projection);
    glBindVertexArray(boltVao);
    glBindBuffer(GL_ARRAY_BUFFER, boltVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(stripScratch.size() * sizeof(float)),
                 stripScratch.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(stripScratch.size() / 4));
    glBindVertexArray(0);
}

void LogoLightning::drawWash(float cx, float cy, float radius, const float rgb[3],
                             float stopMid, float a0, float aMid, float a1) {
    // The unit quad, scaled to cover the whole view.
    const float model[16] = {
            halfW * 2.0f, 0, 0, 0,
            0, halfH * 2.0f, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
    };
    glUseProgram(washProgram);
    glUniformMatrix4fv(uWashModel, 1, GL_FALSE, model);
    glUniformMatrix4fv(uWashProj, 1, GL_FALSE, projection);
    glUniform2f(uWashCenter, cx, cy);
    glUniform1f(uWashRadius, radius);
    glUniform3f(uWashColor, rgb[0], rgb[1], rgb[2]);
    glUniform1f(uWashStopMid, stopMid);
    glUniform1f(uWashA0, a0);
    glUniform1f(uWashAMid, aMid);
    glUniform1f(uWashA1, a1);
    glBindVertexArray(quadVao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void LogoLightning::submit(const Frame& f) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Everything here is emissive on black, so it all composites additively and
    // nothing needs sorting. Set every frame rather than once in the
    // constructor: the caller's loop shares this context with Skia.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDisable(GL_DEPTH_TEST);

    // Warm ambient wash: the air around the channel glowing. Centred on the mark
    // and falling off well before the screen edges -- a full-screen wash makes
    // the whole panel look grey instead of black.
    if (f.flashWarm > 0.002f)
        drawWash(0.0f, 0.0f, 1.5f, LL_WARM_RGB, 0.55f,
                 f.flashWarm, f.flashWarm * 0.35f, 0.0f);

    if (f.logoAlpha > 0.002f) {
        const float s = f.logoScale;
        const float model[16] = {
                s, 0, 0, 0,
                0, s, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
        };
        glUseProgram(logoProgram);
        glUniformMatrix4fv(uLogoModel, 1, GL_FALSE, model);
        glUniformMatrix4fv(uLogoProj, 1, GL_FALSE, projection);
        glUniform1f(uLogoAlpha, f.logoAlpha);
        glUniform1f(uLogoBright, f.logoBright);
        glUniform1f(uLogoReveal, f.reveal);
        glUniform1f(uLogoSoft, f.revealSoft);
        glUniform1f(uLogoBand, f.frontBand);
        glUniform1i(uLogoTex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(quadVao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    for (int i = 0; i < f.channelCount; ++i) {
        const Frame::Channel& c = f.channels[i];
        drawChannel(*c.bolt, c.from, c.to, c.width, c.alpha);
    }

    // The blowout goes last, over everything.
    if (f.flashWhite > 0.002f) {
        const float e = powf(f.flashWhite, 1.7f);
        drawWash(LL_APEX_X, LL_APEX_Y, 2.1f, LL_FLASH_RGB, 0.45f,
                 f.flashWhite, lerp(e, f.flashWhite, 0.45f), e);
    }
}

void LogoLightning::renderIntro(float t) {
    Frame f;
    auto flicker = [&](float rate, float depth) {
        const uint32_t n = (uint32_t)(int32_t)floorf(t * rate);
        return 1.0f - depth * llHash01((seed ^ 0x2545f491u) + n * 2654435761u);
    };

    // ── Stepped leader: dim, jittery, creeping down. This ordering is the
    // physics -- the faint channel always precedes the bright return stroke,
    // and skipping it makes the strike read as a wipe rather than as a strike.
    if (t >= LI_LEADER_START && t < LI_STRIKE) {
        const float p = clamp01((t - LI_LEADER_START) / (LI_STRIKE - LI_LEADER_START));
        const float grow = easeInCubic(p);
        // Stepped, not smooth: a leader advances in discrete jumps.
        const float stepped = floorf(grow * 9.0f) / 9.0f + (1.0f / 9.0f) * 0.35f;
        const float fl = flicker(45.0f, 0.55f);
        f.channels[f.channelCount++] = {&mainBolt, 0.0f, clamp01(stepped), 0.0034f,
                                        0.34f * fl * lerp(0.35f, 1.0f, p)};
        f.flashWarm = 0.018f * p * fl;
    }

    if (t >= LI_STRIKE) {
        const float u = t - LI_STRIKE;

        // ── Return stroke: the illumination runs UP the channel from the apex.
        const float rise = clamp01(u / LI_RETURN_RISE);
        const float from = 1.0f - easeOutCubic(rise);
        const float decay = expf(-u / (LI_BOLT_FADE * 0.42f));
        const float fl = u < LI_BOLT_FADE ? flicker(38.0f, 0.45f) : 0.0f;
        const float boltA = std::max(decay * lerp(1.0f, fl, clamp01(u / 0.05f)), 0.0f);
        if (boltA > 0.004f)
            f.channels[f.channelCount++] = {&mainBolt, from, 1.0f, 0.0075f, boltA};

        // ── Blowout: a hard white spike plus a faster-dying warm wash.
        f.flashWhite = 0.95f * expf(-u / LI_FLASH_FAST);
        f.flashWarm = 0.11f * expf(-u / LI_FLASH_SLOW);

        // ── Reveal: the charge races out of the apex along both arms at once.
        const float rp = easeOutCubic(clamp01(u / LI_REVEAL_DUR));
        f.reveal = rp * LL_REVEAL_MAX;
        f.revealSoft = lerp(0.10f, 0.03f, rp);
        f.frontBand = lerp(0.10f, 0.0f, rp * rp);
        f.logoAlpha = 1.0f;

        // ── Settle: over-exposed and oversized at impact, easing to rest.
        const float sp = easeOutCubic(clamp01(u / (LI_SETTLE_END - LI_STRIKE)));
        f.logoBright = lerp(3.2f, 1.0f, sp);
        f.logoScale = lerp(1.075f, 1.0f, sp);

        // ── Subsequent stroke: real lightning fires several down one channel.
        const float r = t - LI_RESTRIKE;
        if (r >= 0.0f && r < LI_RESTRIKE_DUR) {
            const float rd = expf(-r / (LI_RESTRIKE_DUR * 0.3f));
            const float rrise = clamp01(r / 0.03f);
            f.channels[f.channelCount++] = {&subsequent, 1.0f - easeOutCubic(rrise),
                                            1.0f, 0.0052f,
                                            0.62f * rd * flicker(40.0f, 0.4f)};
            f.flashWhite = std::max(f.flashWhite, 0.42f * expf(-r / 0.045f));
            f.flashWarm = std::max(f.flashWarm, 0.06f * expf(-r / 0.09f));
            f.logoBright = std::max(f.logoBright, lerp(2.1f, 1.0f, clamp01(r / 0.16f)));
        }

        // ── Hold: at rest, but alive. Slow glow breathing plus a rare two-beat
        // micro-flicker, so a slow init never leaves a frozen PNG on screen.
        if (t > LI_SETTLE_END) {
            const float h = t - LI_SETTLE_END;
            float b = 1.0f + 0.035f * sinf(h * 2.0f * 3.14159265f * 0.33f);
            const float cycle = fmodf(h, 3.1f);
            if (cycle < 0.05f) b *= 0.82f;
            else if (cycle > 0.09f && cycle < 0.13f) b *= 0.88f;
            f.logoBright = std::max(f.logoBright, b);
        }
    }

    submit(f);
}

bool LogoLightning::renderExit(float t) {
    Frame f;
    f.logoAlpha = 1.0f;
    f.reveal = LL_REVEAL_MAX * 1.4f;
    f.revealSoft = 0.03f;
    auto flicker = [&](float rate, float depth) {
        const uint32_t n = (uint32_t)(int32_t)floorf(t * rate);
        return 1.0f - depth * llHash01((seed ^ 0x2545f491u) + n * 2654435761u);
    };

    if (t < LE_CHARGE_END) {
        // Wind-up: the mark pulls charge in -- brighter, a touch larger, and the
        // channel above it starts to re-form faintly.
        const float p = easeInCubic(clamp01(t / LE_CHARGE_END));
        f.logoBright = lerp(1.0f, 2.4f, p);
        f.logoScale = lerp(1.0f, 1.055f, p);
        f.channels[f.channelCount++] = {&subsequent, lerp(1.0f, 0.55f, p), 1.0f,
                                        0.0030f, 0.22f * p * flicker(50.0f, 0.6f)};
        f.flashWarm = 0.03f * p;
    } else {
        const float u = t - LE_CHARGE_END;
        // Discharge: the stroke fires back up the channel and off the top edge.
        const float rise = clamp01(u / 0.05f);
        const float boltA = expf(-u / 0.13f);
        f.channels[f.channelCount++] = {&subsequent, 0.0f, 1.0f, 0.0080f,
                                        boltA * lerp(0.5f, 1.0f, rise)};
        f.flashWhite = 0.9f * expf(-u / 0.05f);
        f.flashWarm = 0.10f * expf(-u / 0.10f);

        // The reveal runs backwards: the V drains into its apex and is gone.
        const float rp = easeInCubic(clamp01((t - LE_FLASH_END) /
                                             (LE_RETRACT_END - LE_FLASH_END)));
        f.reveal = lerp(LL_REVEAL_MAX * 1.4f, 0.0f, rp);
        f.revealSoft = lerp(0.03f, 0.10f, rp);
        f.frontBand = lerp(0.0f, 0.09f, rp) * (1.0f - rp * 0.4f);
        f.logoBright = lerp(3.0f, 1.2f, clamp01(u / 0.18f));
        f.logoAlpha = 1.0f - clamp01((t - LE_FLASH_END) /
                                     (LE_RETRACT_END - LE_FLASH_END)) * 0.35f;
        f.logoScale = lerp(1.055f, 0.93f,
                           clamp01((t - LE_CHARGE_END) / (LE_RETRACT_END - LE_CHARGE_END)));
    }

    submit(f);
    return t >= LE_END;
}

ImGuiShapeRenderer::ImGuiShapeRenderer() : shaderProgram(0), vao(0), vbo(0), uProjectionLoc(-1) {
        // Initialize projectionMatrix to zeros
        for(float & i : projectionMatrix) i = 0.0f;
    }

ImGuiShapeRenderer::~ImGuiShapeRenderer() {
        if (shaderProgram) glDeleteProgram(shaderProgram);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
    }

    // --- Initialization (Call once after OpenGL context is created) ---
    void ImGuiShapeRenderer::init() {
        // --- Shader Source Code ---
        const char* VERTEX_SHADER_SRC = R"glsl(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4 uProjection;

out vec4 vColor;

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0); // Z is 0.0 for 2D shapes
    vColor = aColor;
}
)glsl";

        const char* FRAGMENT_SHADER_SRC = R"glsl(#version 300 es
precision mediump float;

in vec4 vColor;
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
)glsl";

        // Compile and link shaders
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
        shaderProgram = createProgram(vertexShader, fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Get uniform location
        uProjectionLoc = glGetUniformLocation(shaderProgram, "uProjection");
        if (uProjectionLoc == -1) {
            LOGE("Warning: uProjection uniform not found!");
        }

        // Setup VAO and VBO
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Define vertex attributes
        // Position (2 floats)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPC), (void*)offsetof(VertexPC, x));
        glEnableVertexAttribArray(0);
        // Color (4 floats)
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexPC), (void*)offsetof(VertexPC, r));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0); // Unbind VAO
        glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO

        // Enable blending for transparency (important for UI overlays)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Disable depth test for 2D overlay (ensures it always draws on top)
        glDisable(GL_DEPTH_TEST);
    }

    // --- Rendering (Call inside your rendering loop) ---
    void ImGuiShapeRenderer::render(float w, float h) {
        // Update projection matrix if screen size changes
        updateOrthographicProjection(w, h);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, projectionMatrix);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo); // Bind VBO to upload data

        // Temporary vectors to hold vertices for each part of the shape
        std::vector<VertexPC> currentShapeVertices;
        GLfloat blueColor[4] = {33.0f/255.0f, 150.0f/255.0f, 243.0f/255.0f, 1.0f}; // RGBA (normalized 0-1)
        GLfloat blackColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        if (h > w) { // Logic when height is greater than width
            float radius = w * .4f;
            float center_x = w * .5f;
            float center_y = h * .5f;
            int segments = 50; // Number of segments for circles and arcs

            // 1. Draw main blue circle
            generateFilledCircleVertices(currentShapeVertices, center_x, center_y, radius, blueColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, currentShapeVertices.size());

            // 2. Draw two small black circles
            float small_radius = w * .05f;
            generateFilledCircleVertices(currentShapeVertices, center_x - w * .15f, center_y - w * .1f, small_radius, blackColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, currentShapeVertices.size());

            generateFilledCircleVertices(currentShapeVertices, center_x + w * .15f, center_y - w * .1f, small_radius, blackColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, currentShapeVertices.size());

            // 3. Draw black arc
            float arc_center_x = w * .5f;
            float arc_center_y = h * .5f + w * .375f;
            float arc_radius = w * .25f;
            float arc_thickness = w * .05f;

            // ImGui angles: PI_F_P + PI_F_P * .2  to  PI_F_P + PI_F_P * .8
            // This corresponds to 1.2 * PI to 1.8 * PI radians.
            float start_angle_rad = PI_F_P * 1.2f;
            float end_angle_rad = PI_F_P * 1.8f;

            generateStrokedArcVertices(currentShapeVertices, arc_center_x, arc_center_y, arc_radius, start_angle_rad, end_angle_rad, arc_thickness, blackColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, currentShapeVertices.size());

        } else { // Logic when width is greater than or equal to height
            float radius = h * .4f;
            float center_x = w * .5f;
            float center_y = h * .5f;
            int segments = 50;

            // 1. Draw main blue circle
            generateFilledCircleVertices(currentShapeVertices, center_x, center_y, radius, blueColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, currentShapeVertices.size());

            // 2. Draw two small black circles
            float small_radius = h * .05f;
            generateFilledCircleVertices(currentShapeVertices, center_x - h * .15f, center_y - h * .1f, small_radius, blackColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, currentShapeVertices.size());

            generateFilledCircleVertices(currentShapeVertices, center_x + h * .15f, center_y - h * .1f, small_radius, blackColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, currentShapeVertices.size());

            // 3. Draw black arc
            float arc_center_x = w * .5f;
            float arc_center_y = h * 1.05f; // This is a very specific Y coordinate, potentially off-screen
            float arc_radius = h * .375f;
            float arc_thickness = h * .05f;

            // ImGui angles: PI_F_P * 1.35  to  PI_F_P + PI_F_P * .65
            // This corresponds to 1.35 * PI to 1.65 * PI radians.
            float start_angle_rad = PI_F_P * 1.35f;
            float end_angle_rad = PI_F_P * 1.65f;

            generateStrokedArcVertices(currentShapeVertices, arc_center_x, arc_center_y, arc_radius, start_angle_rad, end_angle_rad, arc_thickness, blackColor, segments);
            glBufferData(GL_ARRAY_BUFFER, currentShapeVertices.size() * sizeof(VertexPC), currentShapeVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, currentShapeVertices.size());
        }

        glBindVertexArray(0); // Unbind VAO
        glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
        glUseProgram(0); // Unbind program
    }

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


    void ImGuiShapeRenderer::updateOrthographicProjection(float w, float h) {
        // Orthographic projection matrix: maps (0,0) top-left to (w,h) bottom-right to NDC (-1 to 1)
        float L = 0.0f;
        float R = w;
        float B = h; // Screen coordinates Y increase downwards
        float T = 0.0f;
        float N = -1.0f; // Near plane
        float F = 1.0f;  // Far plane

        // Column-major order for OpenGL's glUniformMatrix4fv
        projectionMatrix[0] = 2.0f / (R - L);   projectionMatrix[1] = 0.0f;             projectionMatrix[2] = 0.0f;              projectionMatrix[3] = 0.0f;
        projectionMatrix[4] = 0.0f;             projectionMatrix[5] = 2.0f / (T - B);   projectionMatrix[6] = 0.0f;              projectionMatrix[7] = 0.0f;
        projectionMatrix[8] = 0.0f;             projectionMatrix[9] = 0.0f;             projectionMatrix[10] = -2.0f / (F - N);  projectionMatrix[11] = 0.0f;
        projectionMatrix[12] = -(R + L) / (R - L); projectionMatrix[13] = -(T + B) / (T - B); projectionMatrix[14] = -(F + N) / (F - N); projectionMatrix[15] = 1.0f;
    }

    void ImGuiShapeRenderer::generateFilledCircleVertices(std::vector<VertexPC>& vertices, float cx, float cy, float radius, const GLfloat color[4], int segments) {
        vertices.clear();

        VertexPC center_vertex = {cx, cy, color[0], color[1], color[2], color[3]};
        vertices.push_back(center_vertex); // Center of the fan

        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * PI_F_P * static_cast<float>(i) / static_cast<float>(segments);
            float x = cx + radius * std::cos(angle);
            float y = cy + radius * std::sin(angle);
            vertices.push_back({x, y, color[0], color[1], color[2], color[3]});
        }
    }

    void ImGuiShapeRenderer::generateStrokedArcVertices(std::vector<VertexPC>& vertices, float cx, float cy, float radius, float startAngleRad, float endAngleRad, float thickness, const GLfloat color[4], int segments) {
        vertices.clear();

        float innerRadius = radius - thickness * 0.5f;
        float outerRadius = radius + thickness * 0.5f;

        // Ensure angles are in increasing order for proper sweep calculation
        // This is important for calculating the correct sweep.
        // If start > end, swap them and adjust sweep logic.
        // Or normalize to [0, 2PI) range first.
        // For arcs that cross 2PI/0, this might need more sophisticated handling
        // but for typical cases where endAngle is simply greater than startAngle.
        // ImGui's PathArcTo often provides angles this way, so we assume endAngleRad > startAngleRad for a simple sweep.

        float sweepAngleRad = endAngleRad - startAngleRad;
        // Adjust for cases where end_angle < start_angle (e.g., negative sweep)
        // ImGui's arc logic typically implies a positive sweep angle between a_min and a_max.
        if (sweepAngleRad < 0) {
            sweepAngleRad += 2.0f * PI_F_P; // Add a full circle
        }

        // Generate vertices for the inner and outer edges of the arc
        for (int i = 0; i <= segments; ++i) {
            float angle = startAngleRad + sweepAngleRad * static_cast<float>(i) / static_cast<float>(segments);
            float cos_angle = std::cos(angle);
            float sin_angle = std::sin(angle);

            // Inner vertex
            float ix = cx + innerRadius * cos_angle;
            float iy = cy + innerRadius * sin_angle;
            vertices.push_back({ix, iy, color[0], color[1], color[2], color[3]});

            // Outer vertex
            float ox = cx + outerRadius * cos_angle;
            float oy = cy + outerRadius * sin_angle;
            vertices.push_back({ox, oy, color[0], color[1], color[2], color[3]});
        }
        // This will be drawn as GL_TRIANGLE_STRIP
    }


class LogoGL {
public:
    LogoGL(const sk_sp<SkImage>& icon, const char *vertexShaderSrc, const char *fragmentShaderSrc, int w,
           int h) {
        float vertices[] = {
                // Positions      // Texture Coords
                -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // Bottom Left
                0.5f, -0.5f, 0.0f, 1.0f, 1.0f, // Bottom Right
                0.5f, 0.5f, 0.0f, 1.0f, 0.0f, // Top Right
                -0.5f, 0.5f, 0.0f, 0.0f, 0.0f  // Top Left
        };
        unsigned int indices[] = {
                0, 1, 2,  // First Triangle
                2, 3, 0   // Second Triangle
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
        glEnableVertexAttribArray(0);
        // Texture coord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void *) (3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        logoShaderProgram = CreateShaderProgram(vertexShaderSrc, fragmentShader);
        if (logoShaderProgram == 0) {
            LOGE("Failed to create logo shader program. Aborting logo animation.");
            // Potentially handle this error, maybe fall back to a simpler display or skip
        }

// --- Load Logo Texture ---
        logoTextureID = LoadTextureFromSkImage(icon);
        /*
        if (icon) {
            logoTextureID = LoadTextureFromSkImage(icon);
        } else {
            LOGE("Failed to load logo using LoadPNG().");
        }
*/
        if (logoTextureID == 0) {
            LOGE("Failed to load logo texture. Aborting logo animation.");
            // Potentially handle this error
            if (logoShaderProgram != 0) {
                glDeleteProgram(logoShaderProgram); // Clean up shader if texture failed
                logoShaderProgram = 0;
            }
        }

        // You'll need a matrix library (like GLM) or implement basic matrix math.
        // This is a simplified orthographic projection.
        // For simplicity, let's assume a [-1, 1] NDC space for now.
        // If your logo quad is not centered or sized for [-0.5, 0.5], adjust projection/model.
        float orthoProjection[16] = { // Basic orthographic matrix
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
        };
        // If your window aspect ratio isn't 1:1, you'll want to adjust this
        // to prevent stretching. For example:
        if (w != 0 && h != 0) {
            float aspect = (float) w / (float) h;
            if (aspect > 1.0f) { // Wider than tall
                orthoProjection[0] = 1.0f / aspect; // Scale X
            } else { // Taller than wide
                orthoProjection[5] = aspect;      // Scale Y
            }
        }


        const float pulseFrequency = 1.5f; // How many full pulses per second
        const float minScale = 0.90f;     // The smallest the logo will be
        const float maxScale = 1.00f;     // The largest the logo will be (original size)
        const float minBrightness = 0.85f; // Minimum brightness multiplier
        const float maxBrightness = 1.0f;  // Maximum brightness multiplier
        float modelMatrix[16]; // Your 4x4 model matrix
        const float identityMatrix[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
        };
        memcpy(modelMatrix, identityMatrix, sizeof(identityMatrix));



// --- Get Uniform Locations (should be done once after shader linking) ---
        modelLocation = glGetUniformLocation(logoShaderProgram, "model");
        projectionLocation = glGetUniformLocation(logoShaderProgram, "projection");
        overallAlphaLocation = glGetUniformLocation(logoShaderProgram,
                                                    "overallAlpha");
        timeLocation = glGetUniformLocation(logoShaderProgram,
                                            "time"); // For wave effect

        //if (elapsed < initialFadeInNanos) {
        //  overallAlpha = static_cast<float>(elapsed) / static_cast<float>(initialFadeInNanos);
        // }
        //overallAlpha = std::min(1.0f, std::max(0.0f, overallAlpha));


// --- Set Uniforms (inside the rendering block of your animation loop) ---
        if (logoShaderProgram != 0 && logoTextureID != 0) {
            glUseProgram(logoShaderProgram);

            // 1. Set Model Matrix
            if (modelLocation != -1) {
                glUniformMatrix4fv(modelLocation, 1, GL_FALSE, modelMatrix);
            }

            // 2. Set Projection Matrix
            if (projectionLocation != -1) {
                // orthoProjection should be calculated based on your window width (w) and height (h)
                // For example, to map a region from -aspectRatio to +aspectRatio on X,
                // and -1 to 1 on Y (if wider than tall) to NDC:
                float currentOrthoProjection[16]; // Calculate this based on w, h
                // Simplified orthographic projection:
                // This makes the view volume range from -1 to 1 in the shorter dimension,
                // and scaled appropriately in the longer dimension.
                // It assumes your quad vertices are in a similar range (e.g., -0.5 to 0.5).
                const float Z_NEAR = -1.0f;
                const float Z_FAR = 1.0f;
                float R, L, T, B; // Right, Left, Top, Bottom of your view volume

                if (w >= h) { // Wider or square screen
                    R = (float) w / (float) h;
                    L = -R;
                    T = 1.0f;
                    B = -1.0f;
                } else { // Taller screen
                    R = 1.0f;
                    L = -1.0f;
                    T = (float) h / (float) w;
                    B = -T;
                }
                // Orthographic matrix formula elements:
                currentOrthoProjection[0] = 2.0f / (R - L);
                currentOrthoProjection[4] = 0.0f;
                currentOrthoProjection[8] = 0.0f;
                currentOrthoProjection[12] = -(R + L) / (R - L);
                currentOrthoProjection[1] = 0.0f;
                currentOrthoProjection[5] = 2.0f / (T - B);
                currentOrthoProjection[9] = 0.0f;
                currentOrthoProjection[13] = -(T + B) / (T - B);
                currentOrthoProjection[2] = 0.0f;
                currentOrthoProjection[6] = 0.0f;
                currentOrthoProjection[10] = -2.0f / (Z_FAR - Z_NEAR);
                currentOrthoProjection[14] = -(Z_FAR + Z_NEAR) / (Z_FAR - Z_NEAR);
                currentOrthoProjection[3] = 0.0f;
                currentOrthoProjection[7] = 0.0f;
                currentOrthoProjection[11] = 0.0f;
                currentOrthoProjection[15] = 1.0f;

                glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, currentOrthoProjection);
                modelLocation = glGetUniformLocation(logoShaderProgram, "model");
                projectionLocation = glGetUniformLocation(logoShaderProgram, "projection");
                overallAlphaLocation = glGetUniformLocation(logoShaderProgram,
                                                            "overallAlpha");
                timeLocation = glGetUniformLocation(logoShaderProgram,
                                                    "time"); // For wave effect

            }
        }
    }
    void render(float elapsedSeconds = 0) const {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // 3. Set Overall Alpha
        if (overallAlphaLocation != -1) {

            float overallAlpha = 1.0f;
            glUniform1f(overallAlphaLocation, elapsedSeconds < 1. ? elapsedSeconds
                                                                  : 1.); // 'overallAlpha' calculated above
        }

        // 4. Set Time (for wave effect)
        if (timeLocation != -1) {
            glUniform1f(timeLocation, elapsedSeconds);
        }
        glUniform1f(glGetUniformLocation(logoShaderProgram, "explodeTime"),
                    1.5f); // Explode at 4s
        glUniform1f(glGetUniformLocation(logoShaderProgram, "explosionStrength"), 1.5f);
        float offsetX = static_cast<float>(rand() % 1000) / 1000.0f;
        float offsetY = static_cast<float>(rand() % 1000) / 1000.0f;
        glUniform2f(glGetUniformLocation(logoShaderProgram, "randomOffset"), offsetX,
                    offsetY);


        // ... rest of your uniform settings (texture sampler) and drawing code ...
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, logoTextureID);
        glUniform1i(glGetUniformLocation(logoShaderProgram, "ourTexture"), 0);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    ~LogoGL(){
        // Clean up GL resources for the logo animation if they are not needed later
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        if (logoShaderProgram != 0) glDeleteProgram(logoShaderProgram);
        if (logoTextureID !=
            0)  glDeleteTextures(1, &logoTextureID); // Only if not used elsewhere */ }

    }

private:
    GLuint VAO{}, VBO{}, EBO{}, logoShaderProgram, logoTextureID{}, modelLocation{}, projectionLocation{}, overallAlphaLocation{}, timeLocation{};

    const char *fragmentShader = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float overallAlpha;
uniform float time;
uniform float explodeTime;
uniform float explosionStrength;

vec2 rotate2D(vec2 pt, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c) * pt;
}

void main() {
    vec2 uv = vTexCoord;

    // Explosion animation progress (0 to 1+)
    float explodeFactor = max(0.0, time - explodeTime);

    // Seed per-fragment motion direction and spin
    float seed = dot(uv, vec2(12.9898, 78.233));
    float angle = fract(sin(seed) * 43758.5453) * 6.2831; // 0..2π
    vec2 direction = vec2(cos(angle), sin(angle));

    // Strong acceleration outward
    float travel = explosionStrength * explodeFactor * explodeFactor;

    // Apply outward motion
    uv += direction * travel;

    // Optional: Add rotation based on angle
    //float spin = explodeFactor * 5.0; // spin multiplier
    //uv = rotate2D(uv - vec2(0.5), spin) + vec2(0.5);

    // Discard out-of-bounds fragments
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        discard;
    }

    vec4 texColor = texture(ourTexture, uv);
    if (texColor.a < 0.01) discard;

    // Optional fade out
    float fade = clamp(1.0 - explodeFactor * 0.5, 0.0, 1.0);
    FragColor = vec4(texColor.rgb, texColor.a * fade * overallAlpha);
}
)glsl";
    const char *fragmentShader2 = R"glsl(#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float overallAlpha;
uniform float time;
uniform float explodeTime;
uniform float explosionStrength;

vec2 rotate2D(vec2 pt, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c) * pt;
}

void main() {
    vec2 uv = vTexCoord;

    // Explosion animation progress (0 to 1+)
    float explodeFactor = max(0.0, time - explodeTime);

    // Seed per-fragment motion direction and spin
    float seed = dot(uv, vec2(12.9898, 78.233));
    float angle = fract(sin(seed) * 43758.5453) * 6.2831; // 0..2π
    vec2 direction = vec2(cos(angle), sin(angle));

    // Strong acceleration outward
    float travel = explosionStrength * explodeFactor * explodeFactor;

    // Apply outward motion
    uv += direction * travel;

    // Optional: Add rotation based on angle
    //float spin = explodeFactor * 5.0; // spin multiplier
    //uv = rotate2D(uv - vec2(0.5), spin) + vec2(0.5);

    // Discard out-of-bounds fragments
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        discard;
    }

    vec4 texColor = texture(ourTexture, uv);
    if (texColor.a < 0.01) discard;

    // Optional fade out
    float fade = clamp(1.0 - explodeFactor * 0.5, 0.0, 1.0);
    FragColor = vec4(texColor.rgb, texColor.a * fade * overallAlpha);
}
)glsl";

};

