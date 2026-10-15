#pragma once
#include "Common.h"
class WorldPacket
{
public:
    explicit WorldPacket(uint32 opcode=0, unsigned=8) : _opcode(opcode) { }
    uint32 GetOpcode() const { return _opcode; }
    unsigned size() const { return 8; }
    WorldPacket& operator>>(ObjectGuid& guid) { guid.id=42; return *this; }
    WorldPacket& operator<<(uint8) { return *this; }
private:
    uint32 _opcode;
};
