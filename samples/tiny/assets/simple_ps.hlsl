
struct v2f {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

float4 main(v2f IN) : SV_TARGET0 {
    return IN.color;
}
