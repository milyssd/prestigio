#pragma once
class ChatHandler
{
public:
    explicit ChatHandler(void*) { }
    template<class... Args> void PSendSysMessage(char const*, Args...) { }
};
