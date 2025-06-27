#pragma once

#include <fftw3.h>
#include <memory>

std::shared_ptr< fftwf_plan_s > make_fftw_shared_ptr( fftwf_plan s );

double A_weighting_db( double f );
