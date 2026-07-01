# Teste da Malha de Reed Switches - ESP32-S3

Branch de teste para validar a leitura de uma matriz 8x8 de reed switches com diodos e resistores de pull-down, usada em um projeto de xadrez eletrônico.

## Objetivo

Detectar a casa ocupada no tabuleiro quando o reed switch é fechado por um ímã presente na base da peça de xadrez.

A leitura é feita por varredura de matriz:

```text
coluna em HIGH -> reed fechado -> diodo -> linha com pull-down -> leitura HIGH
```

## Plataforma

- Placa: ESP32-S3
- Framework: ESP-IDF
- Linguagem: C
- Matriz: 8 linhas x 8 colunas

## Mapeamento dos pinos

### Linhas

As linhas são entradas com pull-down.

| Linha | GPIO |
|------:|-----:|
| 1 | GPIO4 |
| 2 | GPIO5 |
| 3 | GPIO6 |
| 4 | GPIO7 |
| 5 | GPIO8 |
| 6 | GPIO9 |
| 7 | GPIO10 |
| 8 | GPIO11 |

### Colunas

As colunas são saídas digitais, ativadas uma por vez durante a varredura.

| Coluna | GPIO |
|:------:|-----:|
| A | GPIO12 |
| B | GPIO13 |
| C | GPIO14 |
| D | GPIO15 |
| E | GPIO16 |
| F | GPIO17 |
| G | GPIO18 |
| H | GPIO21 |

## Circuito esperado

Para cada casa da matriz:

```text
GPIO da coluna -> reed switch -> anodo do diodo -> catodo do diodo -> GPIO da linha
catodo do diodo -> resistor 10k -> GND
```

## Como compilar e gravar

Carregue o ambiente do ESP-IDF:

```bash
. ~/esp/esp-idf/export.sh
```

Entre no projeto:

```bash
cd ~/Downloads/OI/Xadrez
```

Configure o alvo:

```bash
idf.py set-target esp32s3
```

Compile:

```bash
idf.py build
```

Liste a porta serial:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Grave e abra o monitor, ajustando a porta se necessário:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Para sair do monitor:

```text
Ctrl + ]
```

## Testes diretos

Antes de conectar a matriz completa, valide com jumper direto:

```text
GPIO12 com GPIO4  -> A1
GPIO13 com GPIO5  -> B2
GPIO14 com GPIO6  -> C3
GPIO15 com GPIO7  -> D4
GPIO16 com GPIO8  -> E5
GPIO17 com GPIO9  -> F6
GPIO18 com GPIO10 -> G7
GPIO21 com GPIO11 -> H8
```

## Observações

- GPIO19 e GPIO20 foram evitados por uso comum com USB nativo.
- GPIO43 e GPIO44 foram evitados por UART0.
- GPIO0, GPIO3, GPIO45 e GPIO46 foram evitados por serem pinos sensíveis de boot/strapping.
- GPIO26 a GPIO37 foram evitados por possíveis conflitos com flash/PSRAM em alguns módulos ESP32-S3.
