using System;
using System.Threading;

namespace rpi_ws281x
{
    static class Program
    {
        private static readonly AutoResetEvent CloseEvent = new AutoResetEvent(false);
        private static WS281x _ws2811;

        private const int GPIO_PIN = 18;
        private const int WIDTH = 8;
        private const int HEIGHT = 8;
        private const int LED_COUNT = WIDTH*HEIGHT;

        private static readonly Led[,] Matrix = new Led[WIDTH,HEIGHT];

        static void Main(string[] args)
        {
            using (_ws2811 = new WS281x(LED_COUNT, GPIO_PIN))
            {
                Console.CancelKeyPress += (s, e) =>
                {
                    e.Cancel = true;
                    CloseEvent.Set();
                };
                while (!CloseEvent.WaitOne(0))
                {
                    matrix_raise();
                    matrix_bottom();
                    matrix_render();
                    _ws2811.Render();
                    Thread.Sleep(1000/15);
                }
            }
        }

        static void matrix_render()
        {
            int x, y;

            for (x = 0; x < WIDTH; x++)
            {
                for (y = 0; y < HEIGHT; y++)
                {
                    _ws2811.Channel1[(y * WIDTH) + x] = Matrix[x,y];
                }
            }
        }

        static void matrix_raise()
        {
            int x, y;

            for (y = 0; y < (HEIGHT - 1); y++)
            {
                for (x = 0; x < WIDTH; x++)
                {
                    Matrix[x,y] = Matrix[x,y + 1];
                }
            }
        }

        static readonly int[] Dotspos = { 0, 1, 2, 3, 4, 5, 6, 7 };
        
        static readonly Led[] Dotcolors =
        {
            new Led{Color = 0x000000},
            new Led{Color = 0x201000},
            new Led{Color = 0x202000},
            new Led{Color = 0x002000},
            new Led{Color = 0x002020},
            new Led{Color = 0x000020},
            new Led{Color = 0x100010},
            new Led{Color = 0x200010}
        };
        static void matrix_bottom()
        {
            int i;

            for (i = 0; i < Dotspos.Length; i++)
            {
                Dotspos[i]++;
                if (Dotspos[i] > (WIDTH - 1))
                {
                    Dotspos[i] = 0;
                }

                Matrix[Dotspos[i],HEIGHT - 1] = Dotcolors[i];
            }
        }

    }
}
