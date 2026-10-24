#include "Fixture.h"
#include <iostream>
#include <random>
#include <stdexcept>
namespace {unsigned checks=0;void Check(bool ok){++checks;if(!ok)throw std::runtime_error("Fixture assertion failed.");}template<class F>void Reject(F f){bool rejected=false;try{f();}catch(...){rejected=true;}Check(rejected);}}
int main(){try{
 for(const auto& original:{Fixture::Mouse(),Fixture::Keyboard()}){Fixture::Validate(original);const auto restored=Fixture::Parse(Fixture::Serialize(original));Check(restored.report_descriptor==original.report_descriptor&&restored.input_bytes==original.input_bytes);const auto c=Fixture::Configuration(restored);const auto r=Vdevice::Validate_Device(c);if(!r)throw std::runtime_error(r.message);Check(c.descriptors.size()==6&&c.delegate_controls);Check(c.descriptors[1].data[31]==original.input_bytes);}
 Check(Fixture::Hex(Fixture::ParseHex("01 02\nAB-CD"))=="01 02 AB CD");Reject([]{Fixture::ParseHex("0");});Reject([]{Fixture::ParseHex("0 1");});Reject([]{Fixture::ParseHex("GG");});Reject([]{Fixture::ParseHex("00 11",1);});
 auto initialKeyboard=Fixture::Keyboard();initialKeyboard.reports.front().data=Fixture::ParseHex("02 00 04 00 00 00 00 00");Check(Fixture::InitialInputState(initialKeyboard)==Fixture::Bytes(8));
 auto numbered=initialKeyboard;numbered.input_bytes=9;numbered.report_descriptor.insert(numbered.report_descriptor.begin(),{0x85,0x07});for(auto& r:numbered.reports)r.data.insert(r.data.begin(),7);auto expected=Fixture::Bytes(9);expected.front()=7;Check(Fixture::InitialInputState(numbered)==expected);numbered.reports.clear();Check(Fixture::InitialInputState(numbered)==expected);
 Reject([]{auto s=Fixture::Mouse();s.input_bytes=8;Fixture::Validate(s);});Reject([]{auto s=Fixture::Mouse();s.report_descriptor.pop_back();Fixture::Validate(s);});Reject([]{auto s=Fixture::Mouse();s.report_descriptor={0x75};Fixture::Validate(s);});Reject([]{auto s=Fixture::Mouse();s.reports[1].at_us=3600000001ULL;Fixture::Validate(s);});Reject([]{auto s=Fixture::Mouse();s.reports[2].at_us=1;Fixture::Validate(s);});Reject([]{auto s=Fixture::Mouse();s.reports[0].data.clear();Fixture::Validate(s);});
 Reject([]{Fixture::Parse("{}");});Reject([]{Fixture::Parse(std::string(4*1024*1024+1,' '));});Reject([]{Fixture::Parse(std::string(30,'[')+std::string(30,']'));});
 std::mt19937 random(7301);for(unsigned i=0;i<20000;++i){auto s=Fixture::Mouse();const auto pos=random()%s.report_descriptor.size();s.report_descriptor[pos]=static_cast<std::uint8_t>(random());try{Fixture::Validate(s);const auto json=Fixture::Serialize(s);Check(Fixture::Parse(json).report_descriptor==s.report_descriptor);}catch(const std::exception&){++checks;}}
 std::cout<<checks<<" checks passed; no driver opened, device created or input sent.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
