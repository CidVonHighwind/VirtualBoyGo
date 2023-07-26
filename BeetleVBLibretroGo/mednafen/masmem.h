#ifndef __MDFN_PSX_MASMEM_H
#define __MDFN_PSX_MASMEM_H

// TODO, WIP (big-endian stores and loads not fully supported yet)

static INLINE uint16 LoadU16_RBO(const uint16 *a)
{
#ifdef ARCH_POWERPC
   uint16 tmp;

   __asm__ ("lhbrx %0, %y1" : "=r"(tmp) : "Z"(*a));

   return(tmp);

#else
   uint16 tmp = *a;
   return((tmp << 8) | (tmp >> 8));
#endif
}

static INLINE uint32 LoadU32_RBO(const uint32 *a)
{
#ifdef ARCH_POWERPC
   uint32 tmp;

   __asm__ ("lwbrx %0, %y1" : "=r"(tmp) : "Z"(*a));

   return(tmp);
#else
   uint32 tmp = *a;
   return((tmp << 24) | ((tmp & 0xFF00) << 8) | ((tmp >> 8) & 0xFF00) | (tmp >> 24));
#endif
}

static INLINE void StoreU16_RBO(uint16 *a, const uint16 v)
{
#ifdef ARCH_POWERPC
   __asm__ ("sthbrx %0, %y1" : : "r"(v), "Z"(*a));
#else
   uint16 tmp = (v << 8) | (v >> 8);
   *a = tmp;
#endif
}

static INLINE void StoreU32_RBO(uint32 *a, const uint32 v)
{
#ifdef ARCH_POWERPC
   __asm__ ("stwbrx %0, %y1" : : "r"(v), "Z"(*a));
#else
   uint32 tmp = (v << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24);
   *a = tmp;
#endif
}

static INLINE uint16 LoadU16_LE(const uint16 *a)
{
#ifdef MSB_FIRST
   return LoadU16_RBO(a);
#else
   return *a;
#endif
}

static INLINE uint32 LoadU32_LE(const uint32 *a)
{
#ifdef MSB_FIRST
   return LoadU32_RBO(a);
#else
   return *a;
#endif
}

static INLINE void StoreU16_LE(uint16 *a, const uint16 v)
{
#ifdef MSB_FIRST
   StoreU16_RBO(a, v);
#else
   *a = v;
#endif
}

static INLINE void StoreU32_LE(uint32 *a, const uint32 v)
{
#ifdef MSB_FIRST
   StoreU32_RBO(a, v);
#else
   *a = v;
#endif
}

#endif
