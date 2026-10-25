# Descargas versionadas

Los archivos de [`v0.2.0`](v0.2.0) son los paquetes construidos y verificados durante la preparación del proyecto. Se incluyen aquí para que estén disponibles desde el repositorio aun cuando la API o el trabajo de GitHub Releases no estén disponibles.

- Instalador Windows x64, ZIP portable, paquete Linux y fuentes.
- `SHA256SUMS.txt` comprueba integridad: `sha256sum --check SHA256SUMS.txt` dentro de la carpeta de versión. En PowerShell puedes usar `Get-FileHash -Algorithm SHA256`.
- La etiqueta `v0.2.0` activa un trabajo que comprueba los hashes y adjunta estos mismos archivos a una release preliminar.

Los paquetes Windows se validaron bajo Wine, no en un DAW Windows real. Son binarios sin firma digital. No se distribuye un binario macOS sin verificar. Véanse [las notas de versión](../docs/releases/v0.2.0.md), [la licencia](../LICENSE) y [los avisos de terceros](../THIRD_PARTY_NOTICES.md).
