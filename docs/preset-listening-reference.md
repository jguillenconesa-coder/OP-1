# Referencias auditivas de presets

El usuario ha aportado [este vídeo de presets](https://www.youtube.com/watch?v=P84SBtIDdSM)
para orientar las comparaciones del instrumento.

**Estado:** la página y oEmbed de YouTube devuelven HTTP 403 desde este entorno
(26 de octubre de 2026). No se ha escuchado ni descargado el audio, verificado
el título, identificado presets o medido equivalencia sonora con esta referencia.
El enlace queda registrado en [el manifiesto](../research/blackfin/listening-references.json).
No se redistribuye contenido del vídeo.

Después, el usuario adjuntó un MP4 con nombre **Every Single OP-1 Preset (2021)**
(720p/30fps, según el nombre aportado). La herramienta de descarga rechazó el
archivo porque supera **32 MiB**, su límite de transferencia al ejecutor. No hay
una copia local, hash, fotogramas inspeccionados ni audio decodificado. Para poder
relacionar los nombres de pantalla con el sonido se necesitan fragmentos menores
de 32 MiB que conserven la legibilidad de la pantalla. No se han inventariado
presets ni sacado conclusiones sonoras a partir del MP4 inaccesible.

Cuando haya audio accesible, la comparación debe identificar preset y minuto,
nota/octava, velocidad y duración de pulsación, además de FX/LFO/modo de ejecución.
Primero se comparará el carácter y el contorno de amplitud; después, afinación,
espectro, modulación y cola. Una grabación de YouTube puede incluir compresión,
normalización o efectos externos: por sí sola no identifica todos los parámetros
ni demuestra que una diferencia proceda del núcleo DSP.

Las coincidencias numéricas de rutinas aisladas documentadas en este repositorio
siguen siendo evidencia separada de una comparación auditiva del aparato completo.

El usuario indicó después que se ignorasen los primeros lotes segmentados. Esos
lotes permanecen apartados. La validación DSP 0.13 procede del firmware y de
pruebas propias, independientemente del vídeo.

El **27 de octubre de 2026** se recibió y descargó correctamente el lote definitivo
de **21 segmentos**. Se decodificó todo su audio y se revisaron visualmente
**169 selecciones de presets en 11 categorías**, correlacionadas por nombre con
el inventario rev246. Los cortes contienen preroll: se alinearon mediante 1.650
paquetes H264 idénticos en los solapes, en lugar de usar sus nombres como offsets.
El OCR localizó 168 nombres; `basslineslide` requirió inspección adicional.

El [índice por categoría, nombre y tiempo](preset-video-index.md) está disponible
con hashes, método y datos numéricos. Sus tiempos son observaciones de pantalla,
no inicios de notas ni intervalos completos de sonido. La comparación de timbre,
notas/velocidades y calibración del plugin sigue pendiente; no se han ajustado
presets ni DSP a partir de este índice. No se publica el vídeo, audio ni fotogramas.

Después del índice se completó un [primer piloto espectral](preset-audio-pilot.md)
con Sugar y Pure y un pasaje polifónico descartado. Se publican magnitudes
calculadas y un renderizador de estudio, sin material audiovisual original.
Persisten diferencias grandes en algunos armónicos; faltan la cadena completa,
parámetros de la grabación y anotación de notas/pulsaciones. No se aplica una
corrección DSP a partir de esos casos ni se afirma equivalencia de timbre.
