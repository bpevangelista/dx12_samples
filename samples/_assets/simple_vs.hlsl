
struct a2v {
    float4 position : POSITION;
    float4 color : COLOR;
};

struct v2f {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

StructuredBuffer<a2v> vertexBuffer : register(t0);

[RootSignature("SRV(t0)")]
v2f main(uint vid : SV_VertexID) {
    a2v IN = vertexBuffer[vid];
    
    v2f OUT;
    OUT.position = IN.position;
    OUT.color = IN.color;
    return OUT;
}


