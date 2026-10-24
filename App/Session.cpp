#include "Session.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
namespace Fixture {
namespace {
std::wstring Wide(const std::string& text){if(text.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);std::wstring out(static_cast<std::size_t>(n),0);MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),n);return out;}
template<class T,class=void>struct Demo{static bool Expired(const T&){return false;}};
template<class T>struct Demo<T,std::void_t<decltype(std::declval<const T&>().trial_expired)>>{static bool Expired(const T& value){return value.trial_expired;}};
}
struct Session::Impl {
    enum class Kind{Create,Send,Delete};
    struct Command{Kind kind=Kind::Delete;Scenario scenario;Bytes report;};
    HWND window=nullptr;std::mutex mutex;std::condition_variable wake;std::deque<Command> queue;std::thread worker;bool stop=false;Status status;
    void Publish(const Status& s){std::lock_guard<std::mutex> lock(mutex);status=s;if(window)PostMessageW(window,Changed,0,0);}
    void Run() noexcept {
        Vdevice::Device device;Scenario scenario;Bytes latest;Status state;
        const auto remove=[&]{if(device){const auto result=Vdevice::Delete_Device(device);if(!result)state.message=L"Delete failed: "+Wide(result.message);device=Vdevice::Device{};}state.connected=state.ready=state.busy=false;};
        for(;;){try{
            auto next=std::chrono::steady_clock::now();
            for(;;){
                std::deque<Command> commands;{std::unique_lock<std::mutex> lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(10),[&]{return stop||!queue.empty();});if(stop)break;commands.swap(queue);}
                for(auto& command:commands){
                    if(command.kind==Kind::Delete){remove();state.message=L"Disconnected. The owned virtual device has been removed.";Publish(state);continue;}
                    if(command.kind==Kind::Create){remove();scenario=std::move(command.scenario);latest=InitialInputState(scenario);state.sent=0;state.busy=true;state.message=L"Creating device; Windows is enumerating USB and HID...";Publish(state);device=Vdevice::Create_Device(Configuration(scenario));if(!device){state.busy=false;state.message=L"Create failed: "+Wide(device.message)+L" (Windows "+std::to_wstring(device.error)+L"). Install Vdevice and run live mode as administrator.";}else{state.connected=true;next=std::chrono::steady_clock::now();}Publish(state);continue;}
                    if(!device||!state.ready){state.message=L"Send rejected: wait until Windows has configured the fixture.";Publish(state);continue;}
                    auto check=scenario;check.reports={{0,command.report}};Validate(check);
                    const auto result=Vdevice::Send_Input(device,0x81,command.report);
                    if(!result)state.message=L"Send failed: "+Wide(result.message)+L" (Windows "+std::to_wstring(result.error)+L").";
                    else{latest=std::move(command.report);++state.sent;state.message=L"Report accepted by Vdevice; acceptance is not a delivery timestamp.";}Publish(state);
                }
                if(!device)continue;
                for(unsigned n=0;n<32;++n){
                    const auto output=Vdevice::Get_Output(0);
                    if(!output){if(output.timed_out||output.error==ERROR_TIMEOUT||output.error==ERROR_NO_MORE_ITEMS||output.error==ERROR_NO_DATA)break;state.message=L"Device event failed: "+Wide(output.message);remove();Publish(state);break;}
                    if(output.kind==Vdevice::Output_Kind::Control){
                        Bytes reply;bool stall=true;const auto& q=output.setup;
                        if(q[0]==0xA1&&q[1]==1&&q[3]==1&&q[4]==0&&q[5]==0&&(q[2]==0||(!latest.empty()&&q[2]==latest.front()))){reply=latest;stall=false;}
                        else if(q[0]==0x21&&(q[1]==9||q[1]==10||q[1]==11)&&q[4]==0&&q[5]==0)stall=false;
                        else if(q[0]==0xA1&&(q[1]==2||q[1]==3)&&q[4]==0&&q[5]==0){reply={static_cast<std::uint8_t>(q[1]==3?1:0)};stall=false;}
                        const auto result=Vdevice::Respond_Output(output,reply,stall);if(!result){state.message=L"Control response failed: "+Wide(result.message);remove();Publish(state);break;}
                    }
                }
                if(device&&std::chrono::steady_clock::now()>=next){
                    next=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
                    const auto info=Vdevice::Get_Info();
                    if(Demo<Vdevice::Info>::Expired(info)){remove();state.message=L"Vdevice demo expired. Restart Windows for a new allowance, or add titancsa on Discord for unrestricted driver licensing.";Publish(state);continue;}
                    if(!info||info.quiescing||info.removing){state.message=L"Vdevice is unavailable or stopping. Reconnect after setup/restart.";remove();Publish(state);continue;}
                    const auto d=std::find_if(info.devices.begin(),info.devices.end(),[&](const auto& item){return item.id==device.Id();});
                    const bool ready=d!=info.devices.end()&&d->plugged&&d->configured&&d->powered&&!d->error;
                    if(state.ready!=ready||state.busy){state.ready=ready;state.busy=!ready;state.message=ready?L"Ready. Reports affect Windows; check the target application before sending.":L"Waiting for Windows USB/HID configuration...";Publish(state);}
                }
            }
            remove();Publish(state);break;
        }catch(const std::exception& error){remove();state.message=L"Operation stopped: "+Wide(error.what());Publish(state);}
        catch(...){remove();state.message=L"Operation stopped after an unexpected error.";Publish(state);}}
    }
    void Add(Command command){std::lock_guard<std::mutex> lock(mutex);if(stop||queue.size()>=32){status.message=L"Command queue full or closing; no command was sent.";if(window)PostMessageW(window,Changed,0,0);return;}queue.push_back(std::move(command));wake.notify_one();}
};
Session::Session():impl_(std::make_unique<Impl>()){}
Session::~Session(){Stop();}
void Session::Start(HWND window){impl_->window=window;impl_->worker=std::thread([this]{impl_->Run();});}
void Session::Create(Scenario scenario){Validate(scenario);impl_->Add({Impl::Kind::Create,std::move(scenario),{}});}
void Session::Send(Bytes report){impl_->Add({Impl::Kind::Send,{},std::move(report)});}
void Session::Delete(){impl_->Add({Impl::Kind::Delete,{},{}});}
void Session::Stop(){{std::lock_guard<std::mutex> lock(impl_->mutex);impl_->stop=true;impl_->wake.notify_all();}if(impl_->worker.joinable())impl_->worker.join();}
Status Session::Read(){std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->status;}
}
