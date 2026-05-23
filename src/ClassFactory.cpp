#include "ClassFactory.h"
#include "TextService.h"

namespace arrowinput {

ClassFactory::ClassFactory() : ref_count_(1)
{
    AddRefServer();
}

ClassFactory::~ClassFactory()
{
    ReleaseServer();
}

HRESULT ClassFactory::QueryInterface(REFIID iid, void** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IClassFactory)) {
        *result = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG ClassFactory::AddRef()
{
    return ++ref_count_;
}

ULONG ClassFactory::Release()
{
    ULONG count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT ClassFactory::CreateInstance(IUnknown* outer, REFIID iid, void** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    if (outer) {
        return CLASS_E_NOAGGREGATION;
    }

    auto* service = new (std::nothrow) TextService();
    if (!service) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = service->QueryInterface(iid, result);
    service->Release();
    return hr;
}

HRESULT ClassFactory::LockServer(BOOL lock)
{
    if (lock) {
        ++g_lock_count;
    } else {
        --g_lock_count;
    }
    return S_OK;
}

}  // namespace arrowinput
