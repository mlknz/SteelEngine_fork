#version 460
#extension GL_GOOGLE_include_directive : require

#define SHADER_STAGE fragment
#pragma shader_stage(fragment)

#include "Common/Common.glsl"

layout(push_constant) uniform PushConstants{
    layout(offset = 4) float pointLightsIntensity;
};

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(ToneMapping(inColor * pointLightsIntensity), 1.0);
}