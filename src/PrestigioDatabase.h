#ifndef MOD_PRESTIGIO_HOLLOW_DATABASE_H
#define MOD_PRESTIGIO_HOLLOW_DATABASE_H

#include "Common.h"
#include <memory>
#include <string>

class PrestigioDatabase
{
public:
    static PrestigioDatabase& Instance();
    ~PrestigioDatabase();
    bool Open(std::string const& connectionInfo);
    void Close();
    bool IsReady() const;
    bool GetCount(uint32 guid, uint32& count);
    bool Record(uint32 guid, uint8 previousLevel, uint8 playerClass, uint8 race, uint32& count);

private:
    PrestigioDatabase();
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
#endif
