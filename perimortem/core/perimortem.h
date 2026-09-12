// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef PERIMORTEM_CORE_PERIMORTEM_H
#define PERIMORTEM_CORE_PERIMORTEM_H

// C and C++ share these scalar definitions so an interface uses the same
// types on both sides. Class behavior and construction support belong to the
// C++ header, while this vocabulary also works for independently built C code.
typedef unsigned char U8;
typedef unsigned short int U16;
#ifdef __LP32__
typedef unsigned long U32;
#else
typedef unsigned int U32;
#endif
typedef unsigned long long U64;

typedef signed char S8;
typedef signed short int S16;
#ifdef __LP32__
typedef signed long S32;
#else
typedef signed int S32;
#endif
typedef signed long long S64;

typedef float R32;
typedef double R64;
typedef U64 Count;

#ifdef __cplusplus
#define PERIMORTEM_C extern "C"
#define PERIMORTEM_SCALAR_ASSERT static_assert
#else
#define PERIMORTEM_C
#define PERIMORTEM_SCALAR_ASSERT _Static_assert
#endif

PERIMORTEM_SCALAR_ASSERT(sizeof(U8) == 1, "U8 requires one byte");
PERIMORTEM_SCALAR_ASSERT(sizeof(U16) == 2, "U16 requires two bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(U32) == 4, "U32 requires four bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(U64) == 8, "U64 requires eight bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(S8) == 1, "S8 requires one byte");
PERIMORTEM_SCALAR_ASSERT(sizeof(S16) == 2, "S16 requires two bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(S32) == 4, "S32 requires four bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(S64) == 8, "S64 requires eight bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(R32) == 4, "R32 requires four bytes");
PERIMORTEM_SCALAR_ASSERT(sizeof(R64) == 8, "R64 requires eight bytes");

#undef PERIMORTEM_SCALAR_ASSERT

#endif
