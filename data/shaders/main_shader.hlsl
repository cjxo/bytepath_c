cbuffer Constants : register(b0)
{
	// why does this fail when I change to column_major?
	row_major float4x4 orthographic;
}

struct Quad
{
	float2 origin 	: Origin;
	float2 x_axis 	: XAxis;
	float2 y_axis 	: YAxis;
	float4 colours[4] : Colours;

	float roundness : Roundness;
	float thickness : Thickness;
};

struct VS_Out
{
	float4 position : SV_Position;
	float4 colour : Colour;
	float2 origin : Origin;
	float2 x_axis : XAxis;
	float2 y_axis : YAxis;
	float roundness : Roundness;
	float thickness : Thickness;
};

StructuredBuffer<Quad> quad_sb : register(t0);

static float2 axis_combination[] =
{
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 1.0f)
};

float
RoundedBoxSDF(float2 sample, float2 center, float2 half_dim, float roundness)
{
	float2 rel_p = center - sample;
	float2 to_rel_p = abs(rel_p) - half_dim + float2(roundness, roundness);
	to_rel_p = max(to_rel_p, 0);

	float result = length(to_rel_p) - roundness;

	return(result);
}

VS_Out VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	Quad quad = quad_sb[instance_id];

	VS_Out output = {
		float4(0.0f, 0.0f, 0.0f, 1.0f),
		quad.colours[vertex_id],
		quad.origin,
		quad.x_axis,
		quad.y_axis,
		quad.roundness,
		quad.thickness
	};
	
	column_major float2x2 coord = {
		quad.x_axis.x, quad.y_axis.x,
		quad.x_axis.y, quad.y_axis.y,
	};

	float2 combination = axis_combination[vertex_id];
	float4 screen_p = float4(quad.origin + mul(coord, combination), 0.0f, 1.0f);
	output.position = mul(orthographic, screen_p);
	return(output);
}

float linear_from_srgb_single(float x)
{
	if (x <= 0.04045f)
	{
		x /= 12.92f;
	}
	else
	{
		x = pow(((x + 0.055f)/1.055f), 2.4f);
	}

	return(x);
}

float4 linear_from_srgb(float4 x)
{
	float4 result = {
		 linear_from_srgb_single(x.r),
		 linear_from_srgb_single(x.g),
		 linear_from_srgb_single(x.b),
		 linear_from_srgb_single(x.a),
	};

	return (result);
}

float4 PSMain(VS_Out input) : SV_Target
{
	float softness = 0.8f;
	float2 softness_padding = float2(softness * 2.0f - 1, softness * 2.0f - 1);
	float2 half_dim = float2(input.x_axis.x * 0.5f, input.y_axis.y * 0.5f);

	if (input.roundness > 0.0f)
	{
		float signed_dist = RoundedBoxSDF(input.position.xy,
										  input.origin + half_dim,
										  half_dim - softness_padding,
										  input.roundness) - 0.25;

		float x = smoothstep(0.0f, softness * 2 - 1.25f, signed_dist);
		input.colour *= 1.0f - x;
	}

	if (input.thickness > 0.0f)
	{
		float2 thicknessv2 = float2(input.thickness, input.thickness);
		float2 reduced_half_dims = half_dim - thicknessv2;
		
		float reduce_percent_sides = min((reduced_half_dims.x / half_dim.x),
										 (reduced_half_dims.y / half_dim.y));
		float signed_dist = RoundedBoxSDF(input.position.xy,
						 			    input.origin + half_dim,
						 				reduced_half_dims - softness_padding,
						 				input.roundness * reduce_percent_sides * reduce_percent_sides) + 0.30f;

		input.colour *= smoothstep(0.0f, softness * 0.05f, signed_dist);
	}

	return pow(input.colour, 2.2f);
}