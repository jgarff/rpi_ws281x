using System.Runtime.InteropServices;

namespace rpi_ws281x
{
    [StructLayout(LayoutKind.Explicit)]
    public struct Led
    {
        [FieldOffset(0)]
        public byte A;

        [FieldOffset(1)]
        public byte R;

        [FieldOffset(2)]
        public byte G;

        [FieldOffset(3)]
        public byte B;

        [FieldOffset(0)]
        public int Color;
        
    }
}