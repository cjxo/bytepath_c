/* date = December 24th 2023 11:23 am */

#ifndef BP_BASE_MATH_H
#define BP_BASE_MATH_H

typedef union v2f
{
    struct
    {
        f32 x, y;
    };
    f32 v[2];
} v2f;

typedef union v4f
{
    struct
    {
        f32 x, y, z, w;
    };
    struct
    {
        f32 r, g, b, a;
    };
    struct
    {
        v2f xy;
        v2f zw;
    };
    f32 v[4];
} v4f;

typedef union m44
{
    struct
    {
        v4f r0, r1, r2, r3;
    };
    f32 m[4][4];
} m44;

typedef struct Quad
{
    v2f origin;
    v2f x_axis;
    v2f y_axis;
    v4f colours[4];
    
    f32 side_roundness;
    f32 side_thickness;
} Quad;

#define two_pi_F32 6.28318531f

#define V2F(x,y) (v2f){x,y}
#define V4F(x,y,z,w) (v4f){x,y,z,w}
#define RGBA(r,g,b,a) V4F(r,g,b,a)

//~ NOTE(christian): nice tools
inline f32 EaseOutQuart(f32 t);
inline f32 EaseInQuart(f32 t);
inline f32 AbsoluteValueF32(f32 x);
inline f32 ToRadians(f32 x);

//~ NOTE(christian): V2F
inline v2f V2F_Scale(v2f a, f32 scale);
inline v2f V2F_Add(v2f a, v2f b);
inline v2f V2F_Subtract(v2f a, v2f b);

//~ NOTE(christian): matrices
inline m44 Matrix4x4_Orthographic_LH_RM_Z01(f32 left, f32 right, f32 top, f32 bottom, f32 near, f32 far);
inline m44 Matrix4x4_Orthographic_LH_CM_Z01(f32 left, f32 right, f32 top, f32 bottom, f32 near, f32 far);
#endif //BP_BASE_MATH_H
