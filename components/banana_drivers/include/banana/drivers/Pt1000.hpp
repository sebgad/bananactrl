#pragma once

#include <array>

#include "banana/drivers/TemperatureConverter.hpp"

/// Pt1000 in a Wheatstone bridge with 3 x 1298 Ohm, bridge voltage -> temperature.
/// Generated from the Arduino Pt1000.h (tools/calc_wheat_stone.py, DIN table tools/PT_1000_tabelle.csv).
namespace banana::drivers::pt1000 {

/// Lookup table for a 5.0 V bridge supply (the hardware in the machine).
inline constexpr std::array<LookupTableConverter::Point, 52> kLookup5V0{{
    {.volts = -0.32419495213228894F, .celsius = 0.0F},  {.volts = -0.30039524838944920F, .celsius = 5.0F},
    {.volts = -0.27702896411257033F, .celsius = 10.0F}, {.volts = -0.25409613450513263F, .celsius = 15.0F},
    {.volts = -0.23155046002845181F, .celsius = 20.0F}, {.volts = -0.20941616047759215F, .celsius = 25.0F},
    {.volts = -0.18767108538014599F, .celsius = 30.0F}, {.volts = -0.16630513376717293F, .celsius = 35.0F},
    {.volts = -0.14529776922732021F, .celsius = 40.0F}, {.volts = -0.12466130141141257F, .celsius = 45.0F},
    {.volts = -0.10436522109014147F, .celsius = 50.0F}, {.volts = -0.08441149883920494F, .celsius = 55.0F},
    {.volts = -0.06479161562112207F, .celsius = 60.0F}, {.volts = -0.04549733291496719F, .celsius = 65.0F},
    {.volts = -0.02652068126520681F, .celsius = 70.0F}, {.volts = -0.00785394938694773F, .celsius = 75.0F},
    {.volts = -0.00414785661920740F, .celsius = 76.0F}, {.volts = -0.00046233510048086F, .celsius = 77.0F},
    {.volts = 0.00320274840054926F, .celsius = 78.0F},  {.volts = 0.00686668075216578F, .celsius = 79.0F},
    {.volts = 0.01051987556435253F, .celsius = 80.0F},  {.volts = 0.01415285853269900F, .celsius = 81.0F},
    {.volts = 0.01778474718886240F, .celsius = 82.0F},  {.volts = 0.02139657272925185F, .celsius = 83.0F},
    {.volts = 0.02499790254059541F, .celsius = 84.0F},  {.volts = 0.02858878230637118F, .celsius = 85.0F},
    {.volts = 0.03216925744531974F, .celsius = 86.0F},  {.volts = 0.03573937311335992F, .celsius = 87.0F},
    {.volts = 0.03929917420548563F, .celsius = 88.0F},  {.volts = 0.04283940240642722F, .celsius = 89.0F},
    {.volts = 0.04637873477828555F, .celsius = 90.0F},  {.volts = 0.04989863602215269F, .celsius = 91.0F},
    {.volts = 0.05341767570909428F, .celsius = 92.0F},  {.volts = 0.05691742456182620F, .celsius = 93.0F},
    {.volts = 0.06040717518456701F, .celsius = 94.0F},  {.volts = 0.06388697036125031F, .celsius = 95.0F},
    {.volts = 0.06734773433488123F, .celsius = 96.0F},  {.volts = 0.07080777191793908F, .celsius = 97.0F},
    {.volts = 0.07425798084074950F, .celsius = 98.0F},  {.volts = 0.07768936198801159F, .celsius = 99.0F},
    {.volts = 0.08112006440407586F, .celsius = 100.0F}, {.volts = 0.08453207287343903F, .celsius = 101.0F},
    {.volts = 0.08793446912258808F, .celsius = 102.0F}, {.volts = 0.09132729371332721F, .celsius = 103.0F},
    {.volts = 0.09471058697956433F, .celsius = 104.0F}, {.volts = 0.09807549962990378F, .celsius = 105.0F},
    {.volts = 0.11478612092189075F, .celsius = 110.0F}, {.volts = 0.14742558035002254F, .celsius = 120.0F},
    {.volts = 0.17909252159981684F, .celsius = 130.0F}, {.volts = 0.20982130254354503F, .celsius = 140.0F},
    {.volts = 0.23966042664344797F, .celsius = 150.0F}, {.volts = 0.26863993618791554F, .celsius = 160.0F},
}};

/// Arduino LOOKUP_TABLE_3_3. Despite the name, the values follow the bridge model at 5.08 V
/// (constant ratio at every point), not 3.3 V.
inline constexpr std::array<LookupTableConverter::Point, 75> kLookup3V3{{
    {.volts = -0.32938207136640557F, .celsius = 0.0F},  {.volts = -0.30520157236368040F, .celsius = 5.0F},
    {.volts = -0.28146142753837144F, .celsius = 10.0F}, {.volts = -0.25816167265721479F, .celsius = 15.0F},
    {.volts = -0.23525526738890704F, .celsius = 20.0F}, {.volts = -0.19067382274622832F, .celsius = 30.0F},
    {.volts = -0.16896601590744770F, .celsius = 35.0F}, {.volts = -0.14762253353495733F, .celsius = 40.0F},
    {.volts = -0.12665588223399515F, .celsius = 45.0F}, {.volts = -0.10603506462758373F, .celsius = 50.0F},
    {.volts = -0.10195334599450293F, .celsius = 51.0F}, {.volts = -0.09788420071449419F, .celsius = 52.0F},
    {.volts = -0.09382757078034132F, .celsius = 53.0F}, {.volts = -0.08979388679726900F, .celsius = 54.0F},
    {.volts = -0.08576208282063223F, .celsius = 55.0F}, {.volts = -0.08175304665911221F, .celsius = 56.0F},
    {.volts = -0.07774584143872311F, .celsius = 57.0F}, {.volts = -0.07376122787128281F, .celsius = 58.0F},
    {.volts = -0.06978872626237852F, .celsius = 59.0F}, {.volts = -0.06582828147106003F, .celsius = 60.0F},
    {.volts = -0.06187983869058424F, .celsius = 61.0F}, {.volts = -0.05795357926629880F, .celsius = 62.0F},
    {.volts = -0.05402894650914845F, .celsius = 63.0F}, {.volts = -0.05011615346241022F, .celsius = 64.0F},
    {.volts = -0.04622529024160667F, .celsius = 65.0F}, {.volts = -0.04234609916856923F, .celsius = 66.0F},
    {.volts = -0.03847852779928518F, .celsius = 67.0F}, {.volts = -0.03462252400345178F, .celsius = 68.0F},
    {.volts = -0.03077803596213515F, .celsius = 69.0F}, {.volts = -0.02694501216545012F, .celsius = 70.0F},
    {.volts = -0.02312340141025752F, .celsius = 71.0F}, {.volts = -0.01932308647725494F, .celsius = 72.0F},
    {.volts = -0.01552411994279577F, .celsius = 73.0F}, {.volts = -0.01174628973897571F, .celsius = 74.0F},
    {.volts = -0.00797961257713890F, .celsius = 75.0F}, {.volts = -0.00421422232511472F, .celsius = 76.0F},
    {.volts = -0.00046973246208855F, .celsius = 77.0F}, {.volts = 0.00325399237495805F, .celsius = 78.0F},
    {.volts = 0.00697654764420043F, .celsius = 79.0F},  {.volts = 0.01068819357338217F, .celsius = 80.0F},
    {.volts = 0.01437930426922219F, .celsius = 81.0F},  {.volts = 0.01806930314388420F, .celsius = 82.0F},
    {.volts = 0.02173891789291988F, .celsius = 83.0F},  {.volts = 0.02539786898124494F, .celsius = 84.0F},
    {.volts = 0.02904620282327312F, .celsius = 85.0F},  {.volts = 0.03268396556444485F, .celsius = 86.0F},
    {.volts = 0.03631120308317368F, .celsius = 87.0F},  {.volts = 0.03992796099277341F, .celsius = 88.0F},
    {.volts = 0.04352483284493006F, .celsius = 89.0F},  {.volts = 0.04712079453473812F, .celsius = 90.0F},
    {.volts = 0.05069701419850713F, .celsius = 91.0F},  {.volts = 0.05427235852043980F, .celsius = 92.0F},
    {.volts = 0.05782810335481543F, .celsius = 93.0F},  {.volts = 0.06137368998752008F, .celsius = 94.0F},
    {.volts = 0.06490916188703032F, .celsius = 95.0F},  {.volts = 0.06842529808423932F, .celsius = 96.0F},
    {.volts = 0.07194069626862609F, .celsius = 97.0F},  {.volts = 0.07544610853420150F, .celsius = 98.0F},
    {.volts = 0.07893239177981978F, .celsius = 99.0F},  {.volts = 0.08241798543454107F, .celsius = 100.0F},
    {.volts = 0.11662269885664100F, .celsius = 110.0F}, {.volts = 0.14978438963562291F, .celsius = 120.0F},
    {.volts = 0.18195800194541389F, .celsius = 130.0F}, {.volts = 0.21317844338424174F, .celsius = 140.0F},
    {.volts = 0.24349499346974313F, .celsius = 150.0F}, {.volts = 0.24648245733028643F, .celsius = 151.0F},
    {.volts = 0.24945420186749667F, .celsius = 152.0F}, {.volts = 0.25241825530364803F, .celsius = 153.0F},
    {.volts = 0.25537464745788557F, .celsius = 154.0F}, {.volts = 0.25832340799540482F, .celsius = 155.0F},
    {.volts = 0.26126456642844320F, .celsius = 156.0F}, {.volts = 0.26419815211726483F, .celsius = 157.0F},
    {.volts = 0.26711635971307324F, .celsius = 158.0F}, {.volts = 0.27003490749857129F, .celsius = 159.0F},
    {.volts = 0.27293817516692220F, .celsius = 160.0F},
}};

/// Regression fits over 0..160 °C for a 3.3 V bridge supply. Worst case errors there: linear 10.8 K,
/// quadratic 1.03 K. On the 5 V hardware both are about 50 K off.
inline constexpr LinearConverter kLinear3V3{433.6520668F, 82.01975366F};
inline constexpr QuadraticConverter kQuadratic3V3{295.16714369F, 417.84868016F, 76.91861221F};

} // namespace banana::drivers::pt1000
