#include "chlist.h"
#include "../driver/uart.h"
#include "../helper/bands.h"
#include "../helper/menu.h"
#include "../radio.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chcfg.h"
#include "textinput.h"
#include "vfo1.h"
#include <string.h>

typedef enum {
  MODE_INFO,
  MODE_TX,
  MODE_SCANLIST,
  MODE_SCANLIST_SELECT,
  MODE_DELETE,
  // MODE_TYPE,
  // MODE_SELECT,
} CHLIST_ViewMode;

static char *VIEW_MODE_NAMES[] = {
    "INFO",   //
    "TX",     //
    "SL",     //
    "SL SEL", //
    "DEL",    //
              // "TYPE",     //
              // "CH SEL",   //
};

static char *CH_TYPE_FILTER_NAMES[] = {
    [TYPE_FILTER_CH] = "CH",              //
    [TYPE_FILTER_CH_SAVE] = "CH sav",     //
    [TYPE_FILTER_BAND] = "BAND",          //
    [TYPE_FILTER_BAND_SAVE] = "BAND sav", //
    [TYPE_FILTER_VFO] = "VFO",            //
    [TYPE_FILTER_VFO_SAVE] = "VFO sav",   //
};

// TODO:
// filter
// - scanlist

bool gChSaveMode = false;
CHTypeFilter gChListFilter = TYPE_FILTER_CH;

static uint8_t viewMode = MODE_INFO;
static CH ch;
static char tempName[9] = {0};

static const Symbol typeIcons[] = {
    [TYPE_CH] = SYM_CH,         [TYPE_BAND] = SYM_BAND,
    [TYPE_VFO] = SYM_VFO,       [TYPE_SETTING] = SYM_SETTING,
    [TYPE_FILE] = SYM_FILE,     [TYPE_MELODY] = SYM_MELODY,
    [TYPE_FOLDER] = SYM_FOLDER, [TYPE_EMPTY] = SYM_MISC2,
};

static inline uint16_t getChannelNumber(uint16_t menuIndex) {
  return gScanlist[menuIndex];
}

static void renderItem(uint16_t index, uint8_t i, bool isCurrent) {
  index = getChannelNumber(index);
  CHANNELS_Load(index, &ch);
  uint8_t y = MENU_Y + i * MENU_ITEM_H;
  if (ch.meta.type) {
    PrintSymbolsEx(2, y + 8, POS_L, C_INVERT, "%c", typeIcons[ch.meta.type]);
    PrintMediumEx(13, y + 8, POS_L, C_INVERT, "%s", ch.name);
  } else {
    PrintMediumEx(13, y + 8, POS_L, C_INVERT, "CH-%u", index);
  }
  switch (viewMode) {
  case MODE_INFO:
    if (CHANNELS_IsFreqable(ch.meta.type)) {
      PrintSmallEx(LCD_WIDTH - 5, y + 8, POS_R, C_INVERT, "%u.%03u %u.%03u",
                   ch.rxF / MHZ, ch.rxF / 100 % 1000, ch.txF / MHZ,
                   ch.txF / 100 % 1000);
    }
    break;
  case MODE_SCANLIST:
    if (CHANNELS_IsScanlistable(ch.meta.type)) {
      UI_Scanlists(LCD_WIDTH - 32, y + 3, ch.scanlists);
    }
    break;
  case MODE_TX:
    PrintSmallEx(LCD_WIDTH - 5, y + 7, POS_R, C_INVERT, "%s",
                 ch.allowTx ? "ON" : "OFF");
    break;
  }
}

static uint16_t channelIndex;

static void save() {
  gChEd.scanlists = 0;
  CHANNELS_Save(getChannelNumber(channelIndex), &gChEd);
  // RADIO_LoadCurrentVFO();
  LogC(LOG_C_YELLOW, "Chlist Save");
  APPS_exit();
  APPS_exit();
}

static void saveNamed() {
  strncpy(gChEd.name, gTextinputText, 9);
  save();
}

static Menu chListMenu = {.render_item = renderItem, .itemHeight = MENU_ITEM_H};

static bool action(const MenuItem *item, KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && key == KEY_MENU) {
    if (gChSaveMode) {
      CHANNELS_LoadScanlist(gChListFilter, gSettings.currentScanlist);

      if (gChEd.name[0] == '\0') {
        gTextinputText = tempName;
        snprintf(gTextinputText, 9, "%lu.%05lu", gChEd.rxF / MHZ,
                 gChEd.rxF % MHZ);
        gTextInputSize = 9;
        // channelIndex = item->setting;
        gTextInputCallback = saveNamed;
        APPS_run(APP_TEXTINPUT);
      } else {
        // save();
      }
      return true;
    }
    LogC(LOG_C_YELLOW, "BAND Selected by user");
    // RADIO_TuneToMR(chNum);
    // Log("Tuned to band, exit app");
    APPS_exit();
    return true;
  }
  return false;
}

void CHLIST_init() {
  CHANNELS_LoadScanlist(gChListFilter, gSettings.currentScanlist);
  chListMenu.num_items = gScanlistSize;
  MENU_Init(&chListMenu);
  // TODO: set menu index
  /* if (gChListFilter == TYPE_FILTER_BAND ||
      gChListFilter == TYPE_FILTER_BAND_SAVE) {
    channelIndex = BANDS_GetScanlistIndex();
  }
  if (gScanlistSize == 0) {
    channelIndex = 0;
  } else if (channelIndex > gScanlistSize) {
    channelIndex = gScanlistSize - 1;
  } */
}

void CHLIST_deinit() { gChSaveMode = false; }

bool CHLIST_key(KEY_Code_t key, Key_State_t state) {
  // uint16_t chNum = getChannelNumber(channelIndex);
  bool longHeld = state == KEY_LONG_PRESSED;
  bool simpleKeypress = state == KEY_RELEASED;
  /* if (!gIsNumNavInput && longHeld && key == KEY_STAR) {
    NUMNAV_Init(channelIndex, 0, gScanlistSize - 1);
    gNumNavCallback = setMenuIndex;
    return true;
  }
  if (state == KEY_RELEASED) {
    if (gIsNumNavInput) {
      channelIndex = NUMNAV_Input(key) - 1;
      return true;
    }
  } */

  if (viewMode == MODE_SCANLIST || viewMode == MODE_SCANLIST_SELECT) {
    if ((longHeld || simpleKeypress) && (key > KEY_0 && key < KEY_9)) {
      if (viewMode == MODE_SCANLIST_SELECT) {
        gSettings.currentScanlist = CHANNELS_ScanlistByKey(
            gSettings.currentScanlist, key, longHeld && !simpleKeypress);
        SETTINGS_DelayedSave();
        CHLIST_init();
      } else {
        /* CHANNELS_Load(chNum, &ch);
        ch.scanlists = CHANNELS_ScanlistByKey(ch.scanlists, key, longHeld);
        CHANNELS_Save(chNum, &ch); */
      }
      return true;
    }
  }

  if (viewMode == MODE_DELETE && simpleKeypress && key == KEY_1) {
    // CHANNELS_Delete(chNum);
    return true;
  }
  if (viewMode == MODE_TX && simpleKeypress && key == KEY_1) {
    /* CHANNELS_Load(chNum, &ch);
    ch.allowTx = !ch.allowTx;
    CHANNELS_Save(chNum, &ch); */
    return true;
  }

  if (state == KEY_LONG_PRESSED) {
    switch (key) {
    case KEY_0:
      gSettings.currentScanlist = 0;
      SETTINGS_Save();
      CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
      CHLIST_init();
      break;
    default:
      break;
    }
  }

  if (MENU_HandleInput(key, state)) {
    return true;
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_0:
      switch (gChListFilter) {
      case TYPE_FILTER_CH:
        gChListFilter = TYPE_FILTER_BAND;
        break;
      case TYPE_FILTER_BAND:
        gChListFilter = TYPE_FILTER_VFO;
        break;
      case TYPE_FILTER_VFO:
        gChListFilter = TYPE_FILTER_CH;
        break;
      case TYPE_FILTER_CH_SAVE:
        gChListFilter = TYPE_FILTER_BAND_SAVE;
        break;
      case TYPE_FILTER_BAND_SAVE:
        gChListFilter = TYPE_FILTER_VFO_SAVE;
        break;
      case TYPE_FILTER_VFO_SAVE:
        gChListFilter = TYPE_FILTER_CH_SAVE;
        break;
      }
      CHANNELS_LoadScanlist(gChListFilter, gSettings.currentScanlist);
      return true;
    case KEY_STAR:
      viewMode = IncDecU(viewMode, 0, ARRAY_SIZE(VIEW_MODE_NAMES), true);
      return true;
    case KEY_MENU:
      return true;
    /* case KEY_PTT:
      RADIO_TuneToMR(chNum);
      APPS_run(APP_VFO1);
      return true;
    case KEY_F:
      gChNum = chNum;
      CHANNELS_Load(gChNum, &gChEd);
      APPS_run(APP_CH_CFG);
      return true; */
    case KEY_EXIT:
      /* if (gIsNumNavInput) {
        NUMNAV_Deinit();
        return true;
      } */
      APPS_exit();
      return true;
    default:
      break;
    }
  }
  return false;
}

void CHLIST_render() {
  MENU_Render();
  STATUSLINE_SetText("%s %s", CH_TYPE_FILTER_NAMES[gChListFilter],
                     VIEW_MODE_NAMES[viewMode]);
}
