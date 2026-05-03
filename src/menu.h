#ifndef MENU_H
#define MENU_H

#include "stdbool.h"
typedef struct MenuState {
    bool host_menu;
    bool connect_menu;

    char ip[64];
    char port[16];

    bool ip_edit_mode;
    bool port_edit_mode;
} MenuState;
typedef struct Global Global;
void menu_loop(Global *global);
#endif // !MENU_H
