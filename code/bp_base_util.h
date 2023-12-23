/* date = December 23rd 2023 5:33 pm */

#ifndef BP_BASE_UTIL_H
#define BP_BASE_UTIL_H

//~ NOTE(christian): prngs
#define seed_random_u32_max 0xFFFFFFFF
#define seed_random_u64_max 0xFFFFFFFFFFFFFFFF

function void SeedRandom_U32(u32 seed);
function void SeedRandom_U64(u64 seed);
function u32 Random_U32(void);
function u64 Random_U64(void);

#endif //BP_BASE_UTIL_H
