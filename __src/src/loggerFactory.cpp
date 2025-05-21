#include "loggerFactory.h"

std::shared_ptr< spdlog::sinks::stdout_color_sink_mt > LoggerFactory::consoleSink_ = nullptr;
std::shared_ptr< spdlog::sinks::basic_file_sink_mt > LoggerFactory::fileSink_ = nullptr;
std::string LoggerFactory::loggerPattern_ = "[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%n] [%l] %v";
std::map< std::string, spdlogger > LoggerFactory::loggers_;
std::mutex LoggerFactory::loggersMutex_;

void LoggerFactory::init( std::string const& logFileName, bool printOnStdOut ) {
  std::scoped_lock lock( LoggerFactory::loggersMutex_ );

  LoggerFactory::loggers_.clear();

  LoggerFactory::consoleSink_ = std::make_shared< spdlog::sinks::stdout_color_sink_mt >();
  LoggerFactory::consoleSink_->set_level( printOnStdOut ? spdlog::level::level_enum::trace : spdlog::level::level_enum::off );
  LoggerFactory::fileSink_ = std::make_shared< spdlog::sinks::basic_file_sink_mt >( logFileName, true );
  LoggerFactory::fileSink_->set_level( spdlog::level::level_enum::trace );

  spdlog::sinks_init_list truncatedSinkList = { LoggerFactory::fileSink_, LoggerFactory::consoleSink_ };
  spdlogger mainLogger = std::make_shared< spdlog::logger >( "main", truncatedSinkList.begin(), truncatedSinkList.end() );
  mainLogger->set_level( spdlog::level::level_enum::trace );
  mainLogger->flush_on( spdlog::level::level_enum::trace );
  mainLogger->set_pattern( LoggerFactory::loggerPattern_ );
  spdlog::register_logger( mainLogger );
  spdlog::set_default_logger( mainLogger );

  LoggerFactory::loggers_.insert_or_assign( "main", mainLogger );
}

void LoggerFactory::deinit() {
  std::scoped_lock lock( LoggerFactory::loggersMutex_ );

  LoggerFactory::loggers_.clear();
  spdlog::shutdown();
}

spdlogger LoggerFactory::get_logger( std::string const& name ) {
  spdlogger ret = nullptr;

  std::scoped_lock lock( LoggerFactory::loggersMutex_ );

  if( LoggerFactory::loggers_.find( name ) != LoggerFactory::loggers_.end() ) {
    ret = LoggerFactory::loggers_.at( name );
  } else {
    spdlog::sinks_init_list truncatedSinkList = { LoggerFactory::fileSink_, LoggerFactory::consoleSink_ };
    ret = std::make_shared< spdlog::logger >( name, truncatedSinkList.begin(), truncatedSinkList.end() );
    ret->set_level( spdlog::level::level_enum::trace );
    ret->flush_on( spdlog::level::level_enum::trace );
    ret->set_pattern( LoggerFactory::loggerPattern_ );
    spdlog::register_logger( ret );

    LoggerFactory::loggers_.insert_or_assign( name, ret );
  }

  return ret;
}
