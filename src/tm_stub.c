// Minimal tmFunction implementation.
// Keep all exported symbols so existing ASM offsets remain valid.
#include "../MexTK/mex.h"

typedef struct MatchInit MatchInit;
typedef struct HSD_Archive HSD_Archive;
typedef struct GOBJ GOBJ;
typedef struct AllowedCharacters AllowedCharacters;


// implemented in bbox.c
void BBox_Init(void);

void *EventPages[] = {0};

void *GetEventDesc(int page, int event)
{
    (void)page;
    (void)event;
    return 0;
}

char *GetEventName(int page, int event)
{
    (void)page;
    (void)event;
    return (char *)"";
}

char *GetEventDescription(int page, int event)
{
    (void)page;
    (void)event;
    return (char *)"";
}

char *GetPageName(int page)
{
    (void)page;
    return (char *)"";
}

int GetPageEventNum(int page)
{
    (void)page;
    return 0;
}

char *GetTMVersShort(void)
{
    return (char *)"";
}

char *GetTMVersLong(void)
{
    return (char *)"";
}

char *GetTMCompile(void)
{
    return (char *)"";
}

int GetPageNum(void)
{
    return 0;
}

unsigned char GetCSSType(int page, int event)
{
    (void)page;
    (void)event;
    return 0;
}

unsigned char GetIsSelectStage(int page, int event)
{
    (void)page;
    (void)event;
    return 0;
}

signed char GetCPUFighter(int page, int event)
{
    (void)page;
    (void)event;
    return 0;
}

short GetStage(int page, int event)
{
    (void)page;
    (void)event;
    return 0;
}

char *GetEventFile(int page, int event)
{
    (void)page;
    (void)event;
    return (char *)"";
}

char *GetCSSFile(int page, int event)
{
    (void)page;
    (void)event;
    return (char *)"";
}

void EventInit(int page, int eventID, MatchInit *matchData)
{
    (void)page;
    (void)eventID;
    (void)matchData;
}

void OnSceneChange(void)
{
}

void OnBoot(void)
{
}

void OnStartMelee(void)
{
    BBox_Init();
}

void OnFileLoad(HSD_Archive *archive)
{
    (void)archive;
}

GOBJ *Message_Display(int msg_kind, int queue_num, int msg_color, char *format, ...)
{
    (void)msg_kind;
    (void)queue_num;
    (void)msg_color;
    (void)format;
    return 0;
}

unsigned char GetScoreType(int page, int event)
{
    (void)page;
    (void)event;
    return 0;
}

int GetPageEventOffset(int pageID)
{
    (void)pageID;
    return 0;
}

int GetJumpTableOffset(int pageID, int eventID)
{
    (void)pageID;
    (void)eventID;
    return 0;
}

AllowedCharacters *GetEventCharList(int eventID, int pageID)
{
    (void)eventID;
    (void)pageID;
    return 0;
}

