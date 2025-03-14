R""(
#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UX3D_MATH_PI 3.1415926535897932384626433832795
#define UX3D_MATH_INV_PI (1.0 / UX3D_MATH_PI)

layout(set = 0, binding = 0) uniform sampler2D uPanorama;
layout(set = 0, binding = 1) uniform samplerCube uCubeMap;
layout(set = 0, binding = 2) uniform uSH9 {
    vec4 coefficients[9];    
};


// enum
const uint cLambertian = 0;
const uint cGGX = 1;
const uint cCharlie = 2;
const uint cGGXCubeMap = 3;

layout(push_constant) uniform FilterParameters {
  float roughness;
  uint sampleCount;
  uint currentMipLevel;
  uint width;
  float lodBias;
  uint distribution; // enum
} pFilterParameters;

layout (location = 0) in vec2 inUV;

// output cubemap faces
layout(location = 0) out vec4 outFace0;
layout(location = 1) out vec4 outFace1;
layout(location = 2) out vec4 outFace2;
layout(location = 3) out vec4 outFace3;
layout(location = 4) out vec4 outFace4;
layout(location = 5) out vec4 outFace5;

layout(location = 6) out vec3 outLUT;

void writeFace(int face, vec3 colorIn)
{
	vec4 color = vec4(colorIn.rgb, 1.0f);
	
	if(face == 0)
		outFace0 = color;
	else if(face == 1)
		outFace1 = color;
	else if(face == 2)
		outFace2 = color;
	else if(face == 3)
		outFace3 = color;
	else if(face == 4)
		outFace4 = color;
	else //if(face == 5)
		outFace5 = color;
}

vec3 uvToXYZ(int face, vec2 uv)
{
    if(face == 0)
        return vec3(     1.f,   uv.y,    -uv.x);

    else if(face == 1)
        return vec3(    -1.f,   uv.y,     uv.x);

    else if(face == 2)
        return vec3(   +uv.x,   -1.f,    +uv.y);

    else if(face == 3)
        return vec3(   +uv.x,    1.f,    -uv.y);

    else if(face == 4)
        return vec3(   +uv.x,   uv.y,      1.f);

    else {//if(face == 5)
        return vec3(    -uv.x,  +uv.y,     -1.f);}
}

vec2 dirToUV(vec3 dir)
{
    return vec2(
            0.5f + 0.5f * atan(dir.z, dir.x) / UX3D_MATH_PI,
            1.f - acos(dir.y) / UX3D_MATH_PI);
}

float saturate(float v)
{
    return clamp(v, 0.0f, 1.0f);
}

vec3 sample_sh(const vec3 direction) {
    float x = direction.x;
    float y = direction.y;
    float z = direction.z;

    // Evaluate spherical harmonics basis functions
    // https://en.wikipedia.org/wiki/Table_of_spherical_harmonics
    float Y00 = 0.282095; // 1/(2*sqrt(pi))

    float Y1_1 = 0.488603 * y; // sqrt(3/(4pi)) * y
    float Y10  = 0.488603 * z; // sqrt(3/(4pi)) * z
    float Y11  = 0.488603 * x; // sqrt(3/(4pi)) * x

    float Y2_2 = 1.092548 * x * y; // sqrt(15/(4pi)) * x * y
    float Y2_1 = 1.092548 * y * z; // sqrt(15/(4pi)) * y * z
    float Y21  = 1.092548 * x * z; // sqrt(15/(4pi)) * x * z
    float Y20  = 0.315392 * (3.0 * z * z - 1.0); // sqrt(5/(16pi)) * (3z^2 - 1)
    float Y22  = 0.546274 * (x * x - y * y); // sqrt(15/(16pi)) * (x^2 - y^2)

    // Combine coefficients with basis functions
    vec3 color = vec3(0.0, 0.0, 0.0);
    for (int i = 0; i < 3; i++) {
        float sum = coefficients[0][i] * Y00;   // L_{00}

        sum += coefficients[1][i] * Y1_1;       // L_{1-1}
        sum += coefficients[2][i] * Y10;        // L_{10}
        sum += coefficients[3][i] * Y11;        // L_{11}
        
        sum += coefficients[4][i] * Y2_2;       // L_{2-2}
        sum += coefficients[5][i] * Y2_1;       // L_{2-1}
        sum += coefficients[6][i] * Y20;        // L_{20}
        sum += coefficients[7][i] * Y21;        // L_{21}
        sum += coefficients[8][i] * Y22;        // L_{22}

        color[i] = sum;
    }

    return color;
}

// Hammersley Points on the Hemisphere
// CC BY 3.0 (Holger Dammertz)
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// with adapted interface
float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// hammersley2d describes a sequence of points in the 2d unit square [0,1)^2
// that can be used for quasi Monte Carlo integration
vec2 hammersley2d(int i, int N) {
    return vec2(float(i)/float(N), radicalInverse_VdC(uint(i)));
}

// Hemisphere Sample

// TBN generates a tangent bitangent normal coordinate frame from the normal
// (the normal must be normalized)
mat3 generateTBN(vec3 normal)
{
    vec3 bitangent = vec3(0.0, 1.0, 0.0);

    float NdotUp = dot(normal, vec3(0.0, 1.0, 0.0));
    float epsilon = 0.0000001;
    if (1.0 - abs(NdotUp) <= epsilon)
    {
        // Sampling +Y or -Y, so we need a more robust bitangent.
        if (NdotUp > 0.0)
        {
            bitangent = vec3(0.0, 0.0, 1.0);
        }
        else
        {
            bitangent = vec3(0.0, 0.0, -1.0);
        }
    }

    vec3 tangent = normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);

    return mat3(tangent, bitangent, normal);
}

struct MicrofacetDistributionSample
{
    float pdf;
    float cosTheta;
    float sinTheta;
    float phi;
};

float D_GGX(float NdotH, float roughness) {
    float a = NdotH * roughness;
    float k = roughness / (1.0 - NdotH * NdotH + a * a);
    return k * k * (1.0 / UX3D_MATH_PI);
}

// GGX microfacet distribution
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
// This implementation is based on https://bruop.github.io/ibl/,
//  https://www.tobias-franke.eu/log/2014/03/30/notes_on_importance_sampling.html
// and https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
MicrofacetDistributionSample GGX(vec2 xi, float roughness)
{
    MicrofacetDistributionSample ggx;

    // evaluate sampling equations
    float alpha = roughness * roughness;
    ggx.cosTheta = saturate(sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y)));
    ggx.sinTheta = sqrt(1.0 - ggx.cosTheta * ggx.cosTheta);
    ggx.phi = 2.0 * UX3D_MATH_PI * xi.x;

    // evaluate GGX pdf (for half vector)
    ggx.pdf = D_GGX(ggx.cosTheta, alpha);

    // Apply the Jacobian to obtain a pdf that is parameterized by l
    // see https://bruop.github.io/ibl/
    // Typically you'd have the following:
    // float pdf = D_GGX(NoH, roughness) * NoH / (4.0 * VoH);
    // but since V = N => VoH == NoH
    ggx.pdf /= 4.0;

    return ggx;
}

// NDF
float D_Ashikhmin(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    // Ashikhmin 2007, "Distribution-based BRDFs"
    float a2 = alpha * alpha;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    float sin4h = sin2h * sin2h;
    float cot2 = -cos2h / (a2 * sin2h);
    return 1.0 / (UX3D_MATH_PI * (4.0 * a2 + 1.0) * sin4h) * (4.0 * exp(cot2) + sin4h);
}

// NDF
float D_Charlie(float sheenRoughness, float NdotH)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float invR = 1.0 / sheenRoughness;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * UX3D_MATH_PI);
}


MicrofacetDistributionSample Charlie(vec2 xi, float roughness)
{
    MicrofacetDistributionSample charlie;

    float alpha = roughness * roughness;
    charlie.sinTheta = pow(xi.y, alpha / (2.0*alpha + 1.0));
    charlie.cosTheta = sqrt(1.0 - charlie.sinTheta * charlie.sinTheta);
    charlie.phi = 2.0 * UX3D_MATH_PI * xi.x;

    // evaluate Charlie pdf (for half vector)
    charlie.pdf = D_Charlie(alpha, charlie.cosTheta);

    // Apply the Jacobian to obtain a pdf that is parameterized by l
    charlie.pdf /= 4.0;

    return charlie;
}

MicrofacetDistributionSample Lambertian(vec2 xi, float roughness)
{
    MicrofacetDistributionSample lambertian;

    // Cosine weighted hemisphere sampling
    // http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations.html#Cosine-WeightedHemisphereSampling
    lambertian.cosTheta = sqrt(1.0 - xi.y);
    lambertian.sinTheta = sqrt(xi.y); // equivalent to `sqrt(1.0 - cosTheta*cosTheta)`;
    lambertian.phi = 2.0 * UX3D_MATH_PI * xi.x;

    lambertian.pdf = lambertian.cosTheta / UX3D_MATH_PI; // evaluation for solid angle, therefore drop the sinTheta

    return lambertian;
}


// getImportanceSample returns an importance sample direction with pdf in the .w component
vec4 getImportanceSample(int sampleIndex, vec3 N, float roughness)
{
    // generate a quasi monte carlo point in the unit square [0.1)^2
    vec2 xi = hammersley2d(sampleIndex, int(pFilterParameters.sampleCount));

    MicrofacetDistributionSample importanceSample;

    // generate the points on the hemisphere with a fitting mapping for
    // the distribution (e.g. lambertian uses a cosine importance)
    if(pFilterParameters.distribution == cLambertian)
    {
        importanceSample = Lambertian(xi, roughness);
    }
    else if(pFilterParameters.distribution == cGGX || pFilterParameters.distribution == cGGXCubeMap)
    {
        // Trowbridge-Reitz / GGX microfacet model (Walter et al)
        // https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html
        importanceSample = GGX(xi, roughness);
    }
    else if(pFilterParameters.distribution == cCharlie)
    {
        importanceSample = Charlie(xi, roughness);
    }

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    vec3 localSpaceDirection = normalize(vec3(
        importanceSample.sinTheta * cos(importanceSample.phi), 
        importanceSample.sinTheta * sin(importanceSample.phi), 
        importanceSample.cosTheta
    ));
    mat3 TBN = generateTBN(N);
    vec3 direction = TBN * localSpaceDirection;

    return vec4(direction, importanceSample.pdf);
}

// Mipmap Filtered Samples (GPU Gems 3, 20.4)
// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
// https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
float computeLod(float pdf)
{
    // removed comments, look at original repo

    // We achieved good results by using the original formulation from Krivanek & Colbert adapted to cubemaps

    // https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
    float lod = 0.5 * log2( 6.0 * float(pFilterParameters.width) * float(pFilterParameters.width) / (float(pFilterParameters.sampleCount) * pdf));


    return lod;
}

vec3 filterColor(vec3 N)
{
    //return  textureLod(uCubeMap, N, 3.0).rgb;
    vec3 color = vec3(0.f);
    float weight = 0.0f;

    for(int i = 0; i < int(pFilterParameters.sampleCount); ++i)
    {
        vec4 importanceSample = getImportanceSample(i, N, pFilterParameters.roughness);

        vec3 H = vec3(importanceSample.xyz);
        float pdf = importanceSample.w;

        // mipmap filtered samples (GPU Gems 3, 20.4)
        float lod = computeLod(pdf);

        // apply the bias to the lod
        lod += pFilterParameters.lodBias;

        if(pFilterParameters.distribution == cLambertian)
        {
            // sample lambertian at a lower resolution to avoid fireflies
            // vec3 lambertian = textureLod(uCubeMap, H, lod).rgb;
            // Use spherical harmonics for diffuse
            vec3 lambertian = sample_sh(H);
            

            color += lambertian;
        }
        else if(pFilterParameters.distribution == cGGX || pFilterParameters.distribution == cGGXCubeMap || pFilterParameters.distribution == cCharlie)
        {
            // Note: reflect takes incident vector.
            vec3 V = N;
            vec3 L = normalize(reflect(-V, H));
            float NdotL = dot(N, L);

            if (NdotL > 0.0)
            {
                if(pFilterParameters.roughness == 0.0)
                {
                    // without this the roughness=0 lod is too high
                    lod = pFilterParameters.lodBias;
                }

                vec3 sampleColor = textureLod(uCubeMap, L, lod).rgb;

                color += sampleColor * NdotL;
                weight += NdotL;
            }
        }
    }

    if(weight != 0.0f)
    {
        color /= weight;
    }
    else
    {
        color /= float(pFilterParameters.sampleCount);
    }

    return color.rgb ;
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
    float a2 = pow(roughness, 4.0);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

// https://github.com/google/filament/blob/master/shaders/src/brdf.fs#L136
float V_Ashikhmin(float NdotL, float NdotV)
{
    return clamp(1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV)), 0.0, 1.0);
}

// Compute LUT for GGX distribution.
// See https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec3 LUT(float NdotV, float roughness)
{
    // Compute spherical view vector: (sin(phi), 0, cos(phi))
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    // The macro surface normal just points up.
    vec3 N = vec3(0.0, 0.0, 1.0);

    // removed comments, look at original repo
    float A = 0.0;
    float B = 0.0;
    float C = 0.0;

    for(int i = 0; i < int(pFilterParameters.sampleCount); ++i)
    {
        // Importance sampling, depending on the distribution.
        vec4 importanceSample = getImportanceSample(i, N, roughness);
        vec3 H = importanceSample.xyz;
        // float pdf = importanceSample.w;
        vec3 L = normalize(reflect(-V, H));

        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        if (NdotL > 0.0)
        {
            if (pFilterParameters.distribution == cGGX || pFilterParameters.distribution == cGGXCubeMap)
            {

                // Taken from: https://bruop.github.io/ibl
                // Shadertoy: https://www.shadertoy.com/view/3lXXDB
                // Terms besides V are from the GGX PDF we're dividing by.
                float V_pdf = V_SmithGGXCorrelated(NdotV, NdotL, roughness) * VdotH * NdotL / NdotH;
                float Fc = pow(1.0 - VdotH, 5.0);
                A += (1.0 - Fc) * V_pdf;
                B += Fc * V_pdf;
                C += 0.0;
            }

            if (pFilterParameters.distribution == cCharlie)
            {
                // LUT for Charlie distribution.
                float sheenDistribution = D_Charlie(roughness, NdotH);
                float sheenVisibility = V_Ashikhmin(NdotL, NdotV);

                A += 0.0;
                B += 0.0;
                C += sheenVisibility * sheenDistribution * NdotL * VdotH;
            }
        }
    }

    // removed comments, look at the original repo
    return vec3(4.0 * A, 4.0 * B, 4.0 * 2.0 * UX3D_MATH_PI * C) / float(pFilterParameters.sampleCount);
}


// entry point
void panoramaToCubeMap() 
{
	for(int face = 0; face < 6; ++face)
	{		
		vec3 scan = uvToXYZ(face, inUV*2.0-1.0);		
			
		vec3 direction = normalize(scan);		
	
		vec2 src = dirToUV(direction);		
			
		writeFace(face, texture(uPanorama, src).rgb);
	}
}

// entry point
void filterCubeMap() 
{
	vec2 newUV = inUV * float(1 << (pFilterParameters.currentMipLevel));
	 
	newUV = newUV*2.0-1.0;

    float angle = radians(90.0f);
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);
	
	for(int face = 0; face < 6; ++face)
	{
		vec3 scan = uvToXYZ(face, newUV);		
			
		vec3 direction = normalize(scan);	
		direction.y = -direction.y;

        vec3 rotateDir = vec3(direction.x*cosTheta + direction.z*sinTheta,
                              direction.y,
                              -direction.x*sinTheta + direction.z*cosTheta);

		writeFace(face, filterColor(rotateDir));
	}

	if (pFilterParameters.currentMipLevel == 0)
	{
		
		outLUT = LUT(inUV.x, inUV.y);
	
	}
}
)""
