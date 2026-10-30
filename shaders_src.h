#pragma once

static const char* WORLD_VS = R"GLSL(
#version 120
uniform float uFogStart;
uniform float uFogEnd;
varying vec2  vUV;
varying vec4  vColor;
varying float vFog;
void main()
{
    vec4 view = gl_ModelViewMatrix * gl_Vertex;
    float d = length(view.xyz);
    vFog = clamp((d - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
    vUV = gl_MultiTexCoord0.xy;
    vColor = gl_Color;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)GLSL";

static const char* WORLD_FS = R"GLSL(
#version 120
uniform sampler2D uAtlas;
uniform vec3      uFogColor;
varying vec2  vUV;
varying vec4  vColor;
varying float vFog;
void main()
{
    vec4 tex = texture2D(uAtlas, vUV);
    if (tex.a < 0.1) discard;
    vec3 col = tex.rgb * vColor.rgb;
    col = mix(col, uFogColor, vFog);
    gl_FragColor = vec4(col, tex.a * vColor.a);
}
)GLSL";

static const char* WATER_VS = R"GLSL(
#version 120
uniform float uTime;
uniform vec3  uChunkOffset;
uniform float uFogStart;
uniform float uFogEnd;
varying vec2  vUV;
varying vec4  vColor;
varying float vFog;
varying vec3  vWorld;
void main()
{
    vec3 world = gl_Vertex.xyz + uChunkOffset;
    float w = sin(world.x * 0.9 + uTime * 1.6) * 0.05
            + cos(world.z * 1.1 + uTime * 1.3) * 0.04;
    vec4 pos = gl_Vertex;
    pos.y += w;
    vec4 view = gl_ModelViewMatrix * pos;
    float d = length(view.xyz);
    vFog = clamp((d - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
    vUV = gl_MultiTexCoord0.xy;
    vColor = gl_Color;
    vWorld = world;
    gl_Position = gl_ModelViewProjectionMatrix * pos;
}
)GLSL";

static const char* WATER_FS = R"GLSL(
#version 120
uniform sampler2D uAtlas;
uniform vec3      uFogColor;
uniform float     uTime;
varying vec2  vUV;
varying vec4  vColor;
varying float vFog;
varying vec3  vWorld;
void main()
{
    vec2 scaled = vUV * 8.0;
    vec2 tile = floor(scaled);
    vec2 local = fract(scaled) + vec2(uTime * 0.20, uTime * 0.10);
    vec2 animUV = (tile + fract(local)) / 8.0;
    vec4 tex = texture2D(uAtlas, animUV);
    if (tex.a < 0.05) discard;
    vec3 col = tex.rgb * vColor.rgb;
    float sparkle = sin(vWorld.x * 4.0 + uTime * 2.3) * cos(vWorld.z * 5.0 - uTime * 1.9);
    col += max(sparkle, 0.0) * 0.10 * vec3(0.5, 0.8, 1.0);
    col = mix(col, uFogColor, vFog);
    gl_FragColor = vec4(col, tex.a * vColor.a);
}
)GLSL";