#include "DisplayAttribute.h"
#include "Config.h"

namespace arrowinput {

namespace {

TF_DISPLAYATTRIBUTE MakeDefaultAttribute()
{
    TF_DISPLAYATTRIBUTE attribute = {};
    attribute.crText.type = TF_CT_NONE;
    attribute.crBk.type = TF_CT_NONE;
    attribute.lsStyle = TF_LS_SOLID;
    attribute.fBoldLine = TRUE;
    attribute.crLine.type = TF_CT_SYSCOLOR;
    attribute.crLine.nIndex = COLOR_HIGHLIGHT;
    attribute.bAttr = TF_ATTR_INPUT;
    return attribute;
}

class DisplayAttributeEnum final : public IEnumTfDisplayAttributeInfo {
public:
    DisplayAttributeEnum() : ref_count_(1), index_(0) {}

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override
    {
        if (!result) {
            return E_POINTER;
        }
        *result = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IEnumTfDisplayAttributeInfo)) {
            *result = static_cast<IEnumTfDisplayAttributeInfo*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        ULONG count = --ref_count_;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    IFACEMETHODIMP Clone(IEnumTfDisplayAttributeInfo** enum_info) override
    {
        if (!enum_info) {
            return E_POINTER;
        }
        auto* clone = new (std::nothrow) DisplayAttributeEnum();
        if (!clone) {
            return E_OUTOFMEMORY;
        }
        clone->index_ = index_;
        *enum_info = clone;
        return S_OK;
    }

    IFACEMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** info, ULONG* fetched) override
    {
        if (!info) {
            return E_POINTER;
        }
        if (fetched) {
            *fetched = 0;
        }
        if (count == 0) {
            return S_OK;
        }

        info[0] = nullptr;
        if (index_ > 0) {
            return S_FALSE;
        }

        auto* attribute_info = new (std::nothrow) DisplayAttributeInfo();
        if (!attribute_info) {
            return E_OUTOFMEMORY;
        }
        info[0] = attribute_info;
        index_ = 1;
        if (fetched) {
            *fetched = 1;
        }
        return count == 1 ? S_OK : S_FALSE;
    }

    IFACEMETHODIMP Reset() override
    {
        index_ = 0;
        return S_OK;
    }

    IFACEMETHODIMP Skip(ULONG count) override
    {
        if (count == 0) {
            return S_OK;
        }
        if (index_ == 0) {
            index_ = 1;
            return count == 1 ? S_OK : S_FALSE;
        }
        return S_FALSE;
    }

private:
    std::atomic<ULONG> ref_count_;
    ULONG index_;
};

}  // namespace

DisplayAttributeInfo::DisplayAttributeInfo()
    : ref_count_(1), attribute_(MakeDefaultAttribute())
{
}

HRESULT DisplayAttributeInfo::QueryInterface(REFIID iid, void** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_ITfDisplayAttributeInfo)) {
        *result = static_cast<ITfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG DisplayAttributeInfo::AddRef()
{
    return ++ref_count_;
}

ULONG DisplayAttributeInfo::Release()
{
    ULONG count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT DisplayAttributeInfo::GetGUID(GUID* guid)
{
    if (!guid) {
        return E_POINTER;
    }
    *guid = kPreeditDisplayAttributeGuid;
    return S_OK;
}

HRESULT DisplayAttributeInfo::GetDescription(BSTR* description)
{
    if (!description) {
        return E_POINTER;
    }
    *description = SysAllocString(L"ArrowInput preedit");
    return *description ? S_OK : E_OUTOFMEMORY;
}

HRESULT DisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute)
{
    if (!attribute) {
        return E_POINTER;
    }
    *attribute = attribute_;
    return S_OK;
}

HRESULT DisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE* attribute)
{
    if (!attribute) {
        return E_POINTER;
    }
    attribute_ = *attribute;
    return S_OK;
}

HRESULT DisplayAttributeInfo::Reset()
{
    attribute_ = MakeDefaultAttribute();
    return S_OK;
}

DisplayAttributeProvider::DisplayAttributeProvider() : ref_count_(1)
{
    AddRefServer();
}

DisplayAttributeProvider::~DisplayAttributeProvider()
{
    ReleaseServer();
}

HRESULT DisplayAttributeProvider::QueryInterface(REFIID iid, void** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_ITfDisplayAttributeProvider)) {
        *result = static_cast<ITfDisplayAttributeProvider*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG DisplayAttributeProvider::AddRef()
{
    return ++ref_count_;
}

ULONG DisplayAttributeProvider::Release()
{
    ULONG count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT DisplayAttributeProvider::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** enum_info)
{
    return CreateDisplayAttributeEnumerator(enum_info);
}

HRESULT DisplayAttributeProvider::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info)
{
    if (!info) {
        return E_POINTER;
    }
    *info = nullptr;

    if (!IsEqualGUID(guid, kPreeditDisplayAttributeGuid)) {
        DebugLog(L"display attribute provider: unknown guid requested");
        return E_INVALIDARG;
    }

    auto* attribute_info = new (std::nothrow) DisplayAttributeInfo();
    if (!attribute_info) {
        return E_OUTOFMEMORY;
    }
    *info = attribute_info;
    return S_OK;
}

HRESULT CreateDisplayAttributeEnumerator(IEnumTfDisplayAttributeInfo** enum_info)
{
    if (!enum_info) {
        return E_POINTER;
    }
    auto* enumerator = new (std::nothrow) DisplayAttributeEnum();
    if (!enumerator) {
        return E_OUTOFMEMORY;
    }
    *enum_info = enumerator;
    return S_OK;
}

HRESULT GetPreeditDisplayAttributeAtom(TfGuidAtom* atom)
{
    if (!atom) {
        return E_POINTER;
    }
    *atom = TF_INVALID_GUIDATOM;

    ITfCategoryMgr* category_mgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&category_mgr));
    if (FAILED(hr)) {
        return hr;
    }

    hr = category_mgr->RegisterGUID(kPreeditDisplayAttributeGuid, atom);
    category_mgr->Release();
    return hr;
}

}  // namespace arrowinput
