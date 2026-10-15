// Link this against AzerothCore's database/common libraries and PrestigioDatabase.cpp.
#include "PrestigioDatabase.h"
#include "MySQLThreading.h"
#include "Log.h"
#include <cassert>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    sLog->Initialize();
    MySQL::Library_Init();
    auto& db = PrestigioDatabase::Instance();
    std::string mode = argv[2];
    bool opened = db.Open(argv[1]);
    if (mode == "unavailable")
    {
        assert(!opened && !db.IsReady());
        uint32 count = 123;
        assert(!db.Record(424242, 80, 2, 10, count));
        assert(count == 123);
    }
    else if (mode == "rollback")
    {
        assert(opened);
        uint32 count = 123;
        assert(!db.Record(424243, 80, 2, 10, count));
        assert(!db.IsReady() && count == 123);
    }
    else
    {
        assert(opened && db.IsReady());
        uint32 count = 99;
        assert(db.GetCount(424242, count) && count == 0);
        assert(db.Record(424242, 80, 2, 10, count) && count == 1);
        assert(db.GetCount(424242, count) && count == 1);
        assert(db.Record(424242, 80, 2, 10, count) && count == 2);
    }
    db.Close();
    assert(!db.IsReady());
    MySQL::Library_End();
    sLog->Close();
    std::cout << "PASS: database mode " << mode << '\n';
}
