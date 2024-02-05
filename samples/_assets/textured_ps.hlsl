// https://learn.microsoft.com/en-us/windows/win32/direct3d12/specifying-root-signatures-in-hlsl
#define ROOT_SIG                                                                \
    "RootFlags(0)"                                                              \
    ", CBV(b0, visibility=SHADER_VISIBILITY_VERTEX, flags=DATA_STATIC)"         \
    ", SRV(t0, visibility=SHADER_VISIBILITY_VERTEX)"                            \
    ", DescriptorTable("                                                        \
    "    SRV(t1)"                                                               \
    "    , visibility=SHADER_VISIBILITY_PIXEL"                                  \
    "  )"                                                                       \
    ", StaticSampler(s0"                                                        \
    "    , filter=FILTER_MIN_MAG_MIP_LINEAR"                                    \
    "    , visibility=SHADER_VISIBILITY_PIXEL"                                  \
    "  )"

Texture2D<float4> albedoTex : register(t1);
SamplerState linearSampler : register(s0);

struct v2f {
    float4 position     : SV_POSITION;
    float2 uv0          : TEXCOORD0;
};

[RootSignature(ROOT_SIG)]
float4 main(v2f IN) : SV_TARGET0 {
    return albedoTex.Sample(linearSampler, IN.uv0);
}

