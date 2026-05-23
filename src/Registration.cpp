#include "Registration.h"
#include "Config.h"

namespace arrowinput {

namespace {

std::wstring GuidToString(REFGUID guid)
{
    wchar_t buffer[64] = {};
    StringFromGUID2(guid, buffer, ARRAYSIZE(buffer));
    return buffer;
}

HRESULT GetModulePath(std::wstring* path)
{
    if (!path) {
        return E_POINTER;
    }

    wchar_t module_path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(g_module, module_path, ARRAYSIZE(module_path));
    if (length == 0 || length == ARRAYSIZE(module_path)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *path = module_path;
    return S_OK;
}

HRESULT SetRegString(HKEY root, const std::wstring& subkey, const wchar_t* name, const std::wstring& value)
{
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                                  KEY_WRITE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(result);
    }

    result = RegSetValueExW(key, name, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(result);
}

HRESULT RegisterComServer()
{
    std::wstring module_path;
    HRESULT hr = GetModulePath(&module_path);
    if (FAILED(hr)) {
        return hr;
    }

    const std::wstring clsid = GuidToString(kClsidTextService);
    const std::wstring base = L"Software\\Classes\\CLSID\\" + clsid;
    hr = SetRegString(HKEY_CURRENT_USER, base, nullptr, kTextServiceDescription);
    if (FAILED(hr)) {
        return hr;
    }
    hr = SetRegString(HKEY_CURRENT_USER, base + L"\\InprocServer32", nullptr, module_path);
    if (FAILED(hr)) {
        return hr;
    }
    hr = SetRegString(HKEY_CURRENT_USER, base + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) {
        return hr;
    }
    return S_OK;
}

HRESULT UnregisterComServer()
{
    const std::wstring clsid = GuidToString(kClsidTextService);
    RegDeleteTreeW(HKEY_CURRENT_USER, (L"Software\\Classes\\CLSID\\" + clsid).c_str());
    return S_OK;
}

HRESULT RegisterTextServiceProfile()
{
    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    if (FAILED(hr)) {
        return hr;
    }

    hr = profiles->Register(kClsidTextService);
    if (SUCCEEDED(hr)) {
        hr = profiles->AddLanguageProfile(kClsidTextService, MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
                                          kProfileGuid,
                                          const_cast<wchar_t*>(kTextServiceDescription),
                                          static_cast<ULONG>(wcslen(kTextServiceDescription)),
                                          nullptr, 0, 0);
    }

    profiles->Release();
    if (FAILED(hr)) {
        return hr;
    }

    ITfCategoryMgr* category_mgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&category_mgr));
    if (FAILED(hr)) {
        return hr;
    }

    HRESULT category_hr = category_mgr->RegisterCategory(
        kClsidTextService,
        GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
        kClsidTextService);
    if (SUCCEEDED(category_hr)) {
        category_hr = category_mgr->RegisterCategory(
            kClsidTextService,
            GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY,
            GUID_PROP_ATTRIBUTE);
    }
    category_mgr->Release();
    hr = category_hr;
    return hr;
}

HRESULT UnregisterTextServiceProfile()
{
    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    if (FAILED(hr)) {
        return hr;
    }

    profiles->RemoveLanguageProfile(kClsidTextService, MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), kProfileGuid);
    profiles->Unregister(kClsidTextService);
    profiles->Release();

    ITfCategoryMgr* category_mgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&category_mgr)))) {
        category_mgr->UnregisterCategory(
            kClsidTextService,
            GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY,
            GUID_PROP_ATTRIBUTE);
        category_mgr->UnregisterCategory(
            kClsidTextService,
            GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
            kClsidTextService);
        category_mgr->Release();
    }
    return S_OK;
}

}  // namespace

HRESULT RegisterServer()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool co_initialized = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        co_initialized = false;
    } else if (FAILED(hr)) {
        return hr;
    }

    HRESULT config_hr = EnsureUserConfig();
    if (FAILED(config_hr)) {
        hr = config_hr;
    } else {
        hr = RegisterComServer();
        if (SUCCEEDED(hr)) {
            hr = RegisterTextServiceProfile();
        }
    }

    if (co_initialized) {
        CoUninitialize();
    }
    return hr;
}

HRESULT UnregisterServer()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool co_initialized = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        co_initialized = false;
    } else if (FAILED(hr)) {
        return hr;
    }

    UnregisterTextServiceProfile();
    UnregisterComServer();

    if (co_initialized) {
        CoUninitialize();
    }
    return S_OK;
}

}  // namespace arrowinput
