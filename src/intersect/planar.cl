

bool intersection_planer_unit_circle(float2 p) {
	return length(p) <= 1;
}

bool intersection_planer_unit_triangle(float2 p) {
	return 0 <= p.x && 0 <= p.y && p.x+p.y <= 1;
}

bool intersection_planer_unit_square(float2 p) {
	return 0 <= p.x && p.x <= 1 && 0 <= p.y && p.y <= 1;
}

// ref: http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
bool intersection_planer_polygon(float2 p, const __global float2* v, int cnt) {
	int i, j, c = 0;
	const float x = p.x;
	const float y = p.y;
	for(i=0, j=cnt; i!=cnt; j=i++) {
		const float2 a = v[i];
		const float2 b = v[j];
		c ^= ((b.y>p.y)!=(b.y>p.y)&&p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x);
	}
	return c;
}