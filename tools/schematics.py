#!/usr/bin/env python3
"""Draws the schematics of docs/ELECTRICAL.md into docs/images/electrical/ (SVG).

Needs schemdraw (`pip install schemdraw`, pure Python, no matplotlib):
    python3 tools/schematics.py
"""

from pathlib import Path

import schemdraw
import schemdraw.elements as elm

OUT = Path(__file__).resolve().parent.parent / "docs" / "images" / "electrical"
NOTE = "#52514e"   # notes, proposed parts

schemdraw.use("svg")
elm.style(elm.STYLE_IEC)


def drawing() -> schemdraw.Drawing:
    d = schemdraw.Drawing(show=False)
    d.config(unit=2.5, fontsize=11, font="sans-serif", bgcolor="#fcfcfb", lw=1.4)
    return d


def thermostat(d, at, length, label):
    """Bimetal thermostat (NC contact) drawn downwards from `at`."""
    sw = d.add(elm.Switch(nc=True).down().at(at).length(length).label(label, loc="bottom", ofst=0.25))
    d.add(elm.Label().at((sw.center[0] - 0.9, sw.center[1])).label("ϑ", fontsize=14))
    return sw


def mains() -> schemdraw.Drawing:
    d = drawing()
    top, bottom = 0.0, -13.0
    rail_end = 32.0

    # Supply: L through fuse and main switch onto the switched rail L'
    d.add(elm.Dot(open=True).at((0, top)).label("L", loc="left"))
    d.add(elm.Fuse().right().length(3).label("F1"))
    d.add(elm.Switch().right().length(3).label("S1 main switch"))
    d.add(elm.Line().right().tox(rail_end).label("L'", loc="right", halign="left"))
    d.add(elm.Dot(open=True).at((0, bottom)).label("N", loc="left"))
    d.add(elm.Line().right().tox(rail_end))
    d.add(elm.Dot(open=True).at((0, bottom - 1.5)).label("PE", loc="left"))
    d.add(elm.Line().right().length(2))
    d.add(elm.GroundChassis().label("chassis, boiler, group", loc="right", ofst=0.3))

    # U1: AC/DC module, 5 V to the controller board
    x = 6.5
    d.add(elm.Dot().at((x, top)))
    d.add(elm.Line().down().length(3))
    # Ic numbers the pins of a side from the bottom up
    u1 = d.add(elm.Ic(pins=[elm.IcPin(name="N", side="left", anchorname="N"),
                            elm.IcPin(name="L", side="left", anchorname="L"),
                            elm.IcPin(name="GND", side="right", anchorname="GND"),
                            elm.IcPin(name="+5V", side="right", anchorname="V5")],
                       size=(3.4, 2.5), pinspacing=1.0, leadlen=0.6, pinfontsize=10,
                       label="U1\nAC/DC\n5 V").right().anchor("L"))
    d.add(elm.Line().at(u1.N).left().length(0.8))
    d.add(elm.Line().toy(bottom))
    d.add(elm.Dot())
    d.add(elm.Line().at(u1.V5).right().length(0.8).label("+5V → sheet 2", loc="right", fontsize=10))
    d.add(elm.Line().at(u1.GND).right().length(0.8).label("GND → sheet 2", loc="right", fontsize=10))

    # Heater: safety thermostat, element, steam thermostat, then SSR || steam switch
    x = 16.5
    d.add(elm.Dot().at((x, top)))
    d.add(elm.Line().down().length(0.5))
    thermostat(d, d.here, 2.2, "F2 safety\nthermostat\n(manual reset)")
    d.add(elm.RBox().down().length(2.5).label("E1 heater\n~1 kW", loc="bottom"))
    thermostat(d, d.here, 2.2, "F3 steam\nthermostat")
    node_a = d.add(elm.Dot()).center
    d.add(elm.Line().down().length(0.5))
    d.add(elm.Switch().down().length(2.5).label("K1 SSR\nload 1–2\n(sheet 2)", loc="bottom"))
    d.add(elm.Line().toy(bottom))
    d.add(elm.Dot())
    d.add(elm.Line().at(node_a).right().length(3.5))
    d.add(elm.Line().down().length(0.5))
    d.add(elm.Switch().down().length(2.5).label("S4\nsteam", loc="bottom"))
    d.add(elm.Line().toy(bottom))
    d.add(elm.Dot())

    # Brew switch S2 (two poles): S2b solenoid valve, S2a pump
    x_valve, x_pump, x_k2, x_water = 23.5, 26.5, 29.0, 31.5
    node_y = -4.0
    d.add(elm.Dot().at((x_valve, top)))
    s2b = d.add(elm.Switch().down().length(3).label("S2b\nbrew", loc="top"))
    d.add(elm.Line().down().length(1.5))
    d.add(elm.Inductor2(loops=3).down().length(2.5).label("Y1 brew\nvalve", loc="bottom"))
    d.add(elm.Line().toy(bottom))
    d.add(elm.Dot())

    d.add(elm.Dot().at((x_pump, top)))
    s2a = d.add(elm.Switch().down().length(3).label("S2a brew", loc="bottom"))
    d.add(elm.Line(ls="--", color=NOTE).at((x_valve - 0.25, s2b.center[1] - 0.35)).tox(x_pump - 0.25))
    d.add(elm.Line().at(s2a.end).toy(node_y))
    pump = d.add(elm.Dot()).center
    d.add(elm.Line().down().length(1.0))
    d.add(elm.Motor().down().length(2.5).label("M1 pump", loc="bottom"))
    d.add(elm.Line().toy(bottom))
    d.add(elm.Dot())

    # K2: pump-detect relay coil, parallel to the pump (contact on sheet 2)
    d.add(elm.Line().at(pump).tox(x_k2))
    d.add(elm.Dot())
    d.add(elm.Line().down().length(1.0))
    d.add(elm.RBox().down().length(2.5).label("K2 pump\ndetect\n(sheet 2)", loc="bottom"))
    d.add(elm.Line().toy(bottom))
    d.add(elm.Dot())

    # S3: hot-water switch, also runs the pump
    d.add(elm.Line().at((rail_end, top)).tox(x_water))
    d.add(elm.Switch().at((x_water, top)).down().length(3).label("S3 hot\nwater", loc="bottom"))
    d.add(elm.Line().toy(node_y))
    d.add(elm.Line().tox(x_k2))

    d.add(elm.Label().at((0, bottom - 3.3)).label(
        "Sheet 1: mains 230 V AC. Brew thermostat replaced by K1 (SSR). Verify against the machine's wiring diagram.",
        halign="left", color=NOTE, fontsize=10))
    return d


def pin(name, side, anchor=None):
    return elm.IcPin(name=name, side=side, anchorname=anchor or name)


def low_voltage() -> schemdraw.Drawing:
    d = drawing()
    spacing = 1.2

    # U2: ADS1115 at 0x48 (pins of a side are numbered from the bottom up)
    ads = d.add(elm.Ic(pins=[pin("AIN3", "left"), pin("AIN2", "left"), pin("AIN1", "left"), pin("AIN0", "left"),
                             pin("ALERT/RDY", "right", "RDY"), pin("SCL", "right"), pin("SDA", "right"),
                             pin("VDD", "top"), pin("GND", "bot"), pin("ADDR", "bot")],
                       size=(4, 6), pinspacing=spacing, edgepadH=0.6, leadlen=0.8, pinfontsize=10,
                       label="U2\nADS1115\n0x48").right().at((12, 0)))
    d.add(elm.Line().at(ads.VDD).up().length(0.4))
    d.add(elm.Vdd().label("+3V3"))
    d.add(elm.Dot().at((ads.VDD[0], ads.VDD[1] + 0.4)))
    d.add(elm.Line().right().length(1.6))
    d.add(elm.Capacitor().down().length(1.4).label("C1\n100 nF", loc="bottom"))
    d.add(elm.Ground())
    d.add(elm.Ground().at(ads.GND))
    d.add(elm.Line().at(ads.ADDR).down().length(0.4))
    d.add(elm.Ground())

    # Wheatstone bridge: Pt1000 half (AIN1) and reference half (AIN0)
    top, bottom = ads.AIN0[1] + 3.0, ads.AIN3[1] - 2.0
    x_pt, x_ref = 2.0, 5.5
    d.add(elm.Line().at((x_pt, top)).tox(x_ref))
    d.add(elm.Dot().at(((x_pt + x_ref) / 2, top)))
    d.add(elm.Vdd().label("V bridge\n5 V (firmware) / 3.3 V (V1 PCB)", fontsize=10))
    for x, upper, lower, net in ((x_pt, "PT1000\nboiler", "R4\n1298 Ω", "AIN1"),
                                 (x_ref, "R1\n1298 Ω", "R2\n1298 Ω", "AIN0")):
        node_y = ads.absanchors[net][1] if hasattr(ads, "absanchors") else getattr(ads, net)[1]
        if upper.startswith("PT"):
            d.add(elm.Thermistor().at((x, top)).down().toy(node_y).label(upper, loc="bottom"))
        else:
            d.add(elm.Resistor().at((x, top)).down().toy(node_y).label(upper, loc="bottom"))
        d.add(elm.Dot())
        d.add(elm.Line().right().tox(getattr(ads, net)[0]))
        d.add(elm.Resistor().at((x, node_y)).down().toy(bottom).label(lower, loc="bottom"))
        d.add(elm.Ground())
    d.add(elm.Line().at(ads.AIN2).left().length(1.2))
    d.add(elm.Dot(open=True).label("SENS3 (spare)", loc="left"))
    d.add(elm.Line().at(ads.AIN3).left().length(1.2))
    d.add(elm.Dot(open=True).label("SENS4 (spare)", loc="left"))

    # U3: SparkFun ESP32 Thing Plus, left pins aligned with the I2C/ALERT pins of U2
    esp = d.add(elm.Ic(pins=[pin("GPIO14", "left", "G14"), pin("GPIO22", "left", "G22"), pin("GPIO23", "left", "G23"),
                             pin("GPIO33", "right", "G33"), pin("GPIO12", "right", "G12"),
                             pin("GPIO27", "right", "G27"), pin("GPIO13", "right", "G13"),
                             pin("GPIO26", "right", "G26"), pin("GPIO17", "right", "G17"),
                             pin("GPIO21", "right", "G21"),
                             pin("VUSB", "top"), pin("3V3", "top", "V33"), pin("GND", "bot")],
                       size=(5.5, 9.6), pinspacing=spacing, edgepadH=0.6, leadlen=0.8, pinfontsize=10,
                       label="U3\nSparkFun\nESP32\nThing Plus").right().anchor("G23").at((ads.SDA[0] + 7, ads.SDA[1])))
    for i, (a, b, ref) in enumerate(((ads.SDA, esp.G23, "R3"), (ads.SCL, esp.G22, "R7"), (ads.RDY, esp.G14, "R6"))):
        d.add(elm.Line().at(a).to(b))
        x = a[0] + 1.5 + 1.6 * i
        d.add(elm.Dot().at((x, a[1])))
        d.add(elm.Resistor().up().toy(ads.VDD[1] + 0.5).label(ref + "\n10 kΩ", loc="bottom"))
        d.add(elm.Vdd().label("+3V3"))
    d.add(elm.Vdd().at(esp.VUSB).label("+5V"))
    d.add(elm.Vdd().at(esp.V33).label("+3V3"))
    d.add(elm.Ground().at(esp.GND))

    # K1: SSR control input
    x_out = esp.G21[0] + 2.5
    d.add(elm.Line().at(esp.G21).tox(x_out + 5.5))
    pd = d.add(elm.Dot().at((x_out, esp.G21[1])))
    d.add(elm.Resistor(color=NOTE, ls="--").at(pd.center).up().length(1.8)
          .label("10 kΩ pull-down\n(proposed)", loc="top", color=NOTE))
    d.add(elm.Line(color=NOTE, ls="--").right().length(0.8))
    d.add(elm.Ground(color=NOTE))
    ssr = d.add(elm.Ic(pins=[pin("3 +", "left", "plus"), pin("4 −", "right", "minus")],
                       size=(2.0, 1.4), pinspacing=1.0, leadlen=0.5, pinfontsize=10).right().anchor("plus").at((x_out + 5.5, esp.G21[1])))
    d.add(elm.Label().at((ssr.center[0], ssr.center[1] - 1.3)).label("K1 SSR input\n(load: sheet 1)"))
    d.add(elm.Line().at(ssr.minus).right().length(0.5))
    d.add(elm.Ground())

    # K2: pump-detect relay contact, internal pull-down on GPIO 17
    d.add(elm.Line().at(esp.G17).tox(x_out + 1.0))
    d.add(elm.Switch().right().length(2.5).label("K2 pump detect\n(coil: sheet 1)", loc="bottom"))
    d.add(elm.Vdd().label("+3V3"))

    # GPIO 26: sensor supply, function on the installed board unknown
    d.add(elm.Line().at(esp.G26).right().length(1.5))
    d.add(elm.Label().label("sensor supply, high (see open points)", loc="right", color=NOTE, fontsize=10))

    # J1 / LED board: RGB status LED, common cathode, active high
    for anchor, name in ((esp.G13, "red"), (esp.G27, "green"), (esp.G12, "blue")):
        d.add(elm.Line().at(anchor).right().length(0.6))
        d.add(elm.Resistor().right().length(2.0).label(f"Rx ({name})", loc="top", fontsize=10))
        d.add(elm.LED().right().length(2.0))
        d.add(elm.Line().right().length(0.4))
        d.add(elm.Ground().right())
    d.add(elm.Line().at(esp.G33).right().length(1.5))
    d.add(elm.Label().label("on-board status LED", loc="right", color=NOTE, fontsize=10))

    d.add(elm.Label().at((0, bottom - 2.5)).label(
        "Sheet 2: low voltage (SELV). GPIO 12 is a strapping pin: must be low at reset.",
        halign="left", color=NOTE, fontsize=10))
    return d


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    mains().save(str(OUT / "mains.svg"))
    low_voltage().save(str(OUT / "low-voltage.svg"))

if __name__ == "__main__":
    main()
