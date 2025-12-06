module CubeVisualizer;
/* @kishimisu - 2023

   This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License (https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en)

   An audio-reactive scene that maps the frequencies of the input music
   and the audio volume to different cells, colors, size and intensity!

   I've been struggling to complete this scene as I wanted to repeat
   the space with random variations for each cell. There's a wonderful
   tutorial by Blackle Mori explaining how to achieve this (https://www.youtube.com/watch?v=I8fmkLK1OKg) ,
   however I'm using an accumulation technique for the lighting with a
   fixed number of steps (30) which gets broken with this new technique.

   I decided to keep a reasonable random variation amount to prevent having
   raymarching artifacts that are too visible. I couldn't get totally rid of them,
   however with this kind of audio reactive scene it seems to be more acceptable
   as there's a lot of rapid movements!
*/

typealias vec2 = float2;
typealias vec3 = float3;
typealias vec4 = float4;
typealias mat2 = float2x2;
typealias mat3 = float3x3;

struct ShaderData
{
    float time;
}

struct AudioData
{
    float frequencies[256];
}

[[vk::push_constant]] ShaderData data;
[[vk::binding(0, 0)]] RWTexture2D texture;
[[vk::binding(1, 0)]] ConstantBuffer<AudioData> audioData;

#define light(d, att) ((1.0) / (1.0 + pow(abs(d * att), 1.3)))
#define logX(x, a, c) (1.0 / (exp(-a * (x - c)) + 1.0))

inline float getLevel(float x)
{
    // Convert x ∈ [0,1] into an index
    uint idx = (uint)(x * 256);

    // Clamp to avoid out-of-range access
    idx = min(idx, 256 - 1);

    return audioData.frequencies[idx];
}

float logisticAmp(float amp)
{
   float c = .88, a = 20.;
   return (logX(amp, a, c) - logX(0.0, a, c)) / (logX(1.0, a, c) - logX(0.0, a, c));
}

float getPitch(float freq, float octave)
{
	freq = pow(2.0, freq)   * 261.0;
    freq = pow(2.0, octave) * freq / 12000.0;
    return logisticAmp(getLevel(freq));
}

float getVol(float samples)
{
	float avg = 0.0;
	for (float i = 0.0; i < samples; i++)
		avg += getLevel(i / samples);
	return avg / samples;
}

float sdBox(vec3 p, vec3 b)
{
	vec3 q = abs(p) - b;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float hash13(vec3 p3)
{
	p3 = fract(p3 * 0.1031);
	p3 += dot(p3, p3.zyx + 31.32);
	return fract((p3.x + p3.y) * p3.z);
}

vec2 rotate(vec2 v, float angle)
{
	float c = cos(angle);
    float s = sin(angle);
    return mul(v, mat2(c, s, -s, c));
}

vec4 mainImage(vec2 fragCoord, vec2 texSize)
{
    vec2 uv = (2.0 * fragCoord - texSize.xy) / texSize.y;
    vec3 col = vec3(0.05, 0.0, 0.14);
    float vol = 1.0;

    vec3 ro = vec3(0, 8, 12) * (1.0 + vol * 0.3);
    ro.zx = rotate(ro.zx, data.time * 0.4);
    vec3 f = normalize(-ro);
    vec3 r = normalize(cross(vec3(0, 1, 0), f));
    vec3 rd = normalize(f + uv.x * r + uv.y * cross(f, r));


    for (float i = 0.0, t = 0.0; i < 30.0; i++)
    {
        vec3 p = ro + t * rd;

        vec2 cen = floor(p.xz) + 0.5;
        vec3 id = abs(vec3(cen.x, 0, cen.y));
        float d = length(id);

        float freq = smoothstep(0.0, 20.0, d) * 3.0 + hash13(id) * 2.0;
        float pitch = getPitch(freq, 0.7);

        float v = vol * smoothstep(2.0, 0.0, d);
        float h = d * 0.2 * (1.0 + pitch * 1.5) + v * 2.0;
        float me = sdBox(p - vec3(cen.x, -50.0, cen.y), vec3(0.3, 50.0 + h, 0.3) + pitch) - 0.05;

        col += lerp(lerp(vec3(0.8, 0.2, 0.4), vec3(0, 1, 0), min(v * 2.0, 1.0)), vec3(0.5, 0.3, 1.2), smoothstep(10.0, 30.0, d))
               * (cos(id) + 1.5)
               * (pitch * d * 0.08 + v)
               * light(me, 20.) * (1.0 + vol * 2.0);

        t += me;
    }

    return vec4(col, 1.0);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void compute(uint3 threadId: SV_DispatchThreadID)
{
	uint2 screenPos = threadId.xy;
	float texWidth = 0, texHeight = 0;
	texture.GetDimensions(texWidth, texHeight);

	if (screenPos.x >= texWidth || screenPos.y >= texHeight)
		return;

	texture[screenPos] = saturate(mainImage(screenPos, float2(texWidth, texHeight)));
}