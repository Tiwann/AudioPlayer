module EclipseVisualizer;
// Credit: https://www.shadertoy.com/view/4tGXzt

static const float PI = 3.14159265;

typealias vec2 = float2;
typealias vec3 = float3;
typealias vec4 = float4;
typealias mat2 = float2x2;
typealias mat3 = float3x3;

struct MaterialParameters
{
	float iTime;
	float freqRange = 64.0;
	float radius = 0.6;
	float brightness = 0.2;
	float speed = 0.2;
}

struct AudioData
{
    float frequencies[2048];
}

[[vk::push_constant]] uniform MaterialParameters material;

[[vk::binding(0, 0)]] RWTexture2D<float4> texture;
[[vk::binding(1, 0)]] ConstantBuffer<AudioData> audioData;


// Convert HSV to RGB
vec3 hsv2rgb(vec3 color)
{
	vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(color.xxx + k.xyz) * 6.0 - k.www);
	return color.z * lerp(k.xxx, clamp(p - k.xxx, 0.0, 1.0), color.y);
}

float luma(vec3 color)
{
	return dot(color, vec3(0.299, 0.587, 0.114));
}

float getfrequency(float x)
{
    int index = int(floor(x * material.freqRange + 1.0));
    return audioData.frequencies[index] + 0.06;
}

float getfrequency_smooth(float x)
{
    float index = floor(x * material.freqRange) / material.freqRange;
    float next = floor(x * material.freqRange + 1.0) / material.freqRange;
    return lerp(getfrequency(index), getfrequency(next), smoothstep(0.0, 1.0, fract(x * material.freqRange)));
}

float getfrequency_blend(float x)
{
	return lerp(getfrequency(x), getfrequency_smooth(x), 0.5);
}

vec3 doHalo(vec2 fragment, float radius) 
{
	float dist = length(fragment);
	float ring = 1.0 / (abs(dist - radius) + 0.005);

    float b = dist < radius ? material.brightness * 0.6 : material.brightness;

    vec3 col = vec3(0.0);
    float angle = atan2(fragment.y, fragment.x);
	col += hsv2rgb( vec3( ( angle + material.iTime * 0.25 ) / (PI * 2.0), 0.6, 0.5 ) ) * ring * b;

    float frequency = max(getfrequency(abs(angle / PI)) - 0.02, 0.0);
	col *= frequency * 0.5;
	
	// Black halo
	col *= smoothstep(radius * 0.86, radius, dist);
	
	return col;
}

vec3 doLine(vec2 fragment, float radius, float x) {
	vec3 col = hsv2rgb(vec3(x * 0.23 + material.iTime * 0.12, 1.0, 1.0));
	
	float freq = abs(fragment.x * 0.5);

    col *= (1.0 / abs(fragment.y)) * material.brightness * getfrequency(freq);	
	col = col * smoothstep(radius, radius * 1.8, abs(fragment.x));
	
	return col;
}

vec2 rotate(vec2 v, float angle)
{
	float c = cos(angle);
    float s = sin(angle);
    return mul(v, mat2(c, s, -s, c));
}

vec4 toLinear(vec4 color)
{
	color.x = color.x <= 0.04045 ? color.x / 12.92 : pow((color.x + 0.055) / 1.055, 2.4);
    color.y = color.y <= 0.04045 ? color.y / 12.92 : pow((color.y + 0.055) / 1.055, 2.4);
    color.z = color.z <= 0.04045 ? color.z / 12.92 : pow((color.z + 0.055) / 1.055, 2.4);
    return color;
}

vec3 reinhard(vec3 v)
{
    return v / (1.0f + v);
}

static const mat3 aces_input_matrix =
    {
        vec3(0.59719f, 0.35458f, 0.04823f),
        vec3(0.07600f, 0.90834f, 0.01566f),
        vec3(0.02840f, 0.13383f, 0.83777f)
    };

static const mat3 aces_output_matrix =
    {
        vec3(1.60475f, -0.53108f, -0.07367f),
        vec3(-0.10208f, 1.10813f, -0.00605f),
        vec3(-0.00327f, -0.07276f, 1.07602f)
    };

vec3 rtt_and_odt_fit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 aces_fitted(vec3 v)
{
    v = mul(aces_input_matrix, v);
    v = rtt_and_odt_fit(v);
    return mul(aces_output_matrix, v);
}

vec4 mainImage(vec2 fragCoord, vec2 texSize)
{
    vec2 fragPos = fragCoord / texSize.xy;
    fragPos = (fragPos - 0.5) * 2.0;
    fragPos.x *= texSize.x / texSize.y;

    vec3 color = vec3(0.0134, 0.052, 0.1);
    color += doHalo(fragPos, material.radius);

	/* if(material.showHorizontal)
	{
		vec2 rot = rotate(fragPos, material.iTime * material.speed);
        color += doLine(rot, material.radius, rot.x);
	} */

    color += max(luma(color) - 1.0, 0.0);
    return vec4(aces_fitted(color), 1.0);
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
	float4 color = mainImage(screenPos, texSize);
	texture[screenPos] = color;
}