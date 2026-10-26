# Banco de referencia del firmware OP-1 246

## Nuevo: estudios Phase

[Pocket-One-Phase-246-Core-Studies.zip](Pocket-One-Phase-246-Core-Studies.zip)
añade **16 presets / 13 WAV únicos** generados con la reconstrucción experimental
del núcleo Pd. Funciona con **Pocket One 0.7.0 → BROWSE → IMPORT BANK** y conserva
los bancos anteriores. Son renders muestreados de estudio; faltan la ruta de
controles, envolventes, efectos y LFO originales. [Contenido, pruebas y límites](../../docs/phase-core-studies.md)
· [SHA256](Phase-Core-Studies-SHA256SUMS.txt).

## Fuentes de audio del firmware

Contenido destinado al repositorio privado Tiny-one y al trabajo de referencia solicitado por su propietario. Los derechos del audio original corresponden a sus titulares; no se etiqueta como CC0 ni como AGPL.

**Pocket-One-OP1-246-Reference.zip** contiene nueve instrumentos sampler y 14 kits de 24 pads (336 regiones). Es un banco de fuentes secas: no convierte los 160 presets de síntesis ni replica los efectos, envolventes, LFO, pitch/volumen por pad o modos del firmware.

Importación: Pocket One **0.6.0 → BROWSE → IMPORT BANK**, elige el ZIP. El modo **CLEAN SAMPLE** queda activado por estos presets para evitar que el filtro global y la saturación seca del instrumento alteren involuntariamente la fuente. La versión 0.5.0 puede importar el banco pero no aplica ese nuevo parámetro.

El perfil RAW se interpreta como PCM16 little endian mono a 44,1 kHz. La escala Q31/12 s con END inclusivo da el final exacto de los 14 archivos (última muestra); se conserva como una interpretación contrastada de los datos, no como una emulación completa. Las frecuencias raíz proceden de `base_freq` de la base de presets.

El generador local es `scripts/build-firmware-bank.py`; requiere una copia de firmware suministrada por el operador. No descarga ni publica nada. Las pruebas sintéticas comprueban posiciones, frecuencia raíz y conservación de bytes PCM al crear WAVs. La calidad sonora del DSP original sigue pendiente de validación con la máquina.
