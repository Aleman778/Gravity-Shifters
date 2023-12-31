#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F32 3.1415926535897932385f

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define sign(value) ((value) < 0 ? -1 : ((value) > 0 ? 1 : 0 ))
#define clamp(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

#if BUILD_DEBUG
#define pln(format, ...) printf(format##"\n", __VA_ARGS__)
#else
#define pln(format, ...)
#endif

// NOTE(Alexander): bit stuff
#define bit(x) (1 << (x))
#define is_bit_set(var, x) ((var) & (1 << (x)))
#define is_bitflag_set(var, flag) ((var) & (flag))


// NOTE(Alexander): define more convinient types
#define fixed_array_count(array) (sizeof(array) / sizeof((array)[0]))

typedef int8_t    s8;
typedef uint8_t   u8;
typedef int16_t   s16;
typedef uint16_t  u16;
typedef int32_t   s32;
typedef uint32_t  u32;
typedef int64_t   s64;
typedef uint64_t  u64;
typedef uintptr_t umm;
typedef intptr_t  smm;
typedef float     f32;
typedef double    f64;
typedef int32_t   b32;
typedef const char* cstring;

struct string {
    u8* data;
    smm count;
};


inline umm
cstring_count(cstring str) {
    if (!str) return 0;
    
    umm count = 0;
    u8* curr = (u8*) str;
    while (*curr) {
        count++;
        curr++;
    }
    return count;
}

inline string
string_lit(cstring str) {
    string result;
    result.data = (u8*) str;
    result.count = cstring_count(str);
    return result;
}

inline cstring
string_to_cstring(string str, u8* dest=0) {
    u8* result = dest ? dest : (u8*) malloc(str.count + 1);
    memcpy(result, str.data, str.count);
    result[str.count] = 0;
    return (cstring) result;
}
inline string
cstring_to_string(cstring str) {
    string result;
    result.data = (u8*) str;
    result.count = cstring_count(str);
    return result;
}

inline int
string_compare(string a, string b) {
    smm count = min(a.count, b.count);
    for (smm i = 0; i < count; i++) {
        if (a.data[i] != b.data[i]) {
            return b.data[i] - a.data[i];
        }
    }
    
    return (int) (b.count - a.count);
}
#define string_equals(a, b) (string_compare(a, b) == 0)

inline void
cstring_free(cstring str) {
    free((void*) str);
}

#define array_count(array) (sizeof(array) / sizeof((array)[0]))

#ifdef assert
#undef assert
#endif

#if BUILD_DEBUG
void
__assert(const char* expression, const char* file, int line) {
    // TODO(alexander): improve assertion printing,
    // maybe let platform layer decide how to present this?
    fprintf(stderr, "%s:%d: Assertion failed: %s\n", file, line, expression);
    *(int *)0 = 0; // NOTE(alexander): purposfully trap the program
}

#define assert(expression) (void)((expression) || (__assert(#expression, __FILE__, __LINE__), 0))
#else
#define assert(expression)
#endif
