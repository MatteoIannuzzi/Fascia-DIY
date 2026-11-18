# Fascia DIY

Prototipo software di una fascia fitness fai-da-te, tipo smartwatch, scritto in C.

Il programma legge le misure grezze dei sensori e calcola **passi, battito cardiaco, ossigeno nel sangue (SpO2), calorie, stato di attività e batteria**. Per ora i dati sono simulati: quando ci sarà l'hardware, basterà sostituire la lettura da file con la lettura dai sensori veri, e i calcoli resteranno uguali.

## Esempio di output

```
  Tempo | Stato        | Battito | Ossigeno | Passi | Calorie    | Batteria
--------+--------------+---------+----------+-------+------------+---------
    5 s | fermo        |  71 bpm |  96.9 %  |     0 |  0.14 kcal | 94%
   60 s | fermo        |  72 bpm |  96.8 %  |     0 |  2.18 kcal | 93%
   65 s | corsa        |  80 bpm |  97.0 %  |    14 |  2.39 kcal | 93%
  120 s | corsa        | 150 bpm |  97.1 %  |   168 | 12.87 kcal | 92%

Righe lette: 6000
```

## File

| File | Cosa fa |
|---|---|
| `Prototipo.c` | Il programma della fascia: legge i dati e fa i calcoli |
| `genera_dati.c` | Crea dati simulati: 60 s da fermo e 60 s di corsa, 50 letture al secondo |

## Come usarlo

Serve un compilatore C (`gcc` o `clang`). Sul Mac si installa con `xcode-select --install`.

```bash
# 1. Compila i due programmi
gcc -Wall genera_dati.c -o genera -lm
gcc -Wall Prototipo.c -o prot -lm

# 2. Crea il file con i dati simulati
./genera > dati.csv

# 3. Avvia la fascia sui dati
./prot < dati.csv
```

Prima di iniziare, in `Prototipo.c` imposta `PESO_KG` e `ETA_ANNI` con i tuoi dati: servono per le calorie.

## Formato dei dati

Una riga per ogni lettura, 10 valori separati da virgole, senza intestazione:

```
tempo_ms,rosso,ir,acc_x,acc_y,acc_z,giro_x,giro_y,giro_z,batteria_mv
0,30013,50013,114,-46,16207,-4,2,7,4150
```

| Campo | Sensore | Valori |
|---|---|---|
| `tempo_ms` | — | istante della lettura in millisecondi |
| `rosso`, `ir` | MAX30102 (ottico) | da 0 a 262143 |
| `acc_x`, `acc_y`, `acc_z` | accelerometro | da -32768 a 32767 (16384 = 1 g) |
| `giro_x`, `giro_y`, `giro_z` | giroscopio | da -32768 a 32767 |
| `batteria_mv` | batteria LiPo | millivolt (4200 = carica, 3300 = scarica) |

## Come funzionano i calcoli

- **Passi**: un passo è una cresta della forza del movimento. Si conta quando la forza supera 1,3 g, e il passo successivo si può contare solo dopo che è riscesa sotto 1,1 g e sono passati almeno 0,25 secondi.
- **Battito**: dalla luce infrarossa si toglie la parte fissa (pelle, ossa) e resta l'onda del sangue. Si contano le creste e dal tempo tra due creste si ricavano i battiti al minuto.
- **Ossigeno**: si confronta quanto oscilla la luce rossa rispetto a quella infrarossa in blocchi di 2 secondi, e una formula empirica trasforma il rapporto in percentuale.
- **Calorie**: formula di Keytel (2005), che le stima da battito, peso ed età.
- **Stato**: in base ai passi degli ultimi 5 secondi (0 = fermo, fino a 10 = cammino, di più = corsa).
- **Fascia indossata**: se la luce infrarossa riflessa supera una soglia, c'è la pelle davanti al sensore.
- **Batteria**: proporzione tra 3300 mV (0%) e 4200 mV (100%).

## Hardware previsto

- Microcontrollore Seeed XIAO nRF52840 Sense (accelerometro e giroscopio integrati)
- Sensore ottico MAX30102 (battito e ossigeno)
- Batteria LiPo 150–300 mAh
- Display OLED 0,96" (opzionale)

## Limiti

- I valori di battito e ossigeno di un dispositivo fai-da-te sono **indicativi**: non è un dispositivo medico e non va usato per decisioni sulla salute.
- Con un sensore vero al polso, soprattutto in movimento, il segnale è molto più disturbato di quello simulato e gli algoritmi andranno migliorati.
- La formula delle calorie è quella per uomini ed è pensata per l'attività fisica: da fermo sovrastima un po'.

## Prossimi passi

- [ ] Conversione del giroscopio in gradi al secondo
- [ ] Dividere il codice in più file (sensori, calcoli, main)
- [ ] Simulare il disturbo del movimento sul sensore ottico e filtrarlo
- [ ] Passare all'hardware vero
