// macOS REAPER extension to run an action when any project is loaded
//
// 1. Grab the reaper-sdk (https://github.com/justinfrankel/reaper-sdk)
// 2. Grab WDL: git clone https://github.com/justinfrankel/WDL.git
// 3. Put this folder into the 'reaper-plugins' folder of the sdk
// 4. Build then copy or link the binary file into <REAPER resource directory>/UserPlugins
//
// macOS
// =====
//
// clang++ -fPIC -O2 -std=c++14 -I../../WDL/WDL -I../../sdk \
//         -mmacosx-version-min=10.11 -arch x86_64 -arch arm64 \
//         -dynamiclib reaper_onprojectload.cpp -o reaper_onprojectload.dylib
//
// Windows
// =======
//
// (Use the VS Command Prompt matching your REAPER architecture, eg. x64 to use the 64-bit compiler)
// cl /nologo /O2 /Z7 /Zo /DUNICODE /I..\..\WDL\WDL /I..\..\sdk reaper_onprojectload.cpp user32.lib /link /DEBUG /OPT:REF /PDBALTPATH:%_PDB% /DLL /OUT:reaper_onprojectload.dll
//
// MinGW64 appears to work, as well:
//  c++ -fPIC -O2 -std=c++14 -DUNICODE -I../../WDL/WDL -I../../sdk -shared reaper_onprojectload.cpp -o reaper_onprojectload.dll
//
// Linux
// =====
//
// c++ -fPIC -O2 -std=c++14 -IWDL/WDL -shared reaper_onprojectload.cpp -o reaper_onprojectload.so

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include <cstdio>

#define VERSION_STRING "1.0.0"

#define SECTION_ID "sockmonkey72"
#define KEY_ID "onprojectload"
#define KEY_FM_ID "onprojectload_frontmost"

static int infoCommandId = 0;
static int setCommandId = 0;
static int showCommandId = 0;
static int clearCommandId = 0;
static int frontmostToggleCommandId = 0;

static int actionToRun = 0;
static bool runOnFrontmostChange = false;

static ReaProject *frontmostProject = nullptr;

static bool loadAPI(void *(*getFunc)(const char *));
static void registerCustomAction();
static bool showInfo(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd);

static void handleActionId(const char *actionId);
static void handleFrontmostTimer();

static void processExtState();
static void runAction();
static void activeProjectCheck();
static int frontmostToggleCallback(int command_id);

static bool setAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd);
static bool showAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd);
static bool clearAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd);
static bool frontmostToggleAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd);

static void BeginLoadProjectStateFn(bool isUndo, struct project_config_extension_t *reg);
//static bool ProcessExtensionLineFn(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg); // returns BOOL if line (and optionally subsequent lines) processed (return false if not plug-ins line)
//static void SaveExtensionConfigFn(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg);

// project_config_extension_t config = { ProcessExtensionLineFn, SaveExtensionConfigFn, BeginLoadProjectStateFn, nullptr };
project_config_extension_t config = { nullptr, nullptr, BeginLoadProjectStateFn, nullptr };

extern "C" REAPER_PLUGIN_DLL_EXPORT
int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t *rec)
{
  if (!rec) {
    return 0;
  }
  if (rec->caller_version != REAPER_PLUGIN_VERSION
      || !loadAPI(rec->GetFunc))
  {
    return 0;
  }

  plugin_register("projectconfig", &config);
  registerCustomAction();

  const char *actionId = GetExtState(SECTION_ID, KEY_ID);
  if (actionId && *actionId) {
    plugin_register("timer", (void *)processExtState);
  }

  const char *wantsFrontmost = GetExtState(SECTION_ID, KEY_FM_ID);
  runOnFrontmostChange = (!strcmp(wantsFrontmost, "1")) ? true : false;
  handleFrontmostTimer();

  return 1;
}

void handleActionId(const char *actionId)
{
  actionToRun = NamedCommandLookup(actionId); // this doesn't work at extension load time
  if (actionToRun <= 0) {
    actionToRun = 0;
  }
}

int frontmostToggleCallback(int command_id)
{
  if (command_id != frontmostToggleCommandId) return -1;

  return (runOnFrontmostChange) ? 1 : 0;
}

void processExtState()
{
  const char *actionId = GetExtState(SECTION_ID, KEY_ID);
  handleActionId(actionId);
  plugin_register("-timer", (void *)processExtState);
}

bool showInfo(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  if (command != infoCommandId) return false;

  char infoString[512];
  snprintf(infoString, 512, "onprojectload // sockmonkey72\nRun an action on any project load\n\nVersion %s\n\n\nCopyright (c) 2022 Jeremy Bernstein\njeremy.d.bernstein@googlemail.com%s",
           VERSION_STRING, __DATE__);
  ShowConsoleMsg(infoString);
  return true;
}

bool setAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  if (command != setCommandId) return false;

  if (actionToRun > 0) {
    const char *actionName = kbd_getTextFromCmd(actionToRun, nullptr);
    if (actionName) {
      char message[512];
      snprintf(message, 512, "Overwrite action: %s ?" , actionName);
      if (ShowMessageBox(message, "onProjectLoad", 4) != 6) {
        return true;
      }
    }
  }

  char retString[512];
  if (GetUserInputs("onProjectLoad", 1, "Enter Action Identifier String", retString, 512)) {
    handleActionId(retString);
    if (actionToRun) {
      SetExtState(SECTION_ID, KEY_ID, retString, true);
    }
    else {
      ShowMessageBox("Bad Action Identifier String", "onProjectLoad", 0);
    }
  }
  return true;
}

bool showAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  if (command != showCommandId) return false;

  if (actionToRun > 0) {
    const char *actionName = kbd_getTextFromCmd(actionToRun, nullptr);
    if (actionName) {
      char message[512];
      snprintf(message, 512, "Action name: %s\n"
               "Action id string: %s\n"
               "Action cmd id: %d", actionName, ReverseNamedCommandLookup(actionToRun), actionToRun);
      ShowMessageBox(message, "onProjectLoad", 0);
    }
  }
  return true;
}

bool clearAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  if (command != clearCommandId) return false;

  if (actionToRun > 0) {
    const char *actionName = kbd_getTextFromCmd(actionToRun, nullptr);
    if (actionName) {
      char message[512];
      snprintf(message, 512, "Clear Action: %s ?", actionName);
      if (ShowMessageBox(message, "onProjectLoad", 4) == 6) {
        actionToRun = 0;
        SetExtState(SECTION_ID, KEY_ID, nullptr, true);
      }
    }
  }
  return true;
}

void activeProjectCheck()
{
  ReaProject *rp = EnumProjects(-1, nullptr, 0);
  if (rp != frontmostProject) {
    runAction();
    frontmostProject = rp;
  }
}

void handleFrontmostTimer()
{
  if (runOnFrontmostChange) {
    plugin_register("timer", (void *)activeProjectCheck);
  }
  else {
    plugin_register("-timer", (void *)activeProjectCheck);
  }
}

bool frontmostToggleAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd)
{
  if (command != frontmostToggleCommandId) return false;

  runOnFrontmostChange = !runOnFrontmostChange;
  handleFrontmostTimer();
  SetExtState(SECTION_ID, KEY_FM_ID, runOnFrontmostChange ? "1" : "0", true);

  return true;
}

void runAction()
{
  if (actionToRun > 0) {
    Main_OnCommand(actionToRun, 0);
  }
  plugin_register("-timer", (void *)runAction); // just run it once
}

void BeginLoadProjectStateFn(bool isUndo, struct project_config_extension_t *reg)
{
  // ShowConsoleMsg("beginprojectload\n");
  plugin_register("timer", (void *)runAction);
  frontmostProject = EnumProjects(-1, nullptr, 0);
}

//const char *actionTag = "SM72_OPLACTION ";

//bool ProcessExtensionLineFn(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
//{
//  if (!strncmp(line, actionTag, strlen(actionTag))) {
//    const char *actionId = line + strlen(actionTag);
//    if (*actionId) {
//      actionToRun = NamedCommandLookup(actionId);
//      if (actionToRun <= 0) {
//        actionToRun = 0;
//      }
//    }
//  }
//  return false;
//}
//
//void SaveExtensionConfigFn(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
//{
//  char line[512];
//  if (actionToRun > 0) {
//    snprintf(line, 512, "SM72_OPLACTION %s", ReverseNamedCommandLookup(actionToRun));
//    ctx->AddLine("%s", line);
//  }
//}
//
void registerCustomAction()
{
  custom_action_register_t infoCustAction {
    0,
    "SM72_OPLINFO",
    "sockmonkey72_onprojectload: Info (Run an action on any project load)",
    nullptr
  };

  infoCommandId = plugin_register("custom_action", &infoCustAction);
  plugin_register("hookcommand2", (void *)&showInfo);

  custom_action_register_t setCustAction {
    0,
    "SM72_OPLSETACTION",
    "sockmonkey72_onprojectload: Set action to run on project load",
    nullptr
  };

  setCommandId = plugin_register("custom_action", &setCustAction);
  plugin_register("hookcommand2", (void *)&setAction);

  custom_action_register_t showCustAction {
    0,
    "SM72_OPLSHOWACTION",
    "sockmonkey72_onprojectload: Display action being run on project load",
    nullptr
  };

  showCommandId = plugin_register("custom_action", &showCustAction);
  plugin_register("hookcommand2", (void *)&showAction);

  custom_action_register_t clearCustAction {
    0,
    "SM72_OPLCLEARACTION",
    "sockmonkey72_onprojectload: Clear action on project load",
    nullptr
  };

  clearCommandId = plugin_register("custom_action", &clearCustAction);
  plugin_register("hookcommand2", (void *)&clearAction);

  custom_action_register_t frontmostToggleCustAction {
    0,
    "SM72_OPLFRONTMOSTTOGGLE",
    "sockmonkey72_onprojectload: Run action on change to frontmost project",
    nullptr
  };

  frontmostToggleCommandId = plugin_register("custom_action", &frontmostToggleCustAction);
  plugin_register("hookcommand2", (void *)&frontmostToggleAction);
  plugin_register("toggleaction", (void *)&frontmostToggleCallback);
}

#define REQUIRED_API(name) {(void **)&name, #name, true}
#define OPTIONAL_API(name) {(void **)&name, #name, false}

static bool loadAPI(void *(*getFunc)(const char *))
{
  if (!getFunc) {
    return false;
  }

  struct ApiFunc {
    void **ptr;
    const char *name;
    bool required;
  };

  const ApiFunc funcs[] {
    REQUIRED_API(ShowConsoleMsg),
    REQUIRED_API(plugin_register),
    REQUIRED_API(GetUserInputs),
    REQUIRED_API(NamedCommandLookup),
    REQUIRED_API(ReverseNamedCommandLookup),
    REQUIRED_API(kbd_getTextFromCmd),
    REQUIRED_API(Main_OnCommand),
    REQUIRED_API(GetExtState),
    REQUIRED_API(SetExtState),
    REQUIRED_API(EnumProjects),
    REQUIRED_API(ShowMessageBox),
  };

  for (const ApiFunc &func : funcs) {
    *func.ptr = getFunc(func.name);

    if (func.required && !*func.ptr) {
      fprintf(stderr, "[reaper_onprojectload] Unable to import the following API function: %s\n", func.name);
      return false;
    }
  }

  return true;
}
