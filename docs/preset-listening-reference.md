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

El usuario indicó después que se ignorasen los lotes segmentados aportados hasta
el momento y que enviaría un lote definitivo. Esos lotes quedan apartados: no se
publican índices ni comparaciones derivados de ellos. La validación DSP 0.13
procede del firmware y de pruebas propias, independientemente del vídeo.
