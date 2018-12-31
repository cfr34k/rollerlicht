# Rollerlicht

Animierte Unterbodenbeleuchtung für Scooter.

## Hardware-Aufbau

Das Programm ist für den Arduino Nano (ATmega328p) geschrieben, sollte aber
auch auf jedem anderen AVR-μC funktionieren, sofern dieser die passende
Peripherie zur Verfügung stellt.

Die LED-Leiste besteht aus SK6812-RGBW-LEDs (60 LEDs/m, 27 LEDs auf jeder Seite
des Rollers).

Die Geschwindigkeit des Rollers wird über eine Lichtschranke gemessen, die an
einem Rad montiert ist. Die Lichtschranke besteht aus einer handelsüblichen
IR-LED mit 2,2 kΩ Vorwiderstand, die mit 38 kHz gepulst wird und einem
IR-Empfänger mit 38 kHz-Bandpass, wie er zum Empfang von IR-Fernbedienungen
verwendet wird. Der hohe Vorwiderstand an der IR-LED ist nötig, damit der
Sensor nicht schon aufgrund des Streulichts anspricht.

Der Umfang des Rads und die Anzahl der Impulse pro Umdrehung können in der
_main.c_ festgelegt werden.

Die gesamte Schaltung läuft mit 5V.

### Pin-Konfiguration

Funktion  | μC-Pin     | Arduino-Pin
----------|------------|-------------
IR-LED    | PD3 / OC2B | D3
IR-Sensor | PD8 / ICP1 | D8
SK6812    | PD5 / OC0B | D5

### Verwendete μC-Peripherie

Peripherie | Funktionalität
-----------|-----------------------------------------------------------------------
Timer 0    | Ansteuerung der SK6812-LEDs
Timer 1    | Messung der Raddrehzahl, allgemeine Zeitmessung (Frameintervall u.ä.)
Timer 2    | Erzeugung von 38 kHz-PWM für die IR-LED
