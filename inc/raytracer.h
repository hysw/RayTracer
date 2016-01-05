// == < Program > =========================================================
// Raytracer for CSC418 by hysw / Peter Chen
// Version: Pre-Alpha at March 29, 2015

// == < Copyright > =======================================================
// Copyright information for the source: WTFPL 2015 by hysw / Peter Chen
//   Just note that there are cited material which might be GPL
//   And the models are found over the internet

// == < Features > ========================================================
// WORKING? Refraction
// WORKING? GPU accelerated ray tracing & photon mapping
// TODO FIXME WARNING BRDF
// FIXME Triangle mesh numerical instability
// FIXME progressive (reverse) photon mapping
//   WORKING? softshadow
//   FIXME [PARITAL WORKING] Caustic
//   FIXME [PARITAL WORKING] space hashing
//   FIXME output hit-map and accumulator map
//   FIXME multiple extended light source
//   TODO FIXME kd-tree implementation
// TODO FIXME Texture mapping
// TODO FIXME Compound object
// TODO FIXME Environment mapping
// TODO FIXME Glossy reflection (partial distribution ray tracing)
// TODO FIXME Phong shading (not aviable as current version)
// TODO FIXME Direct photon mapping mode (with distribution ray tracing)
// TODO [WONTFIX for A3] Distribution Ray Tracing
// TODO [WONTFIX for A3] Depth of field
// TODO [WONTFIX for A3] Motion blur
// TODO [WONTFIX for A3] Motion blur
// TODO [WONTFIX for A3] Dispersion (caustic)
// TODO [WONTFIX for A3] Lens flare
// TODO [WONTFIX for A3] Atmospheric optics
// TODO [WONTFIX for A3] Volumetric lighting
// TODO [WONTFIX for A3] Halo

// == < Research > ========================================================
// Metropolis light transport
//   primary sample space?
// Path tracing
// Bidirectional path tracing
// Importance sampling?

// == < Reference > =======================================================
// Progressive photon mapping
//   http://graphics.ucsd.edu/~henrik/papers/progressive_photon_mapping/progressive_photon_mapping.pdf

// == < Extra links > =====================================================
// Fluid simulation
//   http://matthias-mueller-fischer.ch/talks/GDC2008.pdf
//   https://www.cs.ubc.ca/~rbridson/fluidsimulation/GameFluids2007.pdf
// Height field
//   http://dl.acm.org/citation.cfm?id=988878
// Progressive photon mapping
//   http://graphics.ucsd.edu/~henrik/papers/progressive_photon_mapping/progressive_photon_mapping.pdf
//   http://www.cs.jhu.edu/~misha/ReadingSeminar/Papers/Knaus11.pdf
//   http://cg.ivd.kit.edu/publications/p2012/APPM_Kaplanyan_2012/APPM_Kaplanyan_2012.pdf#page=1
//     http://cg.ivd.kit.edu/english/APPM.php
// Ray Tracing Point Sampled Geometry
//   http://www.graphics.stanford.edu/~henrik/papers/psp/psp.pdf
// Lens flare
//   http://resources.mpi-inf.mpg.de/lensflareRendering/pdf/flare.pdf
// Atmospheric optics
//   https://en.wikipedia.org/wiki/Atmospheric_optics_ray-tracing_codes
// Volumetric lighting
//   http://www.cse.chalmers.se/~uffe/volumetricshadows.pdf
// Dispersion
//   http://people.mpi-inf.mpg.de/~oelek/Papers/SpectralDifferentials/Elek2014b.pdf
//   http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.61.6523&rep=rep1&type=pdf
// Shadow Volume
//   https://mediatech.aalto.fi/~jaakko/publications/laine2005siggraph_paper.pdf

#ifdef __cplusplus
#define HYSW_RAYTRACER_NAMESPACE_START namespace hysw{ namespace raytracer {
#define HYSW_RAYTRACER_NAMESPACE_END }}
#else
#define HYSW_RAYTRACER_NAMESPACE_START
#define HYSW_RAYTRACER_NAMESPACE_END
#endif


