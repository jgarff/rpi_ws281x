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

enum
{
   MODEL_UNKNOWN,
   MODEL_A,
   MODEL_A_PLUS,
   MODEL_B,
   MODEL_B_PLUS,
   MODEL_B_2,
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

   if (address == (unsigned) ~0)
   {
      if (board_model == MODEL_B_2)
         return 0x3f000000;
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
      if (board_model == MODEL_B_2)
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

