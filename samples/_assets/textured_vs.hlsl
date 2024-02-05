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

struct Constants {
    float4x4 matW;
    float4x4 matVP;
    float3 eyePos;
    float3 light0Pos;
};

struct a2v {
    float3 position;
    float3 normal;
    float2 uv0;
};

struct v2f {
    float4 position     : SV_POSITION;
    float3 normalW      : TEXCOORD0;
    float3 eyeVecW      : TEXCOORD1;
    float3 light0VecW   : TEXCOORD2;
    float2 uv0          : TEXCOORD3;
};

ConstantBuffer<Constants> Globals : register(b0);
StructuredBuffer<a2v> vertexBuffer : register(t0);

[RootSignature(ROOT_SIG)]
v2f main(uint vid : SV_VertexID) {
    a2v IN = vertexBuffer[vid];
    v2f OUT;

    float4 positionW = mul(float4(IN.position, 1.0f), Globals.matW);
    float3 normalW = mul(IN.normal, float3x3(Globals.matW[0].xyz, Globals.matW[1].xyz, Globals.matW[2].xyz));
    float3 eyeVec = normalize(Globals.eyePos - positionW.xyz);
    float3 light0Vec = normalize(Globals.light0Pos - positionW.xyz);

    OUT.position = mul(positionW, Globals.matVP);
    OUT.normalW = normalW;
    OUT.eyeVecW = eyeVec;
    OUT.light0VecW = light0Vec;
    OUT.uv0 = IN.uv0;

    return OUT;
}


