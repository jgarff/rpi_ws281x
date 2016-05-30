using System;
using System.Linq;
using System.Runtime.InteropServices;

namespace rpi_ws281x
{
    public class WS281x:IDisposable
    {
        private ws2811_t _ws2811;
        private static readonly int[] ValidChannel1Gpios = {0,12,18,40,52};
        private static readonly int[] ValidChannel2Gpios = {0,13,19,41,53};

        #region ctor overloads

        public WS281x(int channel1LedCount, int channel1GpioPin)
            : this(channel1LedCount, channel1GpioPin, 255, false)
        {
        }

        public WS281x(int channel1LedCount, int channel1GpioPin, int channel2LedCount, int channel2GpioPin)
            : this(channel1LedCount, channel1GpioPin, 255, false, channel2LedCount, channel2GpioPin, 255, false)
        {
        }

        public WS281x(int channel1LedCount, int channel1GpioPin, int channel1Brightness, bool channel1Inverted)
            :this(800000, 5, channel1LedCount, channel1GpioPin, channel1Brightness, channel1Inverted)
        {
        }

        public WS281x(int channel1LedCount, int channel1GpioPin, int channel1Brightness, bool channel1Inverted,
            int channel2LedCount, int channel2GpioPin, int channel2Brightness, bool channel2Inverted)
            :this(800000, 5, channel1LedCount, channel1GpioPin, channel1Brightness, channel1Inverted, channel2LedCount, channel2GpioPin, channel2Brightness, channel2Inverted)
        {
        }

        public WS281x(uint frequency, int dma, int channel1LedCount, int channel1GpioPin, int channel1Brightness, bool channel1Inverted) 
            : this(frequency, dma, channel1LedCount, channel1GpioPin, channel1Brightness, channel1Inverted, 0, 0, 0, false)
        {
        }
        
        #endregion

        public WS281x(uint frequency, int dma,
            int channel1LedCount, int channel1GpioPin, int channel1Brightness, bool channel1Inverted,
            int channel2LedCount, int channel2GpioPin, int channel2Brightness, bool channel2Inverted)
        {
            if (!ValidChannel1Gpios.Contains(channel1GpioPin))
                throw new ArgumentException(String.Format("Invalid GPIO Pin {0}, must be one of ({1})", channel1GpioPin, String.Join(", ", ValidChannel1Gpios)), nameof(channel1GpioPin));

            if (!ValidChannel2Gpios.Contains(channel2GpioPin))
                throw new ArgumentException(String.Format("Invalid GPIO Pin {0}, must be one of ({1})", channel2GpioPin, String.Join(", ", ValidChannel2Gpios)), nameof(channel2GpioPin));

            _ws2811 = new ws2811_t
            {
                dmanum = 5,
                channel = new ws2811_channel_t[RPI_PWM_CHANNELS],
                freq = frequency
            };
            _ws2811.channel[1].brightness = 255;
            _ws2811.dmanum = dma;
            _ws2811.freq = frequency;
            _ws2811.channel[0].count = channel1LedCount;
            _ws2811.channel[0].gpionum = channel1GpioPin;
            _ws2811.channel[0].brightness = channel1Brightness;
            _ws2811.channel[0].invert = channel1Inverted ? 1 : 0;
            _ws2811.channel[1].count = channel2LedCount;
            _ws2811.channel[1].gpionum = channel2GpioPin;
            _ws2811.channel[1].brightness = channel2Brightness;
            _ws2811.channel[1].invert = channel2Inverted ? 1 : 0;

            var res = ws2811_init(ref _ws2811);
            if (res != 0)
            {
                throw new ExternalException("Error initializing rpi_ws281x", res);
            }

            Channel1 = new Led[_ws2811.channel[0].count];
            Channel2 = new Led[_ws2811.channel[1].count];
        }
        
        public void Render()
        {
            var channel1 = new int[_ws2811.channel[0].count];
            var channel2 = new int[_ws2811.channel[1].count];
            for (var i = 0; i < Channel1.Length; i++)
            {
                channel1[i] = Channel1[i].Color;
            }
            for (var i = 0; i < Channel2.Length; i++)
            {
                channel2[i] = Channel2[i].Color;
            }
            
            Marshal.Copy(channel1, 0, _ws2811.channel[0].leds, _ws2811.channel[0].count);
            Marshal.Copy(channel2, 0, _ws2811.channel[1].leds, _ws2811.channel[1].count);
            
            var res = ws2811_render(ref _ws2811);
            
            if (res != 0)
            {
                throw new ExternalException("DMA Error while waiting for previous DMA operation to complete");
            }
        }

        public Led[] Channel1 { get; }
        public Led[] Channel2 { get; }

        public void Wait()
        {
            var res = ws2811_wait(ref _ws2811);
            if (res != 0)
            {
                throw new ExternalException("DMA Error while waiting for previous DMA operation to complete");
            }
        }
        
        #region IDisposable

        private bool _disposed = false;

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (_disposed)
                return;
            if (disposing)
            {
                
                //clean managed
            }
            ws2811_fini(ref _ws2811);
            _disposed = true;
            //clean unmanaged
        }

        #endregion

        #region extern

        [DllImport("ws2811", CallingConvention = CallingConvention.Cdecl)]
        static extern int ws2811_init(ref ws2811_t ws2811);

        [DllImport("ws2811", CallingConvention = CallingConvention.Cdecl)]
        static extern void ws2811_fini(ref ws2811_t ws2811);

        [DllImport("ws2811", CallingConvention = CallingConvention.Cdecl)]
        static extern int ws2811_render(ref ws2811_t ws2811);

        [DllImport("ws2811", CallingConvention = CallingConvention.Cdecl)]
        static extern int ws2811_wait(ref ws2811_t ws2811);

        const int RPI_PWM_CHANNELS = 2;

        [StructLayout(LayoutKind.Sequential)]
        struct ws2811_t
        {
            public IntPtr device;
            public IntPtr rpi_hw;
            public uint freq;
            public int dmanum;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = RPI_PWM_CHANNELS)]
            public ws2811_channel_t[] channel;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct ws2811_channel_t
        {
            public int gpionum;
            public int invert;
            public int count;
            public int brightness;
            public int strip_type;
            public IntPtr leds;
        }

        #endregion
    }
}
