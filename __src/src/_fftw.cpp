#include "_fftw.h"

#include <cmath>

std::shared_ptr< fftwf_plan_s > make_fftw_shared_ptr( fftwf_plan s ) {
  return std::shared_ptr< fftwf_plan_s >( s, []( fftwf_plan p ) { fftwf_destroy_plan( p ); } );
}

double A_weighting_db( double f ) {
  double f2 = f * f;
  double num = 12200.0 * 12200.0 * f2 * f2;
  double den = ( f2 + 20.6 * 20.6 ) * std::sqrt( ( f2 + 107.7 * 107.7 ) * ( f2 + 737.9 * 737.9 ) ) * ( f2 + 12200.0 * 12200.0 );
  double ra = num / den;
  return 20.0 * std::log10( ra ) + 2.0;  // +2 dB normalization
}
