float3 intersect_ray_planar(Ray ray, float3 v0, float3 v1, float3 v2) {
	// ref: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
	float3 e1,e2,h,s,q,d=ray.d,p=ray.o;
	float a,f,u,v;

	e1 = v1 - v0;
	e2 = v2 - v0;

	h = cross(d,e2);
	a = dot(e1,h);

	if (a > -0.00001 && a < 0.00001)
		return -1;

	f = 1/a;
	s = p - v0;
	u = f * dot(s,h);

	q = cross(s,e1);
	v = f * dot(d,q);

	// at this stage we can compute t to find out where
	// the intersection point is on the line
	float t = f * dot(e2,q);

	if (t < 0.00001)
		return -1;
	return (float3)(t, u, v);
}

float3 intersect_ray_triangle(Ray ray, float3 v0, float3 v1, float3 v2) {
	// ref: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
	float3 e1,e2,h,s,q,d=ray.d,p=ray.o;
	float a,f,u,v;

	e1 = v1 - v0;
	e2 = v2 - v0;

	h = cross(d,e2);
	a = dot(e1,h);

	if (a > -0.00001 && a < 0.00001)
		return -1;

	f = 1/a;
	s = p - v0;
	u = f * dot(s,h);

	if (u < 0.0 || u > 1.0)
		return -1;

	q = cross(s,e1);
	v = f * dot(d,q);

	if (v < 0.0 || u + v > 1.0)
		return -1;

	// at this stage we can compute t to find out where
	// the intersection point is on the line
	float t = f * dot(e2,q);


	if (t < 0.00001)
		return -1;
	return (float3)(t, u, v);
}


float3 intersect_ray_sphere(Ray ray, float3 center, float r) {
	float3 o = ray.o;
	float3 d = ray.d;
	float ld = length(d);
	d = normalize(d);

	float3 to = center - o;
	// not consider the case that it is in a sphere
	float d2 = dot(to, to);
	float r2 = r*r;
	float t  = dot(to, d);
	float delta2 = t*t+r2-d2;
	if(delta2 < 0)
		return -1;
	float delta = sqrt(delta2);
	if(t - delta > 0.00001) //0.00001
		t = t - delta;
	else
		t = t + delta;
	if(t < 0.00001)
		return -1;
	return (float3)(t/ld, (o+d*t-center).xz/r);
}