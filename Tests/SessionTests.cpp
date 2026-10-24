#include "Session.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace {std::atomic<unsigned> creates{0},deletes{0},sends{0},polls{0},responses{0};std::atomic<bool> connected{false},requestReport{false};std::mutex replyMutex;Fixture::Bytes lastReply;bool lastStall=true;void Require(bool ok){if(!ok)throw std::runtime_error("Fake Vdevice session assertion failed.");}template<class F>void Until(F ready){const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!ready()){if(std::chrono::steady_clock::now()>=limit)throw std::runtime_error("Fake Vdevice session timed out.");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}}
namespace Vdevice {
Device::~Device()=default;
Device::Device(Device&&) noexcept=default;
Device& Device::operator=(Device&&) noexcept=default;
Device::operator bool()const noexcept{return !token.empty();}
std::uint64_t Device::Id()const noexcept{return token.empty()?0:1;}
Device Create_Device(const Device_Config&){++creates;connected=true;Device d;d.token="fake-only";return d;}
Result Delete_Device(Device& d){if(d){++deletes;connected=false;d.token.clear();}return {};}
Info Get_Info(){++polls;Info i;if(connected){Device_Details d;d.id=1;d.plugged=d.configured=d.powered=true;i.devices.push_back(d);}return i;}
Result Send_Input(const Device&,std::uint8_t,const std::vector<std::uint8_t>&){++sends;return {};}
Output Get_Output(std::uint32_t){Output o;if(requestReport.exchange(false)){o.kind=Output_Kind::Control;o.device_id=1;o.request_id=1;o.setup={0xA1,1,0,1,0,0,8,0};}else{o.error=258;o.timed_out=true;}return o;}
Result Respond_Output(const Output&,const std::vector<std::uint8_t>& data,bool stall){{std::lock_guard<std::mutex> lock(replyMutex);lastReply=data;lastStall=stall;}++responses;return {};}
}
int main(){try{
 {Fixture::Session s;s.Start(nullptr);std::this_thread::sleep_for(std::chrono::milliseconds(20));Require(creates==0&&polls==0&&sends==0);s.Create(Fixture::Mouse());Until([&]{return s.Read().ready;});s.Send(Fixture::Bytes(7));Until([&]{return sends==1;});s.Delete();Until([&]{return !s.Read().connected;});Require(deletes==1);auto keyboard=Fixture::Keyboard();keyboard.reports.front().data[2]=4;s.Create(keyboard);Until([&]{return s.Read().ready;});requestReport=true;Until([&]{return responses==1;});{std::lock_guard<std::mutex> lock(replyMutex);Require(!lastStall&&lastReply==Fixture::Bytes(8));}Require(sends==1);s.Send(keyboard.reports.front().data);Until([&]{return sends==2;});requestReport=true;Until([&]{return responses==2;});{std::lock_guard<std::mutex> lock(replyMutex);Require(!lastStall&&lastReply==keyboard.reports.front().data);}s.Send(Fixture::Bytes(3));Until([&]{return !s.Read().connected;});Require(sends==2&&deletes==2);s.Create(Fixture::Keyboard());Until([&]{return s.Read().ready;});s.Stop();Require(deletes==3&&!connected);}
 std::cout<<"Fake Vdevice lifecycle passed: no implicit driver access, zero initial GET_REPORT independent of editor, cache updates only after Send, explicit cleanup/recovery. No real driver or input used.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
