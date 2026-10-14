void AddPrestigioHollowScripts();

// AzerothCore derives the entry point from the module directory name.
void Addmod_prestigio_hollowScripts()
{
    AddPrestigioHollowScripts();
}

void AddprestigioScripts()
{
    AddPrestigioHollowScripts();
}

// Compatibility with ZIPs downloaded under the previous repository name.
void Addprestigio_mainScripts()
{
    AddPrestigioHollowScripts();
}

// GitHub ZIP directory after renaming the repository to mod-prestigio-hollow.
void Addmod_prestigio_hollow_mainScripts()
{
    AddPrestigioHollowScripts();
}
