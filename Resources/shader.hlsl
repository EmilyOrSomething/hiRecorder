Texture2D<float4> InputRGB : register(t0);
RWTexture2D<unorm float4> OutputAYUV : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    OutputAYUV.GetDimensions(width, height);

    if (id.x >= width || id.y >= height)
        return;

    const float3 matY = float3(0.182586f, 0.614231f, 0.062007f);
    const float3 matU = float3(-0.100644f, -0.338572f, 0.439216f);
    const float3 matV = float3(0.439216f, -0.398942f, -0.040274f);
    const float ofsY = 16.0f / 255.0f;
    const float ofsUV = 0.5f;

    float3 rgb = InputRGB[id.xy].rgb;

    float y = dot(rgb, matY) + ofsY;
    float u = dot(rgb, matU) + ofsUV;
    float v = dot(rgb, matV) + ofsUV;

    OutputAYUV[id.xy] = float4(v, u, y, 1.0f);
}