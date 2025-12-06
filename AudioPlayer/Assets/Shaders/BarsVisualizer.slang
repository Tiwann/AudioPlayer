module BarsVisualizer;

struct Material
{
  	float time;
  	int fftSize;
    int channelCount;
}

[[vk::binding(0, 0)]] RWTexture2D texture;
[[vk::binding(1, 0)]] StructuredBuffer<float> frequencies;

[[vk::push_constant]] Material material;

static const float3 red = float3(1.0, 0.0, 0.0);
static const float3 green = float3(0.0, 1.0, 0.0);

bool isBetween(float v, float min, float max)
{
	return v >= min && v <= max;
}

float getSmoothedFrequency(int index, int radius)
{
    float sum = 0.0;
    int count = 0;

    for (int i = -radius; i <= radius; i++)
    {
        int idx = clamp(index + i, 0, material.fftSize - 1);
        sum += frequencies[idx];
        count++;
    }

    return sum / count;
}

float3 hue(float3 col, float hueAdjust)
{
    const float3 k = float3(0.57735, 0.57735, 0.57735);
    float cosAngle = cos(hueAdjust);
    return col * cosAngle + cross(k, col) * sin(hueAdjust) + k * dot(k, col) * (1.0 - cosAngle);
}

float3 drawBars(float2 uv)
{
  	float3 color;
  	int freqCount = material.fftSize / (2 * material.channelCount);
    int index = int(clamp(uv.x * (freqCount - 1.0), 0.0, (freqCount - 1.0)));
    float freq = getSmoothedFrequency(index, 2) * 0.25;
    float easedFreq = smoothstep(0.0, 1.0, freq);
    float halfFreq = easedFreq * 0.5;
    float mask = isBetween(uv.y, 0.5 - halfFreq, 0.5 + halfFreq) ? 1.0 : 0.0;
    float3 lowColor = hue(green, material.time);
    float3 highColor = hue(red, material.time);
    float3 grandient = lerp(lowColor, highColor, smoothstep(0.0, 1.0, uv.y)) * easedFreq;
    color = mask * grandient;

	return color;
}

float4 mainImage(float2 fragCoord, float2 texSize)
{
  	float2 uv = fragCoord / texSize;

    float3 color = float3(0.0);
    color += drawBars(uv);

    return float4(color, 1.0);
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

    float2 texSize = float2(texWidth, texHeight);
    texture[screenPos] = mainImage(screenPos, texSize);

}