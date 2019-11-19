//
// Created by Petr Flajsingr on 2019-01-25.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include "../time/now.h"
#include "../meta/glm.h"
#include "../meta/meta.h"
/**
 * Types of log messages
 */
enum class LogLevel { Verbose, Info, Status, Debug, Warning, Error };

namespace LoggerStreamModifiers {
struct verbose{};
struct info{};
struct status{};
struct debug{};
struct warning{};
struct error{};
struct flush{};
struct out{};
struct err{};

template <typename T>
constexpr LogLevel modifierToLogLevel() {
  if constexpr (std::is_same_v<T, verbose>) {
    return LogLevel::Verbose;
  }
  if constexpr (std::is_same_v<T, info>) {
    return LogLevel::Info;
  }
  if constexpr (std::is_same_v<T, status>) {
    return LogLevel::Status;
  }
  if constexpr (std::is_same_v<T, debug>) {
    return LogLevel::Debug;
  }
  if constexpr (std::is_same_v<T, warning>) {
    return LogLevel::Warning;
  }
  if constexpr (std::is_same_v<T, error>) {
    return LogLevel::Error;
  }
}

template <typename T>
constexpr bool is_logger_stream_modifier_v = is_one_of_v<T, verbose, info, status, debug, warning, error>;
template<typename T>
constexpr bool is_logger_flusher_v = std::is_same_v<flush, T>;
template<typename T>
constexpr bool is_stream_v = is_one_of_v<T, out, err>;
}

template <typename OutStream> class Logger {
private:
  /**
   *
   * @return Current time as HH-MM-SS
   */
  [[nodiscard]] std::string getTime() const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%H-%M-%S");
    return ss.str();
  }
  /**#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
   * Log tags
   * @param level
   * @return
   */
  [[nodiscard]] std::string levelToString(LogLevel level) const {
    switch (level) {
    case LogLevel::Verbose:
      return "";
    case LogLevel::Info:
      return "[INFO]";
    case LogLevel::Status:
      return "[STATUS]";
    case LogLevel::Debug:
      return "[DEBUG]";
    case LogLevel::Warning:
      return "[WARNING]";
    case LogLevel::Error:
      return "[ERROR]";
    }
    throw std::exception();
  }

  std::chrono::nanoseconds startTimeMs;
  std::chrono::nanoseconds endTimeMs;

  LogLevel defaultLevel = LogLevel::Verbose;
  bool defaultPrintTime = true;

  OutStream &outputStream;

  [[nodiscard]] std::string indent(unsigned int level) const {
    auto cnt = level * 2;
    return std::string(cnt, ' ');
  }

  template <typename T>
  void print(T value, unsigned int indentLevel = 0) const {
    using namespace std::string_literals;
    if constexpr (is_iterable_v<std::decay_t<T>> && !is_string_v<std::decay_t<T>>) {
      print(indent(indentLevel));
      print("Container, size: "s + std::to_string(value.size()));
      print(" {\n");

      for (const auto &val : value) {
        print(indent(indentLevel + 1));
        print(val, indentLevel + 1);
        print(",\n");
      }
      print(indent(indentLevel));
      print("},\n");
    } else if constexpr (is_vec_specialisation_v<T>) {
      print(indent(indentLevel));
      print("glm::vec" + std::to_string(value.length()) + ":");
      print(" {");
      for (auto i = 0; i < value.length(); ++i) {
        print(value[i]);
        if (i < value.length() - 1) {
          print(", ");
        }
      }
      print("}");
    } else {
      outputStream << value;
    }
  }
public:
  explicit Logger(OutStream &outputStream) : outputStream(outputStream) {}

  template <LogLevel Level, bool PrintTime = false, bool PrintNewLine = true, typename... T> void log(T ... message) const {
    if constexpr (Level != LogLevel::Verbose) {
      if constexpr (PrintTime) {
        outputStream << levelToString(Level) + " " + getTime() + ": ";
      } else {
        outputStream << levelToString(Level) + ": ";
      }
    }
    (print(message), ...);
    if (PrintNewLine) {
      outputStream << std::endl;
    }
  }
  void startTime() {
    startTimeMs = now<std::chrono::nanoseconds>();
  }
  void endTime() {
    endTimeMs = now<std::chrono::nanoseconds>();
  }

  /**
   * Print time difference between endTime() and startTime() calls
   */
  void printElapsedTime() {
    auto tmp = endTimeMs - startTimeMs;
    log<LogLevel::Verbose>("Time elapsed: " + std::to_string(tmp.count()) + " ns");
  }

  void setDefaultLevel(LogLevel logLevel) {
    defaultLevel = logLevel;
  }

  void setDefaultPrintTime(bool printTime) {
    defaultPrintTime = printTime;
  }

private:
  template <LogLevel logLevel>
  struct OutOperator {
    Logger &logger;
    explicit OutOperator(Logger &logger) :logger(logger) {}

    template <typename T>
    auto operator<<(const T &rhs) {
      if constexpr (LoggerStreamModifiers::is_logger_flusher_v<T>) {
        logger.outputStream.flush();
      } else if constexpr (logLevel == LogLevel::Verbose) {
        logger.log<logLevel, false, false>(rhs);
        return *this;
      } else {
        logger.log<logLevel, false, false>(rhs);
        return OutOperator<LogLevel::Verbose>(logger);
      }
    }
  };

public:

  template <typename T>
  auto operator<<(const T &rhs) {
    if constexpr(LoggerStreamModifiers::is_logger_stream_modifier_v<T>) {
      return OutOperator<LoggerStreamModifiers::modifierToLogLevel<T>()>(*this);
    } else if constexpr (LoggerStreamModifiers::is_logger_flusher_v<T>) {
      outputStream.flush();
      return *this;
    } else {
      log<LogLevel::Info, true, false>(rhs);
      return OutOperator<LogLevel::Verbose>(*this);
    }
  }

  template <typename Callable, typename Resolution = std::chrono::milliseconds>
  void measure(Callable &&callable, unsigned int iterations, std::string_view name = "") {
      using namespace MakeRange;
      static_assert(std::is_invocable_v<Callable>, "Callable must be invokable.");
      static_assert(is_duration_v<Resolution>, "Resolution must be duration type.");
      const auto startTime = now<Resolution>();
      for (auto i : until(0, iterations)) {
          callable();
      }
      const auto endTime = now<Resolution>();
      const auto totalTime = endTime - startTime;
      log<LogLevel::Verbose>("Measure time for: '", name, "', ", iterations, " iterations.");
      log<LogLevel::Verbose>("Total: ", totalTime.count(), " ", durationToString<Resolution>());
      log<LogLevel::Verbose>("Average: ", totalTime.count() / static_cast<float>(iterations), " ", durationToString<Resolution>());
  }
};

#endif // LOGGER_H