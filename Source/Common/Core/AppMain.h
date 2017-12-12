#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

struct GlobalState;

typedef struct GlobalState GlobalState;

typedef struct IAppMain
{
    void(*Initialize)(GlobalState**);
    void(*GlobalTick)(GlobalState*, float);
    void(*Finalize)(GlobalState*);
} IAppMain;

extern IAppMain* GetAppInterface();

#ifdef __cplusplus
}
#endif