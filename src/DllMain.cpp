#include "ArrowInput.h"
#include "ClassFactory.h"
#include "DisplayAttribute.h"
#include "Registration.h"
#include "TextService.h"

namespace arrowinput {

HINSTANCE g_module = nullptr;
std::atomic<long> g_object_count = 0;
std::atomic<long> g_lock_count = 0;

void AddRefServer()
{
    ++g_object_count;
}

void ReleaseServer()
{
    --g_object_count;
}

}  // namespace arrowinput

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        arrowinput::g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return (arrowinput::g_object_count == 0 && arrowinput::g_lock_count == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    if (!IsEqualCLSID(clsid, arrowinput::kClsidTextService)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) arrowinput::ClassFactory();
    if (!factory) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = factory->QueryInterface(iid, result);
    factory->Release();
    return hr;
}

STDAPI DllRegisterServer()
{
    return arrowinput::RegisterServer();
}

STDAPI DllUnregisterServer()
{
    return arrowinput::UnregisterServer();
}
