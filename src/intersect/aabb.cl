bool test_AABB_impl(Ray ray, float2 x, float2 y, float2 z) {
	float2 t = (float2)(0, INFINITY);
	// FIXME buggy
	t = clamp(((x - ray.o.x)/ray.d.x), min(t.x, t.y), max(t.x, t.y));
	t = clamp(((y - ray.o.y)/ray.d.y), min(t.x, t.y), max(t.x, t.y));
	t = clamp(((z - ray.o.z)/ray.d.z), min(t.x, t.y), max(t.x, t.y));
	return fabs(t.x-t.y) > RAYINT_DISTANCE_MIN;
}

bool test_AABB(Env env, Ray ray, int vtx) {
	if(vtx == 0) return true; // we hit the world -_-
	float3 mins = env.v[vtx+0].xyz;
	float3 maxs = env.v[vtx+1].xyz;
	return test_AABB_impl(ray,
		(float2)(mins.x, maxs.x),
		(float2)(mins.y, maxs.y),
		(float2)(mins.z, maxs.z));
}