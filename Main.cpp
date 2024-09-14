/*
This is a simple demo of rendering fonts using stb_truetype.h

The following example demonstrates to render text using batched rendering 
i.e., rendering a huge number of quads in one batch rather than once for every quad.

The following demostration doesn't use Element Buffers for the sake of simplicity, the same can be
extended to save a little bit of VRAM

The following file is divided into following sections, search the following keywords 
to reach the following sections: 

Sections
    [INCLUDES]
    [GLOBAL SETTINGS]
    [SHADER SOURCES]
    [DATA SECTION]
    [RENDERER]
    [OPENGL]
    [GLFW WINDOW CODE]
    [MAIN FUNCTION]
*/


// INCLUDES: ------------------------------------------------------------------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// GLOBAL SETTINGS: ------------------------------------------------------------------------------------------------------------------------------
std::string textToDisplay = "This is some text rendered in OpenGL.";
std::string fontFilePath = "fonts/sui.ttf";

// Font Atlas settings:
const uint32_t codePointOfFirstChar = 32;      // ASCII of ' '(Space)
const uint32_t charsToIncludeInFontAtlas = 95; // Include 95 charecters

const uint32_t fontAtlasWidth = 512;
const uint32_t fontAtlasHeight = 512;

// Window Settings:
constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

const std::string title = "stb_truetype_example";
// ------------------------------------------------------------------------------------------------------------------------------------------------

// SHADER SOURCES: --------------------------------------------------------------------------------------------------------------------------------
std::string vertexShaderSrc = R"(
#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec4 color;
out vec2 texCoord;

uniform mat4 uViewProjectionMat;

void main()
{
    gl_Position = uViewProjectionMat * vec4(aPosition, 1.0);

    color = aColor;
    texCoord = aTexCoord;
}
)";

std::string fragmentShaderSrc = R"(
#version 330 core

in vec4 color;
in vec2 texCoord;

uniform sampler2D uFontAtlasTexture;

out vec4 fragColor;

void main()
{
    fragColor = vec4(texture(uFontAtlasTexture, texCoord).r) * color;
}

)";
// ------------------------------------------------------------------------------------------------------------------------------------------------

// DATA SECTION: ----------------------------------------------------------------------------------------------------------------------------------
// Struct that represents a vertex drawn
struct Vertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 texCoord;
};

// A struct to hold all the useful stuff. Used this instead declaring them globally.
struct LocalState
{
    GLFWwindow* window;
    uint32_t currentWindowHeight;
    
    // Renderer data: ------------------------
    std::vector<Vertex> vertices;
    uint32_t vertexIndex;

    // Font Data: (This is the data required to render a quad for each glyph)
    stbtt_packedchar packedChars[charsToIncludeInFontAtlas];
    stbtt_aligned_quad alignedQuads[charsToIncludeInFontAtlas];

    glm::mat4 viewProjectionMat;

    // OpenGL Renderer IDs: ----------------
    uint32_t vaoID, vboID, eboID;
    uint32_t shaderProgramID;
    uint32_t fontTextureID;

}localState;

static void SetupVAOAndVBO();
static void SetupShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource);
static void SetupFontTexture(uint32_t fontAtlasWidth, uint32_t fontAtlasHeight, void* textureData, size_t size);
static void Render(const std::vector<Vertex>& vertices);
static void BindFontTexture(uint8_t slot);
static void UseShaderProgram(bool yes);

// RENDERER: ------------------------------------------------------------------------------------------------------------------------------
static uint8_t* SetupFont(const std::string& fontFile)
{
    // Read the font file
    std::ifstream inputStream(fontFile.c_str(), std::ios::binary);

    inputStream.seekg(0, std::ios::end);
    auto&& fontFileSize = inputStream.tellg();
    inputStream.seekg(0, std::ios::beg);

    uint8_t* fontDataBuf = new uint8_t[fontFileSize];

    inputStream.read((char*)fontDataBuf, fontFileSize);

    stbtt_fontinfo fontInfo = {};

    uint32_t fontCount = stbtt_GetNumberOfFonts(fontDataBuf);
    std::cout << "Font File: " << fontFile << " has " << fontCount << " fonts\n";

    if(!stbtt_InitFont(&fontInfo, fontDataBuf, 0))
        std::cerr << "stbtt_InitFont() Failed!\n";

    uint8_t* fontAtlasTextureData = new uint8_t[fontAtlasWidth * fontAtlasHeight];

    float fontSize = 64.0f;
    
    stbtt_pack_context ctx;

    stbtt_PackBegin(
        &ctx,                                     // stbtt_pack_context (this call will initialize it) 
        (unsigned char*)fontAtlasTextureData,     // Font Atlas texture data
        fontAtlasWidth,                           // Width of the font atlas texture
        fontAtlasHeight,                          // Height of the font atlas texture
        0,                                        // Stride in bytes
        1,                                        // Padding between the glyphs
        nullptr);

    stbtt_PackFontRange(
        &ctx,                                     // stbtt_pack_context
        fontDataBuf,                              // Font Atlas texture data
        0,                                        // Font Index                                 
        fontSize,                                 // Size of font in pixels. (Use STBTT_POINT_SIZE(fontSize) to use points) 
        codePointOfFirstChar,                     // Code point of the first charecter
        charsToIncludeInFontAtlas,                // No. of charecters to be included in the font atlas 
        localState.packedChars                    // stbtt_packedchar array, this struct will contain the data to render a glyph
    );
    stbtt_PackEnd(&ctx);

    for (int i = 0; i < charsToIncludeInFontAtlas; i++)
    {
        float unusedX, unusedY;

        stbtt_GetPackedQuad(
            localState.packedChars,              // Array of stbtt_packedchar
            fontAtlasWidth,                      // Width of the font atlas texture
            fontAtlasHeight,                     // Height of the font atlas texture
            i,                                   // Index of the glyph
            &unusedX, &unusedY,                  // current position of the glyph in screen pixel coordinates, (not required as we have a different corrdinate system)
            &localState.alignedQuads[i],         // stbtt_alligned_quad struct. (this struct mainly consists of the texture coordinates)
            0                                    // Allign X and Y position to a integer (doesn't matter because we are not using 'unusedX' and 'unusedY')
        );
    }

    delete[] fontDataBuf;

    // Optionally write the font atlas texture as a png file.
    stbi_write_png("fontAtlas.png", fontAtlasWidth, fontAtlasHeight, 1, fontAtlasTextureData, fontAtlasWidth);
    
    return fontAtlasTextureData;
}

static void RendererInit()
{
    SetupShaderProgram(vertexShaderSrc.c_str(), fragmentShaderSrc.c_str());
    SetupVAOAndVBO();

    uint8_t* fontTextureData = SetupFont(fontFilePath);
    SetupFontTexture(fontAtlasWidth, fontAtlasHeight, fontTextureData, fontAtlasWidth * fontAtlasHeight * sizeof(uint8_t));

    delete[] fontTextureData;

    UseShaderProgram(true);
}

static void DrawBegin()
{
    localState.vertexIndex = 0;
}

// Adds the required vertices and indices to render:
static void DrawText(const std::string& text, glm::vec3 position, glm::vec4 color, float size)
{
    int order[6] = { 0, 1, 2, 0, 2, 3 };
    float pixelScale = 2.0f / localState.currentWindowHeight;

    for(char ch : text)
    {
        if(localState.vertices.size() <= localState.vertexIndex)
            localState.vertices.resize(localState.vertices.size() + 6);

        // Retrive the data that is used to render a glyph of charecter 'ch'
        stbtt_packedchar* packedChar = &localState.packedChars[ch - codePointOfFirstChar]; 
        stbtt_aligned_quad* alignedQuad = &localState.alignedQuads[ch - codePointOfFirstChar];

        // The units of the fields of the above structs are in pixels, 
        // convert them to a unit of what we want be multilplying to pixelScale  
        glm::vec2 glyphSize = 
        {
            (packedChar->x1 - packedChar->x0) * pixelScale * size,
            (packedChar->y1 - packedChar->y0) * pixelScale * size
        };

        glm::vec2 glyphBoundingBoxBottomLeft = 
        {
            position.x + (packedChar->xoff * pixelScale * size),
            position.y - (packedChar->yoff + packedChar->y1 - packedChar->y0) * pixelScale * size
        };

        // The order of vertices of a quad goes top-right, top-left, bottom-left, bottom-right
        glm::vec2 glyphVertices[4] = 
        {
            { glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y + glyphSize.y },
            { glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y + glyphSize.y },
            { glyphBoundingBoxBottomLeft.x, glyphBoundingBoxBottomLeft.y },
            { glyphBoundingBoxBottomLeft.x + glyphSize.x, glyphBoundingBoxBottomLeft.y },
        };

        glm::vec2 glyphTextureCoords[4] = 
        {
            { alignedQuad->s1, alignedQuad->t0 },
            { alignedQuad->s0, alignedQuad->t0 },
            { alignedQuad->s0, alignedQuad->t1 },
            { alignedQuad->s1, alignedQuad->t1 },
        };

        // We need to fill the vertex buffer by 6 vertices to render a quad as we are rendering a quad as 2 triangles
        // The order used is in the 'order' array
        // order = [0, 1, 2, 0, 2, 3] is meant to represent 2 triangles: 
        // one by glyphVertices[0], glyphVertices[1], glyphVertices[2] and one by glyphVertices[0], glyphVertices[2], glyphVertices[3]
        for(int i = 0; i < 6; i++)
        {
            localState.vertices[localState.vertexIndex + i].position = glm::vec3(glyphVertices[order[i]], position.z);
            localState.vertices[localState.vertexIndex + i].color = color;
            localState.vertices[localState.vertexIndex + i].texCoord = glyphTextureCoords[order[i]];
        }

        localState.vertexIndex += 6;

        // Update the position to render the next glyph specified by packedChar->xadvance.
        position.x += packedChar->xadvance * pixelScale * size;
    }
}

static void RenderFrame()
{
    BindFontTexture(0);
    Render(localState.vertices);
}

static void SetupViewProjection(float aspectRatio)
{
    glm::mat4 projectionMat = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f);
    glm::mat4 viewMat = glm::mat4(1.0f);

    viewMat = glm::translate(viewMat, {0.0f, 0.0f, 0.0f});
    viewMat = glm::rotate(viewMat, 0.0f, {1, 0, 0});
    viewMat = glm::rotate(viewMat, 0.0f, {0, 1, 0});
    viewMat = glm::rotate(viewMat, 0.0f, {0, 0, 1});
    viewMat = glm::scale(viewMat, {1.0f, 1.0f, 1.0f});

    localState.viewProjectionMat = projectionMat * viewMat;
}


// OPENGL CODE: --------------------------------------------------------------------------------------------------------------------------------
// VBO size in bytes
const size_t VBO_SIZE = 600000 * sizeof(Vertex); // Size enough for 600000 vertices (100000 quads)

// The vertex array is set up the following way:
// Each vertex has consists of 9 floats, 
// -> first 3 determines the position of the vertex
// -> next 4 floats determines the color of the vertex
// -> next 2 floats determines the texture coordinates of the vertex.

static void SetupVAOAndVBO()
{
    // Setting up the VAO and VBO: -----------------------
    glGenBuffers(1, &localState.vboID);
    glBindBuffer(GL_ARRAY_BUFFER, localState.vboID);
    glBufferData(GL_ARRAY_BUFFER, VBO_SIZE, nullptr, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &localState.vaoID);
    glBindVertexArray(localState.vaoID);

    // position attribute:
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    // color attribute:
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (const void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // texCoord attribute:
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (const void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

static void SetupShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource)
{
    uint32_t vtxShaderID, fragShaderID;
    
    // Vertex Shader: -----------------------------------
    vtxShaderID = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vtxShaderID, 1, &vertexShaderSource, nullptr);
    glCompileShader(vtxShaderID);

    // Fragment Shader: ---------------------------------
    fragShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShaderID, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragShaderID);

    // Checking for shader compilation errors: -------------------
    GLint success;
    GLchar infoLog[512];

    glGetShaderiv(vtxShaderID, GL_COMPILE_STATUS, &success);
    if (!success) 
    {
        glGetShaderInfoLog(vtxShaderID, 512, NULL, infoLog);
        std::cerr << "Vertex Shader Compilation Error: " << infoLog << std::endl;
    }

    glGetShaderiv(fragShaderID, GL_COMPILE_STATUS, &success);
    if (!success) 
    {
        glGetShaderInfoLog(fragShaderID, 512, NULL, infoLog);
        std::cerr << "Fragment Shader Compilation Error: " << infoLog << std::endl;
    }

    // Linking the shaders into a shader program: ----------------------
    localState.shaderProgramID = glCreateProgram();
    glAttachShader(localState.shaderProgramID, vtxShaderID);
    glAttachShader(localState.shaderProgramID, fragShaderID);

    glLinkProgram(localState.shaderProgramID);

    // Check for linking errors: ---------------------------------------
    glGetProgramiv(localState.shaderProgramID, GL_LINK_STATUS, &success);
    if (!success) 
    {
        glGetProgramInfoLog(localState.shaderProgramID, 512, NULL, infoLog);
        std::cerr << "Shader Program Linking Error: " << infoLog << std::endl;
    }
}

static void UseShaderProgram(bool use)
{
    glUseProgram(use ? localState.shaderProgramID : 0);
}

static void SetupFontTexture(uint32_t textureWidth, uint32_t textureHeight, void* textureData, size_t size)
{
    glGenTextures(1, &localState.fontTextureID);
    glBindTexture(GL_TEXTURE_2D, localState.fontTextureID);

    // The given texture data is a single channel 1 byte per pixel data 
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, textureWidth, textureHeight, 0, GL_RED, GL_UNSIGNED_BYTE, textureData);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void BindFontTexture(uint8_t slot)
{
    glBindTexture(GL_TEXTURE_2D, localState.fontTextureID);
    glActiveTexture(GL_TEXTURE0 + slot);

    int uniformLoc = glGetUniformLocation(localState.shaderProgramID, "uFontAtlasTexture");
    glUniform1i(uniformLoc, (int)slot);
}

static void Render(const std::vector<Vertex>& vertices)
{
    // The vertex buffer need to be divided into chunks of size 'VBO_SIZE',
    // Upload them to the VBO and render
    // This is repeated for every divided chunk of the vertex buffer.

    size_t sizeOfVertices = vertices.size() * sizeof(Vertex);
    uint32_t drawCallCount = (sizeOfVertices / VBO_SIZE) + 1; // aka number of chunks.

    // Render each chunk of vertex data.
    for(int i = 0; i < drawCallCount; i++)
    {
        const Vertex* data = vertices.data() + i * VBO_SIZE;
        
        uint32_t vertexCount = 
            i == drawCallCount - 1 ? 
            (sizeOfVertices % VBO_SIZE) / sizeof(Vertex): 
            VBO_SIZE / (sizeof(Vertex) * 6);

        int uniformLocation = glGetUniformLocation(localState.shaderProgramID, "uViewProjectionMat");
        glUniformMatrix4fv(uniformLocation, 1, GL_TRUE, glm::value_ptr(localState.viewProjectionMat));

        glBindVertexArray(localState.vaoID);
        glBindBuffer(GL_ARRAY_BUFFER, localState.vboID);
        glBufferSubData(GL_ARRAY_BUFFER, 
            0, 
            i == drawCallCount - 1 ? sizeOfVertices % VBO_SIZE : VBO_SIZE,
            data);

        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------


// GLFW WINDOW CODE: -------------------------------------------------------------------------------------------------------------
static void SetupWindowAndContext(uint32_t windowWidth, uint32_t windowHeight, const std::string& title)
{
    // Initialize the window:
    if(!glfwInit())
    {
        std::cerr << "GLFW Failed to initialize!\n";
        exit(-1);
    }

    // Create the window
    localState.window = glfwCreateWindow(windowWidth, windowHeight, title.c_str(), nullptr, nullptr);

    // Center the window
    const GLFWvidmode* mainDisplayVidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(
        localState.window, 
        (mainDisplayVidMode->width - windowWidth) / 2, 
        (mainDisplayVidMode->height - windowHeight) / 2
    );

    glfwShowWindow(localState.window);
    glfwMakeContextCurrent(localState.window);

    int version = gladLoadGL(glfwGetProcAddress);
    std::cout << "Loaded: OpenGL " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version); 

    // Setup View Projection matrix:
    SetupViewProjection((float)windowWidth / (float)windowHeight);
    localState.currentWindowHeight = windowHeight;

    glfwSetWindowSizeCallback(localState.window, 
        [](GLFWwindow* window, int width, int height)
        {
            SetupViewProjection((float)width / (float)height);
            localState.currentWindowHeight = height;

            // Resize the viewport
            glViewport(0, 0, width, height);

        }
    );

    // Setup Alpha blending.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
}

static bool WindowShouldClose()
{
    return glfwWindowShouldClose(localState.window);
}

static void ClearWindow()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
}

static void UpdateWindow()
{
    glfwPollEvents();
    glfwSwapBuffers(localState.window);
}

static void Terminate()
{
    glfwDestroyWindow(localState.window);
    glfwTerminate();
}

// MAIN FUNCTION: -----------------------------------------------------------------------------------------------------------------------------------
int main()
{
    SetupWindowAndContext(WIDTH, HEIGHT, title);
    RendererInit();

    while (!WindowShouldClose())
    {
        ClearWindow();
        DrawBegin();

        // Add more DrawText() calls here.
        DrawText(textToDisplay, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.7f);
        DrawText("The color of text can be changed too!", { -0.5f, -0.4f, 0.0f }, { 0.1f, 0.5f, 1.0f, 1.0f }, 0.5f);
        DrawText("stb_truetype.h example", { -0.8f, 0.4f, 0.0f }, { 0.9f, 0.2f, 0.3f, 1.0f }, 1.0f);
        
        RenderFrame();
        UpdateWindow();
    }

    Terminate();
    return 0;
}