#pragma once

template < typename T >
T my_mod( T a, T b ) {
  while( a >= b ) {
    a -= b;
  }
  while( a < 0 ) {
    a += b;
  }
  return a;
}
