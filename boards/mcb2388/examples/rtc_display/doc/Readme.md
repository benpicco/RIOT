---
title: RIOT auf dem MCB2388
author: Benjamin Valentin | 896366
---

# Das Betriebssystem RIOT

Bei [RIOT](https://github.com/RIOT-OS/RIOT) handelt es sich um ein freies Betriebssystem für Microcontroller.
Ziel ist es, durch eine dünne Hardwareabstraktionsschicht Code zu schreiben, der auf Hardwareplatformen
verschiedener Hersteller lauffähig ist.
Dadurch ist eine leichte Wiederverwendbarkeit von Softwaremodulen und Treibern gegeben, sofern für eine
Platform die nötigen low-level Treiber implementiert sind.

Seinen Ursprung hat RIOT an der Freien Universität Berlin, heute wird es an einer Vielzahl von Hochschulen
und Forschungseinrichtungen eingesetzt.
Ein besonderes Merkmal ist ein vollständiger IPv6 Stack mit 6LoWPAN Erweiterung, der das effiziente
versenden von IP Paketen in Sensornetzen ermöglicht.

## RIOT und LPC23xx

Seinen Urpsrung hat RIOT als *FireKernel* im *FeuerWhere* Projekt (2008) zur Indoor-Lokalisierung von
Einsatzkräften. Als Hardwareplatform entstand hierbei das *ScatterWeb MSB-A2 Board* als leistungsfähigerer
Nachfolger zum *MSB-430*.
Während letzteres auf den 16-bit MSP430 von Ti setzte, kam beim MSB-A2 ein 32-bit ARM7TDMI-S in Form des
LPC2387 von NXP zum Einsatz.
Als Funkchip setzte man weiterhin auf den per SPI angebundenen CC1100, der bereits auf dem *MSB-430*
zu finden war.

![Das ScatterWeb MSB-A2 Board](msba2.jpg)

Nun muss man sagen, dass seinerzeit der Fokus wohl noch nicht darauf lag, ein universelles Betriebssystem
zu schaffen. Das änderte sich zwar schon bald, als aus *FireKernel* *µkleos* wurde, doch lag der Fokus
weiterhin mehr auf Sensornetzen als in einer möglichst universell einsetzbaren Hardwareabstraktion - was
man für die Ansteuerung des CC1100 Funkchips brauchte war ja bereits vorhanden.

Das änderte sich schließlich mit der Umbenennung in *RIOT* und dem Ziel, auch ein für externe Entwickler
nützliches System zur Verfügung zu stellen.
Doch lag der Fokus der Entwicklung nun auf den moderneren Cortex-M Mikroprozessoren, der `lpc2387` Port
fand nur noch wenig Beachtung und es gab bereits Diskussionen, ihn ganz zu entfernen.

Hier setzt nun mein Projekt an: Mein Ziel war es, den `lpc2387` Port zu einer vollwertigen RIOT-Platform
wiederherzustellen, so dass sie auch in Zukunft noch für verschiedene Projekte genutzt werden kann.

Dies umfasst natürlich auch die Unterstützung für den `lpc2388` wie er auf dem MCB2388 board zu finden ist.

## Eine Bestandsaufnahme

Der `lpc2387` Port befand sich zu Begin des Projekts in einem Zustand, in dem Funktionalität oft nur so
weit implemtiert war, wie sie für den Anwendungsfall des `msba-a2` Boards nötig war.
Entsprechend waren Treiber oft hard-gecoded mit festen Werten für Pins und Datenraten.

 - UART: Nur ein UART mit fixer Baudrate
 - PLL config erwartet 16 MHz Quart (`mcb2388` hat 12 MHz Quartz)
 - SPI: Nur ein SPI mit fixer Frequenz
 - I2C: nicht implementiert
 - ADC: nicht implementiert
 - DAC: nicht implementiert
 - Power Management / Backup RAM: nicht implementiert
 - diverse kleinere Bugs

## Board Support für MCB2388

Die von RIOT unterstützen Boards sind im `boards/` Verzeichnis zu finden.
Der erste Schritt bestand entsprechend darin, ein neues Verzeichniss für das `mcb2388` anzulegen.



![Der Testaufbau mit ATREB215-XPRO und GY-85](Aufbau.jpg)


![Ausgabe von `ifconfig` und `ping6` auf dem MCB2388](ifconfig_ping.png)

## Übersicht über meine Pull-Requests in RIOT

Inzwischen wurden sämtliche meiner Änderungsvorschläge zur Verbesserung der `lpc23xx` Unterstützung
in RIOT gemerged. Das in den kommenden Tagen erscheinende RIOT 2020.04 Release wird mit einer vollwertigen
Unterstützung für das `mcb2388` mit sämtlicher Peripherie erscheinen.

Die Änderungen im Einzelnen:

 - [`#12229: cpu/lpc2387: clean up the platform`](https://github.com/RIOT-OS/RIOT/pull/12229)
 - [`#12580: cpu/lpc2387: enable RTC on rtc_init()`](https://github.com/RIOT-OS/RIOT/pull/12580)
 - [`#12581: cpu/lpc2387: allow for more flexible clock selection`](https://github.com/RIOT-OS/RIOT/pull/12581)
 - [`#12637: cpu/lpc2387: update the UART driver`](https://github.com/RIOT-OS/RIOT/pull/12637)
 - [`#12669: boards: add mcb2388`](https://github.com/RIOT-OS/RIOT/pull/12669)
 - [`#12747: cpu/lpc2387: implement periph/pm`](https://github.com/RIOT-OS/RIOT/pull/12747)
 - [`#12748: cpu/lpc2387: add support for backup RAM`](https://github.com/RIOT-OS/RIOT/pull/12748)
 - [`#12809: cpu/lpc2387: implement periph/dac`](https://github.com/RIOT-OS/RIOT/pull/12809)
 - [`#12830: cpu/lpc2387: clean up lpc2387.ld, fixes tests/cpp_ctors`](https://github.com/RIOT-OS/RIOT/pull/12830)
 - [`#12868: lpc2k_pgm: fix build warnings, add lpc2388`](https://github.com/RIOT-OS/RIOT/pull/12868)
 - [`#12871: cpu/lpc2387: rtc: remove use of localtime() `](https://github.com/RIOT-OS/RIOT/pull/12871)
 - [`#12899: cpu/lpc2387: align lpc2387.ld with cortexm_base.ld, provide thread_isr_stack_*() - enables MicroPython`](https://github.com/RIOT-OS/RIOT/pull/12899)
 - [`#12901: cpu/lpc2387: use the same default stack sizes as cortexm_common`](https://github.com/RIOT-OS/RIOT/pull/12901)
 - [`#12911: cpu/lpc2387: implement periph/adc`](https://github.com/RIOT-OS/RIOT/pull/12911)
 - [`#12928: sys/newlib: enable multiple heaps in _sbrk_r()`](https://github.com/RIOT-OS/RIOT/pull/12928)
 - [`#13037: cpu/lpc2387: implement periph/i2c`](https://github.com/RIOT-OS/RIOT/pull/13037)
 - [`#13246: cpu/lpc2387: make SPI configurable`](https://github.com/RIOT-OS/RIOT/pull/13246)
 - [`#13857: cpu/lpc2387: gpio: Fix interrupts on PORT2`](https://github.com/RIOT-OS/RIOT/pull/13857)
 - [`#13817: cpu/lpc2387: fix check for max number of timers`](https://github.com/RIOT-OS/RIOT/pull/13817)
 - [`#13317: tests/event_threads: remove arch_arm7 from blacklist`](https://github.com/RIOT-OS/RIOT/pull/13317)

Mein besonderer Dank gillt hierbei Marian Buschsieweke, der mir stets mit Kommentaren und Anmerkungen zur
Seite stand und sicherstellte, dass die Änderungen auch auf dem `msba2` weiterhin funktionieren.
