# Reconstrucción independiente de Digital rev246

Se aísla la función candidata `0x01a51f7a`, su constructor `0x01a51f28` y el
reinicio `0x01a521e2` del LDR inventariado. La identificación de la ruta parte
de las tablas virtuales y referencias públicas de op1-sdk, contrastadas mediante
llamadas aisladas. No se arranca el sistema original ni se accede a hardware.

El modelo de referencia es GNU GDB 13.1 BF524 con el [parche separado de
writeback D-register](envelope246-research.md). Sus límites siguen vigentes:
no se considera un simulador completo validado contra el hardware ni una
referencia de tiempos del DSP. El plugin no ejecuta instrucciones Blackfin.

## Contrato observado

La llamada recibe estado de 32 bytes, salida de 128 `int16` y cuatro controles
`int16`. Consume 128 incrementos de fase desde `0xff801950`. El seno se obtiene
de la fuente verificada `0x018b6688`, con copia escalar al destino `0x0101e5e8`
para evitar introducir un nuevo hazard de setup. La prueba de rangos existente
confirma mínimos/máximos Digital: `[0,32767]`, `[2048,26624]`, `[-32768,32767]`,
`[0,32767]`. Se usa el mismo contrato de reloj de voz de 88,2 kHz que Phase;
el proveedor completo original de frecuencia sigue sin validarse.

La reconstrucción expresa el comportamiento como realimentación de seno,
oscilador secundario con ratio/fine y sync, máscara XOR y promedios saturados.
Conserva redondeo Q15 a par, multiplicación Q31 con truncado hacia menos infinito,
wrap de 16/32 bits, saturación y comparación firmada de los cruces de fase.
Las dos pasadas originales no comparten dependencia temporal entre sus estados;
la implementación por muestra produce los mismos outputs y estados de bloque.

La prueba también conserva los campos de padding/no modificados. El reinicio
observado no reinicia la fase de la máscara; el constructor sí la inicializa.
Nuestro asignador de voces crea un estado nuevo por nota: no se afirma que
reproduzca el robo, retrigger o la política de asignación de voces original.

## Evidencia y reproducción

`Tests/ReferenceVectors/digital246-input.bin` contiene 2.048 entradas de 556 bytes:
32 de estado, cuatro `int16` de control, 128 `uint32` de incremento y un `uint32`
de acción (0 continuar, 1 cargar estado, 2 reiniciar, 3 construir). Todo little
endian. Las primeras 1.024 cubren estados arbitrarios, bordes, incrementos
positivos/negativos y cruces; las siguientes son 16 secuencias de 64 bloques con
controles almacenados, trayectoria de pitch, reinicio y cambios de parámetros.

La salida contiene 2.048 registros de 288 bytes: 128 `int16` de audio y los 32
bytes de estado completo. El test C++ compara cada valor, no solo hashes ni
estadísticas. Prueba además el adaptador a siete tasas de host y los 16 presets
con ambas envolventes a 44,1/48/96 kHz, pedal y final de release. Se ejecutó también
con AddressSanitizer y UndefinedBehaviorSanitizer.

Para reproducir los oráculos desde un LDR propio con el hash inventariado:

```bash
python3 scripts/verify-digital246.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/digital246-nuevo
```

El directorio de salida debe ser nuevo y estar fuera del repositorio. Allí quedan
imagen, objetos y ELF originales; no se empaquetan. `--record` es una opción de
desarrollo para registrar resultados numéricos después de verificar el hash del
LDR. El comportamiento por defecto compara los dos fixtures comprometidos.
[Huellas, herramientas y conteos](firmware/digital246-validation.json).

El banco conserva nombres y campos numéricos de 16 entradas Digital del DB,
con procedencia en `research/blackfin/digital-controls.json`. No se distribuyen
firmware, opcodes, claves, DB original ni simulador. Implementación, adaptador,
GUI y harness son código independiente del proyecto.

No se han reconstruido los efectos, LFO, ruteo completo de envolvente/velocidad,
mezclador ni filtro/decimador originales. No se promete equivalencia acústica de
presets completos. Punch se revisó como candidato: se identificó su entrada de
proceso, pero todavía no tiene un oráculo/port validado y no se incorpora aquí.
