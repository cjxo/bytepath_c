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

// TODO(christian): UTF-16 variant
function String_Decode
StringDecode_UTF8(u8 *str, u32 capacity)
{
    String_Decode result = { 0, 0 };
    
    local const u8 lengths_table[] = 
    {
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0,
        2, 2, 2, 2, 3, 3, 4, 0
    };
    
    local const u8 masks_table_based_on_length[] =
    {
        0x00, 0x7F, 0x1F, 0x0F, 0x07,
    };
    
    local const u8 shifts_back_table_based_on_length[] =
    {
        0, 18, 12, 6, 0
    };
    
    result.byte_count = lengths_table[str[0] >> 3];
    result.next_in_str = str + result.byte_count + !result.byte_count;
    
    result.codepoint = (u32)(str[0] & masks_table_based_on_length[result.byte_count]) << 18u;
    result.codepoint |= (u32)(str[1] & 0x3F) << 12u;
    result.codepoint |= (u32)(str[2] & 0x3F) << 6u;
    result.codepoint |= (u32)(str[3] & 0x3F) << 0u;
    result.codepoint >>= shifts_back_table_based_on_length[result.byte_count];
    
    return(result);
}

// TODO(christian): UTF-16 variant
function u32
StringEncode_UTF8(u8 *dest, u32 codepoint)
{
    u32 byte_count_result = 0;
    
    if (codepoint < 0xFFu)
    {
        dest[0] = (u8)codepoint;
        byte_count_result = 1;
    }
    else if (codepoint < (0xFFFu))
    {
        dest[0] = (u8)((codepoint >> 6) | 0xC0);
        dest[1] = (u8)((codepoint & 0x3F) | 0x80);
        byte_count_result = 2;
    }
    else if (codepoint < 0xFFFFu)
    {
        dest[0] = (u8)((codepoint >> 12) | 0xE0);
        dest[1] = (u8)(((codepoint >> 6) & 0x3F) | 0x80);
        dest[2] = (u8)(((codepoint >> 0) & 0x3F) | 0x80);
        byte_count_result = 3;
    } else if (codepoint < 0x200000u)
    {
        dest[0] = (u8)((codepoint >> 18) | 0xF0);
        dest[1] = (u8)(((codepoint >> 12) & 0x3F) | 0x80);
        dest[2] = (u8)(((codepoint >> 6) & 0x3F) | 0x80);
        dest[3] = (u8)(((codepoint >> 0) & 0x3F) | 0x80);
        byte_count_result = 4;
    }
    
    return(byte_count_result);
}