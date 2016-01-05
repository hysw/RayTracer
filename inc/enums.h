#ifndef HYSW_RAYTRACER_IMPL_ENUMS_H
#define HYSW_RAYTRACER_IMPL_ENUMS_H
#include "raytracer.h"
HYSW_RAYTRACER_NAMESPACE_START

enum {
	MATERIAL_REFLECT,   // the reflection we will have, w indicate glossy
	MATERIAL_REFRACT,   // the refraction we will have, w indicate refraction index
	MATERIAL_DIFFUSE,   // diffusion light, w=1 indicate it is a light source
	MATERIAL_SPECULAR,  // specular highlight
	MATERIAL_AMBIENT,   // ambient light(with ambient osclation)
	MATERIAL_SIGNITURE, // constant light, used for scene signiture
	MATERIAL_END
};

enum {
	TEXTURE_UCHAR_RGB = 0x01,
	TEXTURE_PPMP6_RGB = 0x02, // note this have inversed y value
	TEXTURE_END
};

enum {
	COMPOUNDED_UNION,
	COMPOUNDED_MINUS,
	COMPOUNDED_INTERSECT,
	COMPOUNDED_COMPLEMENT,
	COMPOUNDED_END
};

enum {
	ENTITY_NULL,
	ENTITY_Global,
	ENTITY_Sphere,
	ENTITY_Cube,
	ENTITY_HalfPlane,
	ENTITY_Triangle,
	ENTITY_Circle,
	ENTITY_Square,
	ENTITY_Polygon,
	ENTITY_Material,
	ENTITY_Texture,
	ENTITY_List,
	ENTITY_TrMatrix,
	ENTITY_AABBTest,
	ENTITY_SMaterial,
	ENTITY_Compounded,
	ENTITY_END
};

HYSW_RAYTRACER_NAMESPACE_END

#endif
