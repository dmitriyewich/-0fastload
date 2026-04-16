/*
 * Copyright (C) 2014 LINK/2012 <dma_2012@hotmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include <windows.h>
#include "Injector.h"
using namespace injector;

bool GetPrivateProfileBoolA(const char* lpAppName, const char* lpKeyName, bool bDefault, const char* lpFileName);
float GetPrivateProfileFloatA(const char* lpAppName, const char* lpKeyName, float fDefault, const char* lpFileName);

struct imfast_t
{
    uint32_t version;   // Just in case, you never know when you want to update a format, etc
    int      slot;      // Last used slot
};

// Configuration
const char* IniConfigName = ".\\imfast.ini";
const char* IniConfigSect = "CONFIG";
bool bEnable        = true;
bool bAutoLoad      = true;
bool bNoCopyright   = true;
bool bNoFading      = false;
bool bNoLoadBar     = false;
bool bNoLoadScreen  = false;
bool bLoopScreens   = true;
bool bNoLoadingTune = false;
bool bDetectSave = true;
float fScreenChangeTime = 5.0f;
int iStartGameAt = 3;
int iSoundDelay = 50;
int vkAvoidLoad = 17;  // VK_CONTROL

/*
 *  Read configuration file
 */
bool ReadConfigINI()
{
    bEnable = GetPrivateProfileBoolA(IniConfigSect, "ENABLE", bEnable, IniConfigName);
    bDetectSave = GetPrivateProfileBoolA(IniConfigSect, "DETECT_SAVE", bDetectSave, IniConfigName);
    bAutoLoad = GetPrivateProfileBoolA(IniConfigSect, "AUTO_LOAD", bAutoLoad, IniConfigName);
    bNoCopyright = GetPrivateProfileBoolA(IniConfigSect, "NO_COPYRIGHT", bNoCopyright, IniConfigName);
    bNoFading = GetPrivateProfileBoolA(IniConfigSect, "NO_FADING", bNoFading, IniConfigName);
    bNoLoadBar = GetPrivateProfileBoolA(IniConfigSect, "NO_LOADBAR", bNoLoadBar, IniConfigName);
    bNoLoadScreen = GetPrivateProfileBoolA(IniConfigSect, "NO_LOADSCREEN", bNoLoadScreen, IniConfigName);
    bLoopScreens = GetPrivateProfileBoolA(IniConfigSect, "LOOP_SCREENS", bLoopScreens, IniConfigName);
    bNoLoadingTune = GetPrivateProfileBoolA(IniConfigSect, "NO_LOADING_TUNE", bNoLoadingTune, IniConfigName);
    fScreenChangeTime = GetPrivateProfileFloatA(IniConfigSect, "SCREEN_TIME", fScreenChangeTime, IniConfigName);
    iStartGameAt = GetPrivateProfileIntA(IniConfigSect, "START_GAME_AT", iStartGameAt, IniConfigName);
    iSoundDelay = GetPrivateProfileIntA(IniConfigSect, "TUNE_LOAD_DELAY", iSoundDelay, IniConfigName);
    vkAvoidLoad = GetPrivateProfileIntA(IniConfigSect, "VKEY_AVOID_LOAD", vkAvoidLoad, IniConfigName);

    // Translate iStartGameAt to game's gGameState language
    switch(iStartGameAt)
    {
        case 0: iStartGameAt = 0; break;
        case 1: iStartGameAt = 1; break;
        case 2: iStartGameAt = 3; break;
        default:
        case 3: iStartGameAt = 5; break;
    }
    
    return bEnable;
}

/*
 *  Register the last save slot the user has used
 */
void RegisterLastSlot(int slot)
{
    save_manager::scoped_userdir udir;
    imfast_t data;
    
    if(FILE* f = fopen("imfast.b", "wb"))
    {
        data.version = 0;
        data.slot    = slot;
        fwrite(&data, sizeof(data), 1, f);
        fclose(f);
    }
}

/*
 *  Findout the last slot the user has used
 *  Returns -2 for none, -1 for new-game and 0+ for save slots
 */
int GetLastSlot()
{
    save_manager::scoped_userdir udir;
    imfast_t data;
    int slot = -2;
    
    // Take slot from imfast.b
    if(FILE* f = fopen("imfast.b", "rb"))
    {
        if(fread(&data, sizeof(data), 1, f)) slot = data.slot;
        fclose(f);
    }
    
    // If any save slot, check if it really exists
    if(slot >= 0)
    {
        char buf[64];
        sprintf(buf, "GTASAsf%d.b", slot + 1);
        if(GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES)
            slot = -2;
    }
    
    return slot;
}


/*
 *  Simulates that the copyright screen happened
 */
void SimulateCopyrightScreen()
{
    // Simulate that the copyright screen happened
    *memory_pointer(0x8D093C).get<int>() = 0;       // Previous splash index = copyright notice
    *memory_pointer(0xBAB340).get<float>()-= 1000.0;// Decrease timeSinceLastScreen, so it will change immediately
    *memory_pointer(0xBAB31E).get<char>() = 1;      // First Loading Splash
}

static int prevDisplayedSplash = 1;

/*
 *  Fix for the non-looping splashes
 */
void IncreaseDisplayedSplash()
{
    int& currDisplayedSplash = *memory_pointer(0x8D093C).get<int>();
    prevDisplayedSplash = 1 + currDisplayedSplash;  // (+1 because the offset the game uses to access the texture array)
    if(++currDisplayedSplash >= 7) currDisplayedSplash = 1;
}

/*
 *  Starts game at slot
 */
void StartGame(int slot)
{
    if(bAutoLoad)
    {
        if(bNoLoadingTune == false)
        {
            // Services the loading tune
            for(int i = 0; i < iSoundDelay; ++i)
            {
                // Choose between 0x507750 and 0x5078A0...
                //ThisCall<void>(0x507750, 0xB6BC90);           // CAudioEngine::Service
                ThisCall<void>(0x5078A0, 0xB6BC90, 1.0f);       // CAudioEngine::ServiceLoadingTune
            }
        }
        
        // Make the game load automatically
        *memory_pointer(0xBA6748+0x32).get<char>() = slot < 0;  // menu.bDeactivateMenu
        *memory_pointer(0xBA6748+0x15F).get<char>() = slot;     // menu.SaveNumber
        *memory_pointer(0xB72910).get<char>() = 0;              // game.bMissionPack

        if(slot >= 0)
        {
            // Simulate that we came into the menu and clicked to load game
            *memory_pointer(0xBA6748+0x15D).get<char>() = 13;
            *memory_pointer(0xBA6748+0x1B3C).get<char>() = 1;
        }
        else
        {
            // Some init before loading any game
            ThisCall<void>(0x573330, 0xBA6748);
        }
    }
}


extern "C" __declspec(dllexport) // Needed on MinGW...
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    auto& gvm = address_manager::singleton();
    gvm.init_gvm();
    if(fdwReason == DLL_PROCESS_ATTACH
    && gvm.IsSA() && gvm.GetMajorVersion() == 1 && gvm.GetMinorVersion() == 0
    && ReadConfigINI())
    {
        // Select game state to start with
        MakeNOP(0x747483, 6);           // Disable gGameState = 0 setting
        WriteMemory<int>(0xC8D4C0, iStartGameAt);  // Put the game where the user wants (default's to the copyright screen)
        
        if(bNoCopyright)
        {
            // Hook the copyright screen fading in/out and simulares that it has happened
            MakeNOP (0x748C2B, 5);
            MakeCALL(0x748C9A, raw_ptr(SimulateCopyrightScreen));
        }

        if(bAutoLoad)
        {
            typedef function_hooker<0x53ECCB, void()> qload_hook;
            typedef function_hooker<0x53E80E, int()> norender_hook;

            MakeNOP(0x748CBD, 2);   // Let FrontentIdle run even when minimized
            
            // FrontendIdle hook
            make_function_hook<qload_hook>([](qload_hook::func_type FrontendIdle) 
            {
                //
                //  We need to still run the frontend processing because it has some important stuff
                //  So let's run it once (not rendering anything btw) and then start the game.
                //
            
                static int nTimes = 0;
                
                // Disable menu audio
                scoped_nop<0x576C0C, 0x576C1D - 0x576C0C> noaudio1;
                scoped_nop<0x576C5A, 0x576C6C - 0x576C5A> noaudio2;
            
                // Don't take time rendering the menu and stuff
                scoped_hook<norender_hook> norender([](norender_hook::func_type)
                {
                    return 0;
                });
            
                // On the second processing tick, do the game start
                if(++nTimes == 2)
                {
                    // If the user is pressing the auto-load avoid key, don't load the game
                    bool bNoLoad = (GetAsyncKeyState(vkAvoidLoad) & 0xF000) != 0;
                    int slot;
                
                    // Check if we have a "last slot"
                    if(!bNoLoad)
                        bNoLoad = (slot = GetLastSlot()) == -2;

                    // If bNoLoad, unhook FrontendIdle so the menu will flow normally on the next tick,
                    // otherwise load game.
                    if(bNoLoad)
                        qload_hook::restore();
                    else
                        StartGame(slot);
                }
                
                // Process the frontend
                return FrontendIdle();
            });
        }

        if(bNoFading || bNoLoadScreen)
        {
            // Skip fading screen rendering
            MakeJMP(0x590AE4, 0x590C9E);
        }
        
        if(bNoLoadBar)
        {
            // Disable loading bar rendering
            MakeNOP(0x5905B4, 5);
        }
        
        if(bNoLoadScreen)
        {
            // Disable loading screen rendering
            MakeNOP(0x590D9F, 5);
            WriteMemory<uint8_t>(0x590D9F, 0xC3, true);
            // There's no point in a audio tune here
            bNoLoadingTune = true;
        }
        
        if(bNoLoadingTune)
        {
            // Disable audio tune from loading screen
            MakeNOP(0x748CF6, 5);
        }
        
        if(fScreenChangeTime != 5.0f)
        {
            // Replace time to change between splashes on loading screen
            WriteMemory<void*>(0x590DA4 + 2, &fScreenChangeTime, true); 
        }
        
        if(bLoopScreens)
        {
            // Fix the non-looping screen issue
            MakeCALL(0x590ADE, raw_ptr(IncreaseDisplayedSplash));
            MakeNOP(0x590ADE + 5, 1);
            // Use stored previous displayed splash for non-bugged fading
            WriteMemory<void*>(0x590042 + 2, &prevDisplayedSplash, true);
            WriteMemory<void*>(0x59059C + 1, &prevDisplayedSplash, true);
            WriteMemory<void*>(0x590B22 + 1, &prevDisplayedSplash, true);
            WriteMemory<void*>(0x590BF6 + 1, &prevDisplayedSplash, true);
            WriteMemory<void*>(0x590C69 + 1, &prevDisplayedSplash, true);
        }
        
        if(bDetectSave)
        {
            // Register whenever the user loads or save a game
            save_manager::on_load(RegisterLastSlot);
            save_manager::on_save(RegisterLastSlot);
        }
    }
    return TRUE;
}




/*
 * GetPrivateProfileBoolA
 *      WinAPI-like to get bool on the file
 */
bool GetPrivateProfileBoolA(const char* lpAppName, const char* lpKeyName, bool bDefault, const char* lpFileName)
{
    char buf[256];
    if(GetPrivateProfileStringA(lpAppName, lpKeyName, 0, buf, sizeof(buf), lpFileName))
    {
        if(buf[0] == 0) return bDefault;
        else if(buf[1] == 0) return buf[0] != '0';
        else return stricmp(buf, "FALSE");
    }
    return bDefault;
}

/*
 *  GetPrivateProfileFloatA
 */
float GetPrivateProfileFloatA(const char* lpAppName, const char* lpKeyName, float fDefault, const char* lpFileName)
{
    char buf[256];
    if(GetPrivateProfileStringA(lpAppName, lpKeyName, 0, buf, sizeof(buf), lpFileName))
    {
        float& fResult = fDefault;
        sscanf(buf, "%f", &fResult);
        return fResult;
    }
    return fDefault;
}

// g++ dllmain.cpp -oimfast.asi -std=c++11 -O3 -static -shared -s
