from guizero import App, Text, PushButton, ButtonGroup, Window, Slider
import time
from rpi_ws281x import *
import argparse

LED_COUNT      = 300     # Number of LED pixels.
LED_PIN        = 18      # GPIO pin connected to the pixels (18 uses PWM!).
LED_PIN        = 10      # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ    = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA        = 10      # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 255     # Set to 0 for darkest and 255 for brightest
LED_INVERT     = False   # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL    = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53

strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS, LED_CHANNEL)
strip.begin()

appMaster = App(title = "LED Strip")
appMaster.when_closed = exit
appMaster.visible = False
app = Window(appMaster, title="Entry")
app.when_closed = exit
presets = Window(app, title="LED Presets")
presets.when_closed = exit
presets.visible = False
customizer = Window(app, title="Customizer",layout="grid",)
customizer.when_closed = exit
customizer.visible = False

presetPressed = ""

presetsGenerated = False

sliderRedVal = 0
sliderBlueVal = 0
sliderGreenVal = 0
sliderStartVal = 0
sliderEndVal = 0

counter = 0
congrats = Text(app)
congrats.hide() 

def pullLEDs(id):
    leds = []
    LedsFile = open("PresetLEDActuals.txt")
    line = LedsFile.readline()
    split = line.split(" ")
    while (split[0] != id) :
        line = LedsFile.readline()
        split = line.split(" ")
    start = int(split[1])
    end = int(LedsFile.readline().split(" ")[1])-2
    LedsFile.close()
    LedsFile = open("PresetLEDActuals.txt")
    lines = LedsFile.readlines()
    LedLines = []
    for i in list(range(start-1,end)) :
        LedLines.append(lines[i].strip("\n"))
    for l in LedLines :
        splitted = l.split(" ")
        skipping = False
        if (splitted[0] == "skipping") :
            skipping = True
            splitted.pop(0)
        toAdd = LEDs(splitted[0],splitted[1],splitted[2],splitted[3],splitted[4])
        toAdd.skipping = skipping
        leds.append(toAdd)
    return leds

class LEDs:
    skipping = False
    def __init__ (self, start, end, g, r, b):
        self.start = start
        self.end = end
        self.g = g
        self.r = r
        self.b = b
    def toStr(self) :
        return str(self.start) + " " + str(self.end) + " " + self.g + " " + self.r + " " + self.b
class GradientLEDs:
    skipping = False
    def __init__ (self, start, end, gMax, rMax, bMax, order, speed):
        self.start = start
        self.end = end
        self.gMax = gMax
        self.rMax = rMax
        self.bMax = bMax
        self.order = order
        if (speed >0 and speed <11) :
            self.speed = speed
        else :
            speed = 5

def buttonCount():
    global counter
    global testMessage
    global congrats
    counter += 1
    testMessage.hide()
    testMessage = Text(app,"Button Pushed " + str(counter) + " Time(s)")
    if counter % 25 == 0 :
        congrats = Text(app, "CONGRATS ON " + str(counter) + " PUSHES!")
        congrats.show()
    elif counter % 25 > 5 :
        congrats.hide()
    
def getPresets():
    presets = []
    Pfile = open("PresetList.txt", "r")
    for line in Pfile.readlines() :
        presets.append(line.strip("\n"))
    return presets

def hidePresets():
    presets.hide()
    app.show()

def computePresetButtonPress(name):
    print("computePresetButtonPress with " + name)
    leds = pullLEDs(name)
    for l in leds :
        print(l.toStr())
    hidePresets()

def presetsApp():
    global app
    global presetsGenerated
    global presetButtonList
    presetButtonList = []
    app.hide()
    presetList = getPresets()
    if not presetsGenerated :
        for p in presetList :
            presetButtonList.append(PushButton(presets, command=computePresetButtonPress,text=p,args=[p]))
    presetsGenerated = True
    presets.show()

def customColoring():
    global customizer
    customizer.show()
    app.hide()

    redText = Text(customizer, "Red:",grid=[0,0],align="left")
    redSlider = Slider(customizer, 0, 255,grid=[2,0],command=setRS,align="right")
    #redSlider.text_color("red2")

    greenText = Text(customizer, "Green:",grid=[0,1],align="left")
    greenSlider = Slider(customizer, 0, 255,grid=[2,1],command=setGS,align="right")
    #greenSlider.text_color("green2")
    greenSlider

    blueText = Text(customizer, "Blue:",grid=[0,2],align="left")
    blueSlider = Slider(customizer, 0, 255,grid=[2,2],command=setBS,align="right")
    #blueSlider.text_color("RoyalBlue1")

    startText = Text(customizer, "Start:", grid=[0,3],align="left")
    startSlider = Slider(customizer, 1, 299, grid=[2,3],command=setSS,align="right")

    endText = Text(customizer, "End:", grid=[0,4],align="left")
    endSlider = Slider(customizer, 2, 300, grid=[2,4],command=setES,align="right")
    
    testButton = PushButton(customizer,text = "Test", grid=[0,5],align="left")
    quitButton = PushButton(customizer,command=backToEntry,text="Back", grid=[1,5])
    SetButton = PushButton(customizer, command=setLedsFromSliders, text = "Set", grid=[1,5],align="right")

def backToEntry():
    global customizer
    global app
    global sliderable
    customizer.hide()
    app.show()

def setRS(sliderVal):
    global sliderRedVal
    sliderRedVal = sliderVal
def setGS(sliderVal):
    global sliderGreenVal
    sliderGreenVal = sliderVal
def setBS(sliderVal):
    global sliderBlueVal
    sliderBlueVal = sliderVal
def setSS(sliderVal):
    global sliderStartVal
    sliderStartVal = sliderVal
def setES(sliderVal):
    global sliderEndVal
    sliderEndVal = sliderVal

def setLedsFromSliders():
    global sliderRedVal
    global sliderBlueVal
    global sliderGreenVal
    global sliderStartVal
    global sliderEndVal
    for i in range(int(sliderStartVal),int(sliderEndVal)):
        strip.setPixelColor(i, Color(r,b,g))
    strip.show()



presetsButton = PushButton(app, command=presetsApp, text="Presets")
customizeButton = PushButton(app, command=customColoring, text="Customize")
testButton = PushButton(app,command=buttonCount,text="Push Me")
testMessage = Text(app,"Button Not Pushed")
appMaster.display()

