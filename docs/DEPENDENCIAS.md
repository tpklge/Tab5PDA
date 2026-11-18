# Dependências — Tab5PDA 1.0.0

| Componente | Versão |
| --- | --- |
| PlatformIO Core | 6.2.0 |
| pioarduino espressif32 | 55.03.311, release fixada |
| Arduino-ESP32 | 3.3.11, fornecido pela plataforma |
| Placa | m5stack-tab5-p4, ESP32-P4 ES anterior à revisão 300 |
| M5Unified | 0.2.25 |
| M5GFX | 0.2.32 |
| LVGL | 8.3.11 |
| PDACommon | 1.0.0, incorporado em `lib/PDACommon` |
| Pillow (regenerar fonte/capturas) | 12.3.0 |
| pyserial (utilitário USB) | 3.5 |

A configuração não usa `master`, `latest` nem o ZIP `stable` mutável. A plataforma
fixada inclui a variante de placa, os pinos SDIO do C6 e suporte ao P4.
[Definição da placa nesta versão](https://github.com/pioarduino/platform-espressif32/blob/55.03.311/boards/m5stack-tab5-p4.json).

A identificação de painel é do M5GFX. A implementação fixada reconhece o firmware
do touch ST para distinguir ST7121/ST7123, com perfis e timings separados,
além do ILI9881C/GT911. M5Unified/M5GFX também corrigem o reset pelo expansor
I2C nas revisões ST. [Orientação oficial](https://docs.m5stack.com/en/core/Tab5).

A heap LVGL e os buffers são da PSRAM. O processo de teste usa uma heap LVGL
local e o mesmo código de widgets/fonte, sem drivers do hardware. A fonte
Montserrat é distribuída sob [SIL OFL](../assets/fonts/OFL.txt).

Criptografia do PIN: PBKDF2-HMAC-SHA256 do mbedTLS do SDK, 10000 iterações,
salt aleatório de 16 bytes representado em hexadecimal e hash de 32 bytes;
mesmo algoritmo/formato de preferências do CardputerPDA 1.0.0.

O SDK fixado pode produzir avisos de dependências (operação entre enums nos
headers LVGL sob o padrão C++ do SDK e qualifier volatile no HAL SPI do Arduino).
Não são avisos do código de aplicação; registrar sua ocorrência no build, sem
alterar a instalação global ou os fontes dos fornecedores para escondê-los.
