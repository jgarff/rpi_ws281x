#include <stdint.h>

extern int board_info_init(void);
extern uint32_t board_info_peripheral_base_addr(void);
extern uint32_t board_info_sdram_address(void);
// jimbotel: externalize the new function 
extern uint32_t get_osc_freq(void);
