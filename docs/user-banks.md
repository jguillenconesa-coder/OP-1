# Bancos de muestras del usuario

En **0.3.0**, pulsa **BROWSE → IMPORT BANK** y elige un ZIP Pocket One. En 0.2.2 el botón se llama BANK. Espera a que termine la importación con el transporte parado; los presets aparecen en la biblioteca bajo el nombre del banco. Puedes buscar, filtrar y separar variantes de muestras únicas. La biblioteca permanece en el equipo al cerrar y reabrir el plugin.

- Windows: `%APPDATA%\Pocket Instruments\Pocket One\Banks`.
- Las sesiones y proyectos del DAW incorporan la muestra o las ocho muestras del kit activo, junto con `sampleRoot`. Se pueden abrir sin instalar de nuevo el banco.
- Cargar un sonido mantiene el volumen de salida, el tempo, el patrón y el audio grabado en cinta.
- Las muestras estéreo se convierten a mono en este motor. La envolvente, el delay y el LFO son los de Pocket One; no se importan los algoritmos de efectos del OP-1.
- Un AIFF de sampler se reproduce como audio. No implica soporte para los presets de síntesis ni los slices de batería del OP-1.

## Crear un banco desde tus archivos

```sh
python3 scripts/build-user-bank.py --input /ruta/a/tus/muestras --output Mi-banco.zip
```

El generador lee WAV y AIFF/AIFF-C PCM, detecta duplicados por SHA256, conserva los nombres y divide WAVs largos en partes de hasta 16 segundos. Usa `base_freq` del APPL del OP-1 cuando existe, o la nota indicada en el nombre (por ejemplo, `A2`, `G3`, `F3`, `C4`). Sin esa información utiliza MIDI 60; no efectúa detección automática de afinación.

Produce un preset Dry por archivo/parte y un preset Space adicional por AIFF (delay y envolvente propios). El APPL original se conserva en el manifiesto como procedencia; sus valores no se presentan como una conversión exacta. También puede montar dos kits de ocho sonidos cuando se encuentran las muestras de campo correspondientes.

El ZIP contiene `bank.json` (versión 1), `samples/` y notas de uso. El importador limita tamaños, comprueba rutas y decodifica las muestras antes de mostrar los sonidos. Un banco inválido no debe dañar los bancos ya instalados. No se aceptan ejecutables ni se ejecuta código del ZIP.

## Colección aportada en esta sesión

Se han preparado **79 presets**, incluidos **dos kits**, usando **49 audios únicos** (25 AIFF y 24 WAV). Se eliminaron 24 duplicados. SOMMARNATT, AKTERSNURRAN y TYPISK MIDSOMMAR se dividieron para conservar todas sus partes: el banco contiene 52 archivos de audio.

Los AIFF declaran `type: sampler`, `synth_version: 3` y audio estéreo; son instrumentos muestreados, no patches numéricos para los motores FM/PHASE/STRING. Los nombres y metadatos completos están en el manifiesto del banco entregado. DRAGSPEL A2 se asigna a MIDI 45; DRAGSPEL G3 a 55; SKOGSFIOL F3 a 53 y C4 a 60.

El banco se ofrece ahora como [descarga directa del repositorio](../downloads/user-banks/README.md), después de que el enlace local del chat no permitiera descargarlo. No se etiqueta CC0: sus autores y condiciones de redistribución siguen pendientes de identificación. La infraestructura y los presets de parámetros propios sí forman parte del código público; el banco de fábrica original del proyecto sigue disponible con sus licencias.

En 0.3.0, [buttons-and-browser.md](buttons-and-browser.md) explica el navegador, favoritos, slots y el botón ADD SAMPLES.
