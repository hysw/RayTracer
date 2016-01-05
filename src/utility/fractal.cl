

float mandb(float x0, float y0) {
	x0 = x0*3.5 - 2.5;
	y0 = y0*2 - 1;
	float x = 0;
	float y = 0;
	float iteration = 0;
	int max_iteration = 256;
	while ( x*x + y*y < 65536  &&  iteration < max_iteration ) {
		float xtemp = x*x - y*y + x0;
		y = 2*x*y + y0;
		x = xtemp;
		iteration = iteration + 1;
	}
	if ( iteration < max_iteration ) {
		float nu = log( log(sqrt( x*x + y*y )) / log(2.0) ) / log(2.0);
		iteration = iteration + 1 - nu;
		// Rearranging the potential function.
		// Could remove the sqrt and multiply log(zn) by 1/2, but less clear.
		// Dividing log(zn) by log(2) instead of log(N = 1<<8)
		// because we want the entire palette to range from the
		// center to radius 2, NOT our bailout radius.
	}
	return clamp(iteration/max_iteration, 0.0, 0.99);
}

float julia(float x, float y) {
	//each iteration, it calculates: new = old*old + c, where c is a constant and old starts at current pixel
    float cRe, cIm;                   //real and imaginary part of the constant c, determinate shape of the Julia Set
    float newRe, newIm, oldRe, oldIm;   //real and imaginary parts of new and old
    float zoom = 1, moveX = 0, moveY = 0; //you can change these to zoom and change position
    int maxIterations = 256; //after how much iterations the function should stop

    //pick some values for the constant c, this determines the shape of the Julia Set
    cRe = -0.7;
    cIm = 0.27015;

	 //calculate the initial real and imaginary part of z, based on the pixel location and zoom and position values
        newRe = 1.5 * x / (0.5 * zoom) + moveX;
        newIm = y / (0.5 * zoom) + moveY;
        //i will represent the number of iterations
        int i;
        //start the iteration process
        for(i = 0; i < maxIterations; i++)
        {
            //remember value of previous iteration
            oldRe = newRe;
            oldIm = newIm;
            //the actual iteration, the real and imaginary part are calculated
            newRe = oldRe * oldRe - oldIm * oldIm + cRe;
            newIm = 2 * oldRe * oldIm + cIm;
            //if the point is outside the circle with radius 2: stop
            if((newRe * newRe + newIm * newIm) > 4) break;
        }
	return i/maxIterations;
}

float fractal_SierpinskiCarpet(float x, float y) {
	float frac = 1;
	while(frac>0.1 && (x<0.3333 || 0.6666<x || y<0.3333 || 0.6666<y)) {
		x = x*3; x = x - (int)x;
		y = y*3; y = y - (int)y;
		frac = frac * 0.7;
	}
	if(frac > 0.1) {
		return 1-frac;
	} else {
		return 1;
	}
}