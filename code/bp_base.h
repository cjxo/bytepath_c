/* date = December 21st 2023 4:13 pm */

#ifndef BP_BASE_H
#define BP_BASE_H
#include <string.h>
#include <stdint.h>

typedef    int8_t s8;
typedef   uint8_t u8;
typedef  int16_t s16;
typedef uint16_t u16;
typedef  int32_t s32;
typedef uint32_t u32;
typedef  int64_t s64;
typedef uint64_t u64;
typedef    float f32;
typedef   double f64;
typedef      s32 b32;

typedef enum String_Kind
{
    String_U8
} String_Kind;

typedef struct String_Decode
{
    u32 codepoint;
    u8 byte_count;
    u8 *next_in_str;
} String_Decode;

typedef struct String_Const_U8
{
    u8 *str;
    u32 count;
} String_Const_U8;

typedef struct String_Const_U16
{
    u16 *str;
    u32 count;
} String_Const_U16;

typedef struct String_Const_U32
{
    u32 *str;
    u32 count;
} String_Const_U32;

#if !defined(AssertBreak)
# define AssertBreak() (*(volatile int *)0 = 0)
#endif

#define Stmnt(s) do{s}while(0)

#if BP_DEBUG
# define Assert(c) Stmnt( if(!(c)){ AssertBreak(); } )
#endif

#define AssertTrue(c) Assert((c)==True)
#define AssertFalse(c) Assert((c)==False)

#define InvalidCodePath() AssertBreak()

#define MemoryCopy(dest,source,size) memcpy(dest,source,size)
#define Min(a,b) ((a)<(b)?(a):(b))
#define Max(a,b) ((a)>(b)?(a):(b))
#define ArrayCount(a) (sizeof(a)/sizeof(*(a)))

#define KB(v) (1024llu*(u64)(v))
#define MB(v) (1024llu*KB(v))
#define GB(v) (1024llu*MB(v))

#define global static
#define local static
#define function static
#define null 0

#define Str8Lit(s) (String_Const_U8){ (u8 *)s, sizeof(s) - 1 } 

#define Unused(v) (void)v

#define True 1
#define False 0

#define bad_index_u32 0xFFFFFFFF

#include "bp_base_util.h"
#include "bp_base_math.h"

#endif //BP_BASE_H
