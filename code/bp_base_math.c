
//~ NOTE(christian): nice tools
inline f32
EaseOutQuart(f32 t)
{
    f32 result = 1.0f - powf(1.0f - t, 4.0f);
    return(result);
}

inline f32
EaseInQuart(f32 t)
{
    f32 result = t * t * t * t;
    return(result);
}

inline f32
AbsoluteValueF32(f32 x)
{
    union { f32 f; u32 n; } a;
    a.f = x;
    a.n &= ~0x80000000;
    return(a.f);
}

inline f32
ToRadiansF32(f32 x)
{
    f32 result;
    result = 0.01745329251f * x;
    return(result);
}

//~ NOTE(christian): V2F
inline v2f
V2F_Scale(v2f a, f32 scale)
{
    v2f result = V2F(a.x * scale, a.y * scale);
    return(result);
}

inline v2f
V2F_Add(v2f a, v2f b)
{
    v2f result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return(result);
}

inline v2f
V2F_Subtract(v2f a, v2f b)
{
    v2f result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return(result);
}

//~ NOTE(christian): matrices
inline m44
Matrix4x4_Orthographic_LH_RM_Z01(f32 left, f32 right, f32 top,
                                 f32 bottom, f32 near, f32 far)
{
    m44 result;
    result.r0 = V4F(2.0f / (right - left), 0.0f, 0.0f, 0.0f);
    result.r1 = V4F(0.0f, 2.0f / (top - bottom), 0.0f, 0.0f);
    result.r2 = V4F(0.0f, 0.0f, 1.0f / (far - near), 0.0f);
    result.r3 = V4F(-(right + left) / (right - left), -(top + bottom) / (top - bottom) , -near / (far - near), 1.0f);
    return(result);
}

inline m44
Matrix4x4_Orthographic_LH_CM_Z01(f32 left, f32 right, f32 top,
                                 f32 bottom, f32 near, f32 far)
{
    m44 result;
    result.r0 = V4F(2.0f / (right - left), 0.0f, 0.0f, -(right + left) / (right - left));
    result.r1 = V4F(0.0f, 2.0f / (top - bottom), 0.0f, -(top + bottom) / (top - bottom));
    result.r2 = V4F(0.0f, 0.0f, 1.0f / (far - near), -near / (far - near));
    result.r3 = V4F(0.0f, 0.0f , 0.0f, 1.0f);
    return(result);
}