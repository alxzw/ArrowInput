#pragma once

#include "ArrowInput.h"

namespace arrowinput {

class ClassFactory final : public IClassFactory {
public:
    ClassFactory();
    ~ClassFactory();

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** result) override;
    IFACEMETHODIMP LockServer(BOOL lock) override;

private:
    std::atomic<ULONG> ref_count_;
};

}  // namespace arrowinput
