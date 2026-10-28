# Colaborar en Tiny-one con Codex

El repositorio es `jguillenconesa-coder/Tiny-one` y permanece privado. La invitación
de GitHub y el acceso de Codex son dos pasos distintos. Cada colaborador utiliza
su propia cuenta y su propio entorno; el historial de este chat y los archivos
privados de investigación no se transfieren con el repositorio.

## Acceso inicial

1. El propietario abre [Settings → Collaborators](https://github.com/jguillenconesa-coder/Tiny-one/settings/access),
   pulsa **Add people** y busca el usuario de GitHub o correo del colaborador.
   Comprueba la identidad del destinatario y envía la invitación. El rol de
   colaborador permite aportar código; no hace falta conceder administración.
2. El colaborador acepta la invitación con su cuenta de GitHub y comprueba que
   puede abrir el [repositorio privado](https://github.com/jguillenconesa-coder/Tiny-one).
3. En Codex conecta esa misma cuenta de GitHub y comprueba que puede seleccionar
   Tiny-one. Si no aparece, revisa la autorización de la aplicación GitHub; como
   alternativa, abre un clon local con Codex.

La integración disponible en la sesión que preparó esta guía pudo consultar el
repositorio y sus colaboradores, pero recibió `403 Resource not accessible by
integration` al consultar invitaciones. Esta guía no confirma que se haya enviado
o aceptado una invitación.

## Trabajar desde su computadora

Instala Git, CMake 3.22 o posterior y un compilador C++17. En Windows utiliza
Visual Studio 2022 o sus Build Tools con las herramientas C++. Para las pruebas
Python existentes, instala también Python 3.

Autentica Git con la cuenta invitada, clona y abre la carpeta en Codex:

```sh
git clone https://github.com/jguillenconesa-coder/Tiny-one.git
cd Tiny-one
git switch -c collab/drwave-validation
```

Primero se pueden comprobar los núcleos sin compilar JUCE ni el plugin:

```sh
cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --config Release --parallel 2
ctest --test-dir build-dsp -C Release --output-on-failure
```

En Windows, si se utiliza el generador de Visual Studio, añade `-A x64` a la
configuración inicial. Para compilar el VST3 y la aplicación, sigue el
[README](../README.md#compilar-y-probar). Los scripts de
[preparación en la nube](cloud.md) están hechos para el entorno Linux con checkout
en `/workspace/OP-1`; no deben ejecutarse tal cual en una computadora Windows.

## Coordinación

- Trabaja en una rama por tarea y abre un pull request contra `main`, con el
  objetivo, archivos modificados, pruebas y límites de lo comprobado.
- Consulta antes el plan y los pull requests abiertos para no duplicar cambios.
  Acuerda en el PR el alcance y los archivos si otro colaborador los está tocando.
- Mantén un único componente DSP activo según el plan. La validación independiente
  del mismo componente permite colaborar sin abrir otro frente de síntesis.
- Evita pushes directos a `main`, merges y releases automáticos. Este es el flujo
  de colaboración acordado por la guía; no implica que se hayan configurado reglas
  de protección en GitHub.
- El firmware y los artefactos privados no están disponibles por clonar el código.
  Los fixtures versionados permiten ejecutar regresiones sin ellos. Recrear los
  oráculos originales exige obtener por separado los archivos y herramientas
  indicados en su documentación, fuera del control de versiones.

## Prompt para pegar en su Codex

```text
Colabora conmigo en el repositorio privado
https://github.com/jguillenconesa-coder/Tiny-one.
Usa mi propia cuenta de GitHub. Si aún no tienes un checkout, clona el repositorio
con mis credenciales configuradas; si ya existe, respeta los cambios locales.

Antes de modificar código, lee AGENTS.md si existe, CONTRIBUTING.md,
docs/collaborating-with-codex.md, docs/firmware-dsp-workplan.md y
docs/original-op1-coverage.md. Para el bloque activo lee también
docs/drwave246-kernel.md y docs/drwave246-counter.md.
El estado versionado más reciente prevalece sobre este prompt.

El objetivo actual es reconstruir los motores DSP, filtros, envolventes y
moduladores del OP-1 rev246 a partir de evidencia verificable. Presets, bancos
y cambios visuales están aplazados. Dr. Wave es el bloque activo; comprueba el
plan actualizado antes de elegir una tarea. No supongas resueltas la cadencia
externa, asignación, afinación/pitch, ADSR o salida. Una coincidencia de un núcleo
aislado no demuestra que el plugin replique ya el instrumento completo.

Empieza reproduciendo las pruebas de núcleo con POCKET_BUILD_PLUGIN=OFF y
revisando de forma independiente el contrato y la cobertura de Dr. Wave,
especialmente su contador de 16 bits. Comprueba los PR abiertos para evitar
duplicar trabajo. Documenta cualquier discrepancia con un caso reproducible;
corrige solo fallos demostrados dentro de un alcance acotado.

Trabaja en una rama collab/<tarea> y prepara un pull request contra main.
Conserva IDs de automatización/MIDI, sesiones y nombres claros de controles.
No introduzcas asignaciones dinámicas, bloqueos ni operaciones de disco en el
callback de audio. Ejecuta las pruebas apropiadas al cambio y comunica resultados
y limitaciones; no afirmes pruebas físicas de Ableton/ASIO si no las has hecho.

Usa los fixtures versionados para las regresiones. No supongas disponibles los
archivos privados de otra sesión. Si una investigación necesita firmware o
herramientas ausentes, indica la dependencia concreta y continúa las comprobaciones
que sí sean posibles. Puedes apoyarte en REA cuando aporte evidencia útil; distingue
siempre observaciones, hipótesis y límites de sus proveedores.
No añadas firmware ni artefactos privados al repositorio.

Entrega un cambio revisable o un informe de validación con sus evidencias.
Puedes publicar tu rama y abrir un PR; no hagas push directo a main, merge ni
release sin que te lo pidamos explícitamente.
```

El prompt orienta el trabajo; no concede acceso al repositorio ni instala o
autentica Codex. La invitación y la conexión de cuentas deben estar completadas.
