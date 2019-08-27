/*
 * This needs cleaning up and expanding.  The MODEL_* defines shoudl be in
 * board_info.h, it currently identifies everything not a B2 as a B, it
 * should have an accessor function for the board model and revision, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "board_info.h"

// jimbotel: move OSC_FREQ here from ws2811.c as this board-dependent
#define OSC_FREQ                                 19200000   // crystal frequency
#define OSC_FREQ_2711                            54000000   // raspberry Pi 4B uses BCM2711 Crystal 19.2->54M

// jimbotel: add model 4B
enum
{
   MODEL_UNKNOWN,
   MODEL_A,
   MODEL_A_PLUS,
   MODEL_B,
   MODEL_B_PLUS,
   MODEL_B_2,
   MODEL_4B,
};

static int board_info_initialised = 0;
static int board_model = MODEL_UNKNOWN;
static int board_revision;

static void
fatal(char *fmt, ...)
{
   va_list ap;

   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   exit(1);
}

// jimbotel: add get_osc_freq function to obtain osc_freq of the board
uint32_t get_osc_freq(void)
{
   uint32_t osc_freq;
   board_info_init();

   if(board_info_initialised == 0)
      return -1;

   if(board_model ==  MODEL_4B)
        osc_freq = OSC_FREQ_2711;
    else
        osc_freq = OSC_FREQ;

   return osc_freq;
}

static unsigned get_dt_ranges(const char *filename, unsigned offset)
{
   unsigned address = ~0;
   FILE *fp = fopen(filename, "rb");
   if (fp)
   {
      unsigned char buf[4];
      fseek(fp, offset, SEEK_SET);
      if (fread(buf, 1, sizeof buf, fp) == sizeof buf)
         address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
      fclose(fp);
   }

   return address;
}

uint32_t board_info_peripheral_base_addr(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);

   board_info_init();

   // jimbotel: in the pi4, the address obtained is 0.
   if (address == 0)
   {
      if (board_model == MODEL_4B)
         return 0xFE000000;
   }

   if (address == (unsigned) ~0)
   {
      if (board_model == MODEL_B_2)
         return 0x3f000000;
      // jimbotel: I also added here just in case, however I haven't seen address being all 1s
      else if (board_model == MODEL_4B)
         return 0xFE000000;
      else
         return 0x20000000;
   }
   else
   {
      return address;
   }
}

uint32_t board_info_sdram_address(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/axi/vc_mem/reg", 8);

   board_info_init();

   if (address == (unsigned)~0)
   {
      // jimbotel: Pi4 shares the same sdram address as previous generation
      if (board_model == MODEL_B_2 || board_model == MODEL_4B)
         return 0xc0000000;
      else
         return 0x40000000;
   }
   else
   {
      return address;
   }
}

int board_info_init(void)
{
   char buf[128], revstr[128], modelstr[128];
   char *ptr, *end, *res;
   FILE *fp;

   if (board_info_initialised)
      return 0;

   revstr[0] = modelstr[0] = '\0';

   fp = fopen("/proc/cpuinfo", "r");

   if (!fp)
      fatal("Unable to open /proc/cpuinfo: %m\n");

   while ((res = fgets(buf, 128, fp))) {
      if (!strncasecmp("model name", buf, 8))
         memcpy(modelstr, buf, 128);
      else if (!strncasecmp(buf, "revision", 8))
         memcpy(revstr, buf, 128);
   }
   fclose(fp);

   if (modelstr[0] == '\0')
      fatal("No 'Model name' record in /proc/cpuinfo\n");
   if (revstr[0] == '\0')
      fatal("No 'Revision' record in /proc/cpuinfo\n");

   if (strstr(modelstr, "ARMv6"))
      board_model = MODEL_B;
   else if (strstr(modelstr, "ARMv7"))
      board_model = MODEL_B_2;
   else
      fatal("Cannot parse the model name string\n");

   ptr = revstr + strlen(revstr) - 3;
   board_revision = strtol(ptr, &end, 16);

   // jimbotel: according to https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md , last two digits of revision = 17 (hex 11) for Pi4
   // jimbotel: board_revision will become 2 a few lines below. Not sure why board_revision is finally set to either 1 or 2, I just didn't want to change previous behavior.
   if (board_revision == 17)
      board_model = MODEL_4B;

   if (end != ptr + 2)
      fatal("Failed to parse Revision string\n");

   if (board_revision < 1)
      fatal("Invalid board Revision\n");
   else if (board_revision < 4)
      board_revision = 1;
   else
      board_revision = 2;

   board_info_initialised = 1;

   return 0;
}

