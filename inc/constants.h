#ifndef HYSW_RAYTRACER_IMPL_CONSTANTS_H
#define HYSW_RAYTRACER_IMPL_CONSTANTS_H

#define E_EFFECT_NONE 0
#define E_EFFECT_ENABLE 1

#define E_SHADOW_NONE 0
#define E_SHADOW_HARD 1
#define E_SHADOW_CRAP 2
#define E_SHADOW_SOFT 3

#define ENABLE_SHADOW    E_EFFECT_NONE
#define ENABLE_REFLECT   E_EFFECT_ENABLE
#define ENABLE_REFRACT   E_EFFECT_ENABLE
#define ENABLE_DIFFUSE   E_EFFECT_ENABLE
#define ENABLE_AMBIENT   E_EFFECT_NONE
#define ENABLE_SPECULAR  E_EFFECT_NONE
#define ENABLE_SIGNITURE E_EFFECT_NONE
#define ENABLE_PHOTONMAP E_EFFECT_ENABLE

// ==< limits and params >=================================================
#define RECURSIVE_TRACE_LIMIT 16

// the directional sample
// acutal number of samples = (2n+1)^2
#define SOFTSHADOW_DIRECTIONAL_SAMPLES 5

// ==< technical parameters >==============================================
#define RAYINT_DISTANCE_MIN 0.000001

// ==< photon mapping information >========================================

#define NWORKERS  65536
#define RAYLIMIT (1048*1024*1024L/32)
// 1MiB = 64*64*(8 rays per pixel)

// the time will be REVPMAP_Q^2*REVPMAP_A^2
#define REVPMAP_Q 3
#define REVPMAP_A 1
// ND kernel range
#define FWIDTH (256)
#define FHEIGHT (256)
// light source split
#define REVPMAP_KERNEL_W (4*REVPMAP_Q)
#define REVPMAP_KERNEL_H (4*REVPMAP_Q)
// area light source split
#define REVPMAP_KERNEL_AW (REVPMAP_A)
#define REVPMAP_KERNEL_AH (REVPMAP_A)
// repeats
#define REVPMAP_ITERS (1) // iteration inside the kernel
#define REVPMAP_REPEATS (8) // repeat the mapping
#define REVPMAP_RECLIMIT (32)
// hashmap size
#define SPACEHASH_SIZE (122949829) // around 400MiB
// photon gathering radius
#define SPACEPART 400

#define TRACERECDEPTH 4
// discarding
#define PHOTONDENSITY (FWIDTH*FHEIGHT/10)
#define PHOTONRADIUS 0.03
#define PHOTONMEM 8 // need to modify first
#define LOWESTFLUX 0.1
#define LOWESTFLUX_DIFFUSE 0.01
#define LOWESTFLUX_REFLECT 0.1
#define LOWESTFLUX_REFRACT 0.1

#endif