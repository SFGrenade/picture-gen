#ifndef GENERATOR_SPDLOG_H_
#define GENERATOR_SPDLOG_H_

#include <memory>
#include <string>

#ifndef FMT_USE_LOCALE
#define FMT_USE_LOCALE 1
#endif

// Including FMT headers
#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>

// Including SPDLOG headers
#ifndef SPDLOG_FMT_EXTERNAL
#define SPDLOG_FMT_EXTERNAL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

typedef std::shared_ptr< spdlog::logger > spdlogger;

template < typename T >
struct fmt::formatter< std::shared_ptr< T > > {
  constexpr auto parse( format_parse_context& ctx ) -> decltype( ctx.begin() ) {
    auto it = ctx.begin(), end = ctx.end();
    if( it != end && ( *it == 'p' ) )
      *it++;
    if( it != end && *it != '}' )
      throw format_error( "invalid format" );
    return it;
  }
  template < typename FormatContext >
  auto format( std::shared_ptr< T > const& p, FormatContext& ctx ) const -> decltype( ctx.out() ) {
    return fmt::format_to( ctx.out(), "{:p}", fmt::ptr( p ) );
  }
};

template < typename T >
struct fmt::formatter< std::unique_ptr< T > > {
  constexpr auto parse( format_parse_context& ctx ) -> decltype( ctx.begin() ) {
    auto it = ctx.begin(), end = ctx.end();
    if( it != end && ( *it == 'p' ) )
      *it++;
    if( it != end && *it != '}' )
      throw format_error( "invalid format" );
    return it;
  }
  template < typename FormatContext >
  auto format( std::unique_ptr< T > const& p, FormatContext& ctx ) const -> decltype( ctx.out() ) {
    return fmt::format_to( ctx.out(), "{:p}", fmt::ptr( p ) );
  }
};

template < typename T >
struct fmt::formatter< std::weak_ptr< T > > {
  constexpr auto parse( format_parse_context& ctx ) -> decltype( ctx.begin() ) {
    auto it = ctx.begin(), end = ctx.end();
    if( it != end && ( *it == 'p' ) )
      *it++;
    if( it != end && *it != '}' )
      throw format_error( "invalid format" );
    return it;
  }
  template < typename FormatContext >
  auto format( std::weak_ptr< T > const& p, FormatContext& ctx ) const -> decltype( ctx.out() ) {
    return fmt::format_to( ctx.out(), "{:p}", fmt::ptr( p ) );
  }
};

#endif /* GENERATOR_SPDLOG_H_ */
