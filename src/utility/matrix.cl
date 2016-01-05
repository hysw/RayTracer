float3 M4V3_mul(float16 m, float3 v) {
	return (float3)(
		dot(v, m.s012),
		dot(v, m.s456),
		dot(v, m.s89a));
}

float3 M4P3_mul(float16 m, float3 v) {
	return (float3)(
		dot(v, m.s012)+m.s3,
		dot(v, m.s456)+m.s7,
		dot(v, m.s89a)+m.sb);
}

float16 M4_tr(float16 m) {
	return m.s048c159d26ae37bf;
}

float16 mmul(float16 a, float16 b) {
	return (float16)(
		dot(a.s0123, b.s048c),dot(a.s0123, b.s159d),dot(a.s0123, b.s26ae),dot(a.s0123, b.s37bf),
		dot(a.s4567, b.s048c),dot(a.s4567, b.s159d),dot(a.s4567, b.s26ae),dot(a.s4567, b.s37bf),
		dot(a.s89ab, b.s048c),dot(a.s89ab, b.s159d),dot(a.s89ab, b.s26ae),dot(a.s89ab, b.s37bf),
		dot(a.scdef, b.s048c),dot(a.scdef, b.s159d),dot(a.scdef, b.s26ae),dot(a.scdef, b.s37bf));
}
float16 M4_ident(){return (float16)(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);}