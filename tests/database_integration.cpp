// Link with the core's database/common libraries. Use only a disposable character database.
#include "PrestigioDatabase.h"
#include "MySQLThreading.h"
#include "Log.h"
#include "PreparedStatement.h"
#include "TestCheck.h"
#include <chrono>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;
void Wait(PrestigioDatabase& database, bool const& done)
{
    auto deadline = Clock::now() + std::chrono::seconds(25);
    while (!done && Clock::now() < deadline)
    {
        database.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(done);
}
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    sLog->Initialize();
    MySQL::Library_Init();
    auto& db = PrestigioDatabase::Instance();
    std::string mode = argv[2];
    auto started = Clock::now();
    db.Start(argv[1], mode != "disabled");
    CHECK(Clock::now() - started < std::chrono::milliseconds(100));
    bool done=false, ready=false;
    db.Check([&](bool ok){ready=ok;done=true;});
    Wait(db,done);
    if (mode == "unavailable" || mode == "disabled")
        CHECK(!ready);
    else
    {
        CHECK(ready);
        if (mode == "idle")
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            done=false;
            db.GetCount(424242,[&](bool ok,uint32 count){CHECK(ok && count==2);done=true;});
            Wait(db,done);
            CHECK(db.IsReady());
        }
        else
        {
            auto baseline=std::make_shared<PrestigeSaveTransaction>();
            auto reset=std::make_shared<PrestigeSaveTransaction>();
            baseline->Append("UPDATE characters SET level=80 WHERE guid=424242");
            reset->Append("UPDATE characters SET level=1 WHERE guid=424242");
            auto actions=new PreparedStatement<CharacterDatabaseConnection>(CHAR_DEL_CHAR_ACTION,1);
            actions->SetData(0,uint32(424242));
            reset->Append(actions);
            done=false;
            db.Save(424242,80,2,10,std::string(32,'a'),baseline,reset,false,[&](bool ok,uint32 count){CHECK(ok && count==1);done=true;});
            Wait(db,done);
            auto mustNotRun=std::make_shared<PrestigeSaveTransaction>();
            mustNotRun->Append("UPDATE characters SET level=79 WHERE guid=424242");
            done=false;
            db.Save(424242,80,2,10,std::string(32,'a'),baseline,mustNotRun,false,[&](bool ok,uint32 count){CHECK(ok && count==1);done=true;});
            Wait(db,done);
            done=false;
            db.Save(424242,80,2,10,std::string(32,'b'),baseline,reset,false,[&](bool ok,uint32 count){CHECK(ok && count==2);done=true;});
            Wait(db,done);
            auto original=std::make_shared<PrestigeSaveTransaction>();
            auto failed=std::make_shared<PrestigeSaveTransaction>();
            original->Append("UPDATE characters SET level=80 WHERE guid=424243");
            original->Append("INSERT INTO character_action(guid,spec,button,action,type) VALUES(424243,0,0,42,0)");
            failed->Append("UPDATE characters SET level=1 WHERE guid=424243");
            failed->Append("INSERT INTO character_action(guid,spec,button,action,type) VALUES(424243,0,0,43,0)");
            done=false;
            db.Save(424243,80,2,10,std::string(32,'c'),original,failed,false,[&](bool ok,uint32 count){CHECK(!ok && count==0);done=true;});
            Wait(db,done);
            done=false;
            db.Save(424243,80,2,10,std::string(32,'c'),original,reset,false,[&](bool ok,uint32 count){CHECK(!ok && count==0);done=true;});
            Wait(db,done); // cancelled receipts must not be replayed as successful resets
        }
    }
    db.Stop();
    CHECK(!db.IsReady());
    MySQL::Library_End();
    sLog->Close();
    std::cout << "PASS: asynchronous database mode " << mode << '\n';
}
