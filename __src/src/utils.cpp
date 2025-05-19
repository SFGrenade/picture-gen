#include "utils.h"

#include <random>
#include <sstream>

std::string replace( std::string const& source, std::string const& from, std::string const& to ) {
  std::string ret = source;
  if( !ret.empty() ) {
    size_t start_pos = 0;
    while( ( start_pos = ret.find( from, start_pos ) ) != std::string::npos ) {
      ret.replace( start_pos, from.length(), to );
      start_pos += to.length();
    }
  }
  return ret;
}

std::vector<std::string> split_multiline(std::string const& str ) {
  std::vector<std::string> ret;
  std::string line;
  std::istringstream ss(str);
  while (std::getline(ss, line)) {
    if (line.size() > 0) {
      ret.push_back(line);
    }
  }
  return ret;
}
