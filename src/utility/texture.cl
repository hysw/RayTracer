
float3 uvmap_TODO(float2 uv) {
	//(3-mandb(uv.x, uv.y))/3
	//(3-julia(uv.x, uv.y))/3;
	//(9-native_cos(uv.x*50))/10;
	//(9-native_sin(uv.y*50))/10;
	//(4+fractal_SierpinskiCarpet(uv.x, uv.y))/5;
	return (4+fractal_SierpinskiCarpet(uv.x, uv.y))/5;
}