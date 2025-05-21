#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

std::string replace( std::string const& source, std::string const& from, std::string const& to );

std::vector< std::string > split_multiline( std::string const& str );
