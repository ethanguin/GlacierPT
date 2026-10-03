#include "raycommon.hlsli"

// Tuning
static const uint  SPP_SMOOTH        = 1;    // pixels that never hit a rough surface
static const uint  SPP_MEDIUM        = 4;    // mildly rough lobes
static const uint  SPP_ROUGH         = 8;    // very rough lobes
static const float LOBE_MEDIUM_MIN   = 0.0;  // lobe >  MIN_ROUGHNESS -> medium
static const float LOBE_ROUGH_MIN    = 0.30; // lobe >= this          -> rough
static const uint  SPLIT_MAX_DEPTH   = 1;    // below this depth rough hits split into both children; deeper hits pick one
static const float MAX_SAMPLE_RADIANCE = 10.0; // per-sample firefly clamp (slightly biased)

// One pending ray in the (depth-first) ray tree.
struct PathRay {
    float3 origin;
    float3 direction;
    float3 throughput;
    uint depth;
};

float MaxComponent(float3 v) {
    return max(v.x, max(v.y, v.z));
}

// R2 low-discrepancy sequence (Roberts). Cheap stratification for the first lobe sample.
float2 R2(uint n) {
    return frac(0.5 + float2(0.7548776662, 0.5698402910) * float(n));
}

// Direct lighting (all lights, hard shadows). N must face the viewer.
float3 EvaluateLights(RayPayload payload, float3 N, float3 V) {
    float3 direct = 0.0;

    for (uint lightIdx = 0; lightIdx < frameConstants.LightCount; ++lightIdx) {
        GPULight light = Lights[lightIdx];

        float3 L;
        float attenuation = 1.0;
        float shadowMaxT = 1000.0;

        if (light.type == LIGHT_TYPE_DIRECTIONAL) {
            L = normalize(-light.direction);
        } else {
            float3 toLight = light.position - payload.position;
            float dist = length(toLight);
            L = toLight / max(dist, 0.0001);
            shadowMaxT = dist;

            attenuation = saturate(1.0 - (dist * dist) / max(light.range * light.range, 0.0001));
            attenuation *= attenuation;

            if (light.type == LIGHT_TYPE_SPOT) {
                float cosAngle = dot(-L, normalize(light.direction));
                attenuation *= smoothstep(0.5, 0.9, cosAngle);
            }
        }

        float NoL = saturate(dot(N, L));
        if (NoL <= 0.0 || attenuation <= 0.0) {
            continue;
        }

        ShadowPayload shadowPayload;
        shadowPayload.isShadowed = true;

        RayDesc shadowRay;
        shadowRay.Origin = payload.position + N * 0.001;
        shadowRay.Direction = L;
        shadowRay.TMin = 0.001;
        shadowRay.TMax = shadowMaxT;

        uint shadowRayFlags = RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        TraceRay(Scene, shadowRayFlags, 0xFF, 0, 0, 1, shadowRay, shadowPayload);

        float visibility = shadowPayload.isShadowed ? 0.0 : 1.0;

        direct += EvaluateDirectLighting(N, V, L, payload.baseColor, payload.roughness, payload.metallic) * light.color.rgb * light.color.a * NoL *
                  visibility * attenuation;
    }

    return direct;
}

// Opaque surfaces get direct + ambient + (rough) specular bounce.
// Transmissive surfaces spawn a Fresnel-weighted reflection + refraction, both derived
// from ONE sampled GGX microfacet normal so rough glass blurs what you see through it.
//
//  seed        : per-pixel RNG state, advanced here
//  sampleIndex : index of this sample within the pixel (drives stratification)
//  rot         : per-pixel random rotation for the stratified sequence
//  lobe        : in/out, max roughness seen on a rough hit (used to pick spp)
float3 TracePath(float3 origin, float3 direction, inout uint seed, uint sampleIndex, float2 rot, inout float lobe) {
    PathRay stack[RAY_STACK_SIZE];
    uint sp = 0;

    PathRay cur;
    cur.origin = origin;
    cur.direction = direction;
    cur.throughput = 1.0;
    cur.depth = 0;

    float3 radiance = 0.0;
    uint raysTraced = 0;

    [loop] while (true) {
        bool alive = true;

        // Out of depth/ray budget: approximate the remainder with the sky.
        if (cur.depth > MAX_BOUNCES || raysTraced >= MAX_RAYS_PER_SAMPLE) {
            radiance += cur.throughput * GetSkyColor(cur.direction);
            alive = false;
        }

        if (alive) {
            RayDesc ray;
            ray.Origin = cur.origin;
            ray.Direction = cur.direction;
            ray.TMin = 0.001;
            ray.TMax = 1000.0;

            RayPayload payload = (RayPayload)0;
            payload.hitT = -1.0;

            TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
            raysTraced++;

            if (payload.hitT < 0.0) {
                radiance += cur.throughput * GetSkyColor(cur.direction);
                alive = false;
            } else {
                float T = saturate(payload.transmission);
                float3 V = -cur.direction;

                // Flip the normal to face the incoming ray; remember which side we hit.
                bool inside = dot(cur.direction, payload.normal) > 0.0;
                float3 N = inside ? -payload.normal : payload.normal;
                float NoV = saturate(dot(N, V));

                // Beer-Lambert: the segment we just travelled was inside the glass.
                if (inside && T > 0.0) {
                    float3 sigma = -log(max(payload.baseColor, 0.001));
                    cur.throughput *= exp(-sigma * payload.hitT * ABSORPTION_SCALE);
                }

                // Opaque portion: direct + ambient

                float3 direct = 0.0;
                if (T < 1.0) {
                    direct = EvaluateLights(payload, N, V);
                }

                float3 F0 = lerp(float3(0.04, 0.04, 0.04), payload.baseColor, payload.metallic);
                float3 F_env = FresnelSchlick(F0, NoV);
                float3 kD = (1.0 - F_env) * (1.0 - payload.metallic);
                float3 ambient = kD * payload.baseColor / PI * AmbientLight.color.rgb * AmbientLight.color.a;

                radiance += cur.throughput * (1.0 - T) * (direct + ambient);

                // Continuation: choose reflected / refracted directions + weights

                float etaI = inside ? payload.ior : 1.0;
                float etaT = inside ? 1.0 : payload.ior;

                float rough = clamp(payload.roughness, 0.0, MAX_ROUGHNESS);
                bool roughPath = rough > MIN_ROUGHNESS;

                float3 reflDir;
                float3 refrDir = cur.direction;
                float3 reflectWeight;
                float3 refractWeight = 0.0;

                if (roughPath) {
                    lobe = max(lobe, rough);

                    float alpha = rough * rough;

                    float3 t, b;
                    BuildONB(N, t, b);
                    float3 Ve = float3(dot(V, t), dot(V, b), NoV);

                    // Stratify the primary-hit lobe sample; later bounces use plain PCG.
                    float2 u = (cur.depth == 0) ? frac(R2(sampleIndex) + rot) : Random2(seed);

                    float3 Hl = SampleGGXVNDF(Ve, alpha, u);
                    float3 H = t * Hl.x + b * Hl.y + N * Hl.z;

                    float VoH = saturate(dot(V, H));
                    float G1 = SmithG1GGX(max(NoV, 1e-3), alpha);

                    float F = FresnelDielectric(VoH, etaI, etaT); // 1.0 on TIR
                    float3 Fo = FresnelSchlick(F0, VoH);

                    // Reflection (opaque specular lobe + dielectric reflection of the glass part)
                    reflDir = reflect(cur.direction, H);
                    float NoL = dot(reflDir, N);
                    float g2r = NoL > 0.0 ? SmithG2GGX(NoV, NoL, alpha) / G1 : 0.0;
                    reflectWeight = ((1.0 - T) * Fo + T * F) * g2r;

                    // Refraction through the SAME microfacet
                    if (T > 0.0 && F < 1.0) {
                        float3 r = refract(cur.direction, H, etaI / etaT);
                        if (dot(r, r) > 0.0) {
                            refrDir = normalize(r);
                            float NoT = dot(refrDir, -N);
                            float g2t = NoT > 0.0 ? SmithG2GGX(NoV, NoT, alpha) / G1 : 0.0;
                            refractWeight = T * (1.0 - F) * g2t;
                        }
                    }
                } else {
                    // Smooth surface: perfect mirror / perfect glass.
                    reflDir = reflect(cur.direction, N);
                    reflectWeight = (1.0 - T) * F_env;

                    if (T > 0.0) {
                        float F = FresnelDielectric(NoV, etaI, etaT);

                        reflectWeight += T * F;

                        if (F < 1.0) { // F == 1 means total internal reflection
                            refrDir = normalize(refract(cur.direction, N, etaI / etaT));
                            refractWeight = T * (1.0 - F);
                        }
                    }
                }

                PathRay reflected;
                reflected.origin = payload.position + N * 0.001;
                reflected.direction = reflDir;
                reflected.throughput = cur.throughput * reflectWeight;
                reflected.depth = cur.depth + 1;

                PathRay refracted;
                refracted.origin = payload.position - N * 0.001;
                refracted.direction = refrDir;
                refracted.throughput = cur.throughput * refractWeight;
                refracted.depth = cur.depth + 1;

                bool hasReflect = MaxComponent(reflected.throughput) >= MIN_THROUGHPUT;
                bool hasRefract = MaxComponent(refracted.throughput) >= MIN_THROUGHPUT;

                if (hasRefract && hasReflect) {
                    if (roughPath && cur.depth >= SPLIT_MAX_DEPTH) {
                        // Collapse: pick one child stochastically (Russian-roulette style,
                        // unbiased) so rough chains cost a fixed number of rays.
                        float wr = MaxComponent(reflected.throughput);
                        float wt = MaxComponent(refracted.throughput);
                        float pR = wr / (wr + wt);

                        if (Random(seed) < pR) {
                            reflected.throughput /= pR;
                            cur = reflected;
                        } else {
                            refracted.throughput /= (1.0 - pR);
                            cur = refracted;
                        }
                    } else {
                        // Split: follow refraction, stack reflection.
                        if (sp < RAY_STACK_SIZE) {
                            stack[sp] = reflected;
                            sp++;
                        }
                        cur = refracted;
                    }
                } else if (hasRefract) {
                    cur = refracted;
                } else if (hasReflect) {
                    cur = reflected;
                } else {
                    alive = false;
                }
            }
        }

        if (!alive) {
            if (sp == 0) {
                break;
            }
            sp--;
            cur = stack[sp];
        }
    }

    return radiance;
}

// Firefly clamp: rescale so the brightest channel never exceeds the limit (keeps hue).
float3 ClampRadiance(float3 c) {
    float m = MaxComponent(c);
    return m > MAX_SAMPLE_RADIANCE ? c * (MAX_SAMPLE_RADIANCE / m) : c;
}

[shader("raygeneration")] void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    float3 camPos = CameraData.position;
    float3 camForward = normalize(CameraData.forward);

    float aspectRatio = float(resolution.x) / float(resolution.y);

    float horizontalFov = 2.0 * atan(CameraData.sensorWidth / (2.0 * CameraData.focalLength));
    float verticalFov = 2.0 * atan(tan(horizontalFov * 0.5) / aspectRatio);

    float halfWidth = tan(horizontalFov * 0.5);
    float halfHeight = tan(verticalFov * 0.5);

    float3 worldUp = float3(0.0, 1.0, 0.0);
    float3 camRight = normalize(cross(camForward, worldUp));
    float3 camUp = cross(camRight, camForward);

    // Per-pixel seed, always initialised. (Noise is frozen per pixel without accumulation;
    // the per-pixel rotation below keeps it decorrelated between neighbours.)
    uint seed = pixel.x + pixel.y * resolution.x;
    seed ^= 0x9E3779B9u;
    seed = PCGHash(seed);

    float2 rot = Random2(seed);

    float lobe = 0.0;

    // Sample 0: pixel centre, also measures how rough this pixel's lobes are.
    float3 sum;
    {
        float2 uv = (float2(pixel) + 0.5) / float2(resolution);
        float2 ndc = uv * 2.0 - 1.0;
        float3 dir = normalize(camForward + camRight * (ndc.x * halfWidth) + camUp * (ndc.y * halfHeight));

        sum = ClampRadiance(TracePath(camPos, dir, seed, 0, rot, lobe));
    }

    // Adaptive sample count: smooth pixels stay at 1 spp, rough ones get more.
    uint spp = SPP_SMOOTH;
    if (lobe >= LOBE_ROUGH_MIN) {
        spp = SPP_ROUGH;
    } else if (lobe > LOBE_MEDIUM_MIN) {
        spp = SPP_MEDIUM;
    }

    [loop] for (uint s = 1; s < spp; ++s) {
        // Stratified sub-pixel jitter for AA, independent of the lobe sequence.
        float2 jitter = frac(R2(s + 17u) + Random2(seed) * 0.0 + rot.yx);
        float2 uv = (float2(pixel) + jitter) / float2(resolution);
        float2 ndc = uv * 2.0 - 1.0;
        float3 dir = normalize(camForward + camRight * (ndc.x * halfWidth) + camUp * (ndc.y * halfHeight));

        float unusedLobe = 0.0;
        sum += ClampRadiance(TracePath(camPos, dir, seed, s, rot, unusedLobe));
    }

    Output[pixel] = float4(sum / float(spp), 1.0);
}
