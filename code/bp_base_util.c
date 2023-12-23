global u32 g_random_u32_state = 17624813;
global u64 g_random_u64_state = 17624813;

function void
SeedRandom_U32(u32 seed)
{
    g_random_u32_state = seed;
}

function void
SeedRandom_U64(u64 seed)
{
    g_random_u64_state = seed;
}

function u32
Random_U32(void)
{
    u32 result = g_random_u32_state;
    result ^= result << 13u;
    result ^= result << 17u;
    result ^= result << 5u;
    return g_random_u32_state = result;
}

function u64
Random_U64(void)
{
    u64 result = g_random_u64_state;
    result ^= result << 13u;
    result ^= result << 17u;
    result ^= result << 5u;
    return g_random_u64_state = result;
}
