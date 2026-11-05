#pragma once
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>
#include <locale>
#include <string>
#include <vector>

namespace tunuino {
constexpr int LogChannelCount = 7;
constexpr size_t MaxLogSamples = 100000;
inline const char *LogNames[] = {"RPM", "TPS", "MAP", "Lambda", "CLT", "IAT", "Battery"};
inline const char *LogUnits[] = {"rpm", "%", "kPa", "lambda", "deg C", "deg C", "V"};
struct LogSample {
    double seconds = 0;
    std::array<double, LogChannelCount> values;
    LogSample() { values.fill(std::numeric_limits<double>::quiet_NaN()); }
};
inline const char *LogCsvHeader = "time_s,RPM,TPS,MAP,Lambda,CLT,IAT,Battery";
inline std::string encodeLogCsv(const std::vector<LogSample> &samples) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << LogCsvHeader << '\n' << std::setprecision(17);
    for (const auto &sample : samples) {
        out << sample.seconds;
        for (double value : sample.values) {
            out << ',';
            if (std::isfinite(value)) out << value;
        }
        out << '\n';
    }
    return out.str();
}
// Strict native CSV: empty fields preserve unavailable channels. Reject the
// entire input before replacing the visible log; never partly import a file.
inline bool decodeLogCsv(const std::string &text, std::vector<LogSample> &result, std::string &error) {
    auto fail = [&](const char *message) { error = message; return false; };
    if (text.size() > 16 * 1024 * 1024) return fail("Log exceeds 16 MB.");
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) return fail("Empty log.");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != LogCsvHeader) return fail("Unsupported CSV header.");
    std::vector<LogSample> parsed;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (parsed.size() >= MaxLogSamples) return fail("Log exceeds 100000 samples.");
        LogSample sample;
        size_t start = 0;
        for (int col = 0; col <= LogChannelCount; ++col) {
            const size_t comma = line.find(',', start);
            if ((col < LogChannelCount) != (comma != std::string::npos)) return fail("Invalid CSV column count.");
            const auto field = line.substr(start, comma == std::string::npos ? comma : comma - start);
            if (field.empty()) {
                if (col == 0) return fail("Missing timestamp.");
            } else {
                std::istringstream number(field);
                number.imbue(std::locale::classic());
                double value;
                if (!(number >> value) || !std::isfinite(value)) return fail("Invalid numeric value.");
                number >> std::ws;
                if (!number.eof()) return fail("Invalid numeric value.");
                if (col == 0) sample.seconds = value;
                else sample.values[col - 1] = value;
            }
            start = comma + 1;
        }
        if (sample.seconds < 0 || (!parsed.empty() && sample.seconds <= parsed.back().seconds))
            return fail("Timestamps must be nonnegative and strictly increasing.");
        parsed.push_back(sample);
    }
    if (parsed.empty()) return fail("No samples in log.");
    result = std::move(parsed);
    error.clear();
    return true;
}
}
