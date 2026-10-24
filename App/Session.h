#pragma once
#include "Fixture.h"
#include <Windows.h>
#include <memory>
#include <string>
namespace Fixture {
constexpr UINT Changed=WM_APP+21;
struct Status {bool connected=false,ready=false,busy=false;std::uint64_t sent=0;std::wstring message=L"Disconnected. Validate offline, then explicitly create the fixture.";};
class Session {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    Session();~Session();
    void Start(HWND window);
    void Create(Scenario scenario);
    void Send(Bytes report);
    void Delete();
    void Stop();
    Status Read();
};
}
