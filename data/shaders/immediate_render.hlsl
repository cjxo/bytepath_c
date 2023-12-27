cbuffer Constants : register(b0)
{
	// why does this fail when I change to column_major?
	row_major float4x4 orthographic;
}

struct VS_In
{
	float2 vertex : Vertex;
	float4 colour : Colour;
};

struct VS_Out
{
	float4 position : SV_Position;
	float4 colour : Colour;
};

VS_Out
VSMain(VS_In vs_in)
{
	VS_Out output = {
		mul(orthographic, float4(vs_in.vertex, 0.0f, 1.0f)),
		vs_in.colour
	};

	return(output);
}

float4 PSMain(VS_Out input) : SV_Target
{
	return pow(input.colour, 2.2f);
}