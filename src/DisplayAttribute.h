#pragma once

#include "ArrowInput.h"

namespace arrowinput {

class DisplayAttributeInfo final : public ITfDisplayAttributeInfo {
public:
    DisplayAttributeInfo();

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP GetGUID(GUID* guid) override;
    IFACEMETHODIMP GetDescription(BSTR* description) override;
    IFACEMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) override;
    IFACEMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE* attribute) override;
    IFACEMETHODIMP Reset() override;

private:
    std::atomic<ULONG> ref_count_;
    TF_DISPLAYATTRIBUTE attribute_;
};

class DisplayAttributeProvider final : public ITfDisplayAttributeProvider {
public:
    DisplayAttributeProvider();
    ~DisplayAttributeProvider();

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** enum_info) override;
    IFACEMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info) override;

private:
    std::atomic<ULONG> ref_count_;
};

HRESULT GetPreeditDisplayAttributeAtom(TfGuidAtom* atom);
HRESULT CreateDisplayAttributeEnumerator(IEnumTfDisplayAttributeInfo** enum_info);

}  // namespace arrowinput
