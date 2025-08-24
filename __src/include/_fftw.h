#pragma once

#include <fftw3.h>
#include <memory>
#include <utility>

std::shared_ptr< fftwf_plan_s > make_fftw_shared_ptr( fftwf_plan s );

double A_weighting_db( double f );

typedef std::pair< double, double > SplinePoint;
SplinePoint catmullRom( SplinePoint const& p0, SplinePoint const& p1, SplinePoint const& p2, SplinePoint const& p3, double t );
