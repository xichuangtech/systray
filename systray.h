extern void systray_ready();
extern void systray_menu_item_selected(int menu_id);
int nativeLoop(void);

void setIcon(const char* iconBytes, int length);
void setTitle(char* title);
void setTooltip(char* tooltip);
void add_or_update_menu_item(int menuId, char* title, char* tooltip, short disabled, short checked);
void add_or_update_menu_item_with_icon(int menuId, wchar_t* title, wchar_t* tooltip, wchar_t* iconFile, short disabled, short checked);
void quit();
