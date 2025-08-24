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

SplinePoint catmullRom( SplinePoint const& p0, SplinePoint const& p1, SplinePoint const& p2, SplinePoint const& p3, double t ) {
  double t2 = t * t;
  double t3 = t2 * t;

  return { 0.5
               * ( ( 2.0 * p1.first ) + ( -p0.first + p2.first ) * t + ( 2.0 * p0.first - 5.0 * p1.first + 4.0 * p2.first - p3.first ) * t2
                   + ( -p0.first + 3.0 * p1.first - 3.0 * p2.first + p3.first ) * t3 ),
           0.5
               * ( ( 2.0 * p1.second ) + ( -p0.second + p2.second ) * t + ( 2.0 * p0.second - 5.0 * p1.second + 4.0 * p2.second - p3.second ) * t2
                   + ( -p0.second + 3.0 * p1.second - 3.0 * p2.second + p3.second ) * t3 ) };
}
