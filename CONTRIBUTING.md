# Contribuir a Pocket One

Consulta primero [README](README.md), [créditos](CREDITS.md), [avisos de terceros](THIRD_PARTY_NOTICES.md) y [limitaciones y pruebas](docs/validation.md).

Para incorporarte con tu propia cuenta de Codex, consulta la
[guía de colaboración y el prompt de inicio](docs/collaborating-with-codex.md).

1. Compila para el sistema que vayas a probar. El VST3 de Linux no funciona en Windows/macOS.
2. Ejecuta las suites CTest existentes. En Windows incluye la prueba que carga el módulo VST3.
3. Para cambios de audio, verifica señal, afinación, estabilidad, liberación de notas y recuperación de estado según lo que modifiques.
4. Para cambios de instalación, comprueba instalación/desinstalación en una máquina o prefijo aislado. No borres instalaciones ajenas para pasar las pruebas.
5. Describe el cambio y la validación en tu pull request. No afirmes compatibilidad con hardware/DAW que no hayas probado.

No añadas firmware, muestras de fábrica, gráficos, fuentes o código de terceros sin una licencia compatible y sus atribuciones. El código original del proyecto se publica bajo AGPL-3.0-only. Los componentes externos mantienen sus licencias.

Para informar de un fallo indica versión, sistema operativo/arquitectura, DAW y versión, formato del plugin, frecuencia de muestreo, tamaño de búfer y pasos reproducibles. Evita adjuntar sesiones con muestras privadas o credenciales.
