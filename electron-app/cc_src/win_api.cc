#include "napi.h"

#include <Windows.h>


namespace
{
  Napi::Value GetDuplicateHandle(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    Napi::HandleScope scope(env);

    HANDLE hShared = (HANDLE)strtoul("0x0000000000000504", nullptr, 16);

    // Get the handle of process A (need to know the PID of process A)
    DWORD dwSourceProcessId = 28828;  // Replace with the actual PID of process A
    HANDLE hSourceProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwSourceProcessId);
    if (!hSourceProcess) {
        //printf("Failed to open source process! Error code: %lu\n", GetLastError());
        //Cleanup();
        Napi::Error::New(env, "Failed to open source process").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Duplicate handle to current process
    HANDLE hDuplicatedHandle = nullptr;
    BOOL bSuccess = DuplicateHandle(
        hSourceProcess,           // Source process handle
        hShared,                  // Source handle
        GetCurrentProcess(),      // Target process (current process)
        &hDuplicatedHandle,       // Output: duplicated handle
        0,                        // Access rights (0 means use source handle permissions)
        FALSE,                    // Do not inherit
        DUPLICATE_SAME_ACCESS     // Duplicate same access rights
    );

    CloseHandle(hSourceProcess);  // Close source process handle

    if (!bSuccess) {
        Napi::Error::New(env, "Failed to duplicate handle").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Convert HANDLE to Buffer and return
    // HANDLE is a pointer type, size is sizeof(HANDLE) bytes
    uint8_t* handleBytes = reinterpret_cast<uint8_t*>(&hDuplicatedHandle);
    Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(env, handleBytes, sizeof(HANDLE));
    
    return buffer;
  }
} // namespace


Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getDuplicateHandle"), Napi::Function::New(env, GetDuplicateHandle));
    return exports;
}


NODE_API_MODULE(win_api, InitAll)