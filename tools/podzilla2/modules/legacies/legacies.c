#include "pz.h"

extern void new_bluecube_window();
extern void new_invaders_window();
extern void new_ipobble_window();
extern void new_mandel_window();
extern void new_matrix_window();
extern void new_mines_window();
extern void new_pong_window();
extern void new_steroids_window();
extern void new_tictactoe_window();
extern void new_tunnel_window();
extern void new_wumpus_window();
extern void lights_init();

PzModule *legacy_module;

static void legacies_init() 
{
    legacy_module = pz_register_module ("legacies", 0);
    pz_menu_add_legacy ("/Extras/Games/BlueCube", new_bluecube_window);
    pz_menu_add_legacy ("/Extras/Games/Invaders", new_invaders_window);
    pz_menu_add_legacy ("/Extras/Games/iPobble", new_ipobble_window);
    lights_init();
    pz_menu_add_legacy ("/Extras/Demos/MandelPod", new_mandel_window);
    pz_menu_add_legacy ("/Extras/Demos/Matrix", new_matrix_window);
    pz_menu_add_legacy ("/Extras/Games/Minesweeper", new_mines_window);
    pz_menu_add_legacy ("/Extras/Games/Pong", new_pong_window);
    pz_menu_add_legacy ("/Extras/Games/Steroids", new_steroids_window);
    pz_menu_add_legacy ("/Extras/Games/Tic-Tac-Toe", new_tictactoe_window);
    pz_menu_add_legacy ("/Extras/Games/Tunnel", new_tunnel_window);
    pz_menu_add_legacy ("/Extras/Games/Hunt the Wumpus", new_wumpus_window);
}

PZ_MOD_INIT (legacies_init)
