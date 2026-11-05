#include "logging/LogData.h"
#include <cstdlib>
#define CHECK(condition) do { if (!(condition)) { std::cerr << "Failed: " << #condition << " at " << __LINE__ << std::endl; std::exit(1); } } while (false)
#include <iostream>

int main() {
    using namespace tunuino;
    LogSample first, second;
    first.seconds=0; first.values[0]=0; first.values[2]=101.325; first.values[4]=-12.75;
    second=first; second.seconds=0.501234; second.values[0]=1234.567;
    std::vector<LogSample> source{first,second}, decoded;
    std::string error;
    const auto csv=encodeLogCsv(source);
    CHECK(decodeLogCsv(csv,decoded,error));
    CHECK(decoded.size()==2 && decoded[0].values[0]==0);
    CHECK(std::isnan(decoded[0].values[1]));
    CHECK(decoded[0].values[4]==-12.75);
    CHECK(decoded[1].seconds==second.seconds && decoded[1].values[0]==second.values[0]);
    auto reject=[&](const std::string &body) {
        const auto previous=decoded;
        CHECK(!decodeLogCsv(std::string(LogCsvHeader)+"\n"+body,decoded,error));
        CHECK(!error.empty() && decoded.size()==previous.size() && decoded[1].seconds==previous[1].seconds);
    };
    reject("0,1,2,3,4,5,6\n");
    reject("0,1,2,3,4,5,6,7,8\n");
    reject("-1,1,,,,,,\n");
    reject(",1,,,,,,\n");
    reject("0,nan,,,,,,\n");
    reject("0,inf,,,,,,\n");
    reject("0,12bad,,,,,,\n");
    reject("0,1,,,,,,\n0,2,,,,,,\n");
    reject("1,1,,,,,,\n0,2,,,,,,\n");
    reject("\n");
    std::string crlf;
    for (char c:csv) { if(c=='\n') crlf+='\r'; crlf+=c; }
    CHECK(decodeLogCsv(crlf,decoded,error));
    CHECK(!decodeLogCsv("not a log\n",decoded,error));
    std::cout << "Log CSV round-trip, missing channels, zero RPM, negative temperatures, CRLF, atomic rejection: passed\n";
}
