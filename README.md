# mod-prestigio-hollow

Módulo de prestigio para AzerothCore 3.3.5a, con su propia base MySQL `mod_prestigio_hollow` y sin cambios en el código fuente del core. No depende de `mod-habilidades-random`.

- Devuelve al personaje al nivel 1, incluidos los Caballeros de la Muerte, y lo lleva a la zona inicial de su raza.
- Reinicia misiones activas/completadas, talentos, XP, barras y hechizos de combate.
- Conserva inventario, bolsas, banco, oro, correo y logros.
- Comprueba antes de empezar que cada pieza equipada tenga un hueco libre compatible. Guarda las piezas originales en las bolsas, con sus encantamientos y gemas; no utiliza el buzón ni retira las bolsas equipadas. Si no cabe todo, cancela sin modificar el personaje.
- Conserva los idiomas que conoce el personaje, aunque `PreservePermanentSpells` esté desactivado. Repara los idiomas iniciales que falten al conectar o hablar con el NPC.
- Conserva por defecto profesiones/recetas, equitación, monturas, compañeros y forja de runas.
- No cambia raza ni clase.

## Base independiente

Los únicos datos propios del módulo son dos tablas en **`mod_prestigio_hollow`**:

| Tabla | Datos |
| --- | --- |
| `mod_prestigio_hollow_stats` | Contador por GUID de personaje. |
| `mod_prestigio_hollow_history` | Número de prestigio, fecha, nivel anterior, clase y raza. |

El módulo utiliza una conexión separada: `PrestigioHollow.DatabaseInfo`. Solo acepta ese nombre de base; no reutiliza las credenciales del core ni crea tablas durante el arranque. Desactivado con `PrestigioHollow.Enable = 0`, no abre la conexión.

Si la configuración, conexión o tablas fallan, el prestigio queda bloqueado antes de cambiar al personaje. El error se registra sin ordenar el apagado de `worldserver`. Tras corregirlo, recarga la configuración o reinicia el servidor para conectar de nuevo.

El nivel, inventario, idiomas y misiones siguen siendo datos del personaje: AzerothCore los guarda en su base habitual mediante sus APIs y consultas preparadas existentes. El NPC necesita sus registros en `world`. Esto no requiere modificar el source ni añadir tablas propias a `characters`, `world` o `auth`. El contador y el historial se guardan juntos en una transacción de la base propia; el guardado del personaje es una transacción del core separada.

## Instalación o actualización

1. Instala una sola copia en `modules/mod-prestigio-hollow`. Desactiva o retira el módulo combinado antiguo si sigue instalado.

   ```sh
   git clone https://github.com/milyssd/prestigio.git mod-prestigio-hollow
   ```

   También se admiten las carpetas `mod-prestigio`, `prestigio`, `prestigio-main` y `mod-prestigio-hollow-main`. Usa una sola de ellas.

2. Con una cuenta administradora de MySQL, ejecuta **`data/sql/db-prestigio/base/mod_prestigio_hollow.sql`**. Crea la base propia y sus dos tablas. Esta ruta es propia del módulo: importa el archivo manualmente; no presupongas que el actualizador de AzerothCore conoce esa base.

3. Si actualizas una instalación que ya tenía prestigios, detén `worldserver` y ejecuta **`data/sql/manual/MIGRATE_from_characters.sql`** después del SQL base. Cambia `@PH_characters_source` si tu base de personajes no se llama `acore_characters`. Copia los contadores y el historial a la nueva base sin borrar ni alterar el origen. Puede repetirse: no duplica los números del historial ni reduce los contadores existentes.

   Las tablas antiguas de personajes quedan como copia hasta que verifiques los datos. La nueva versión no las usa. El SQL que antes las creaba en `data/sql/db-characters/base` se ha retirado. Si actualizas copiando archivos encima de una carpeta antigua, retira también el archivo obsoleto `data/sql/db-characters/base/mod_prestigio_hollow.sql` de esa carpeta del módulo, para que el actualizador no lo vuelva a importar. Para instalaciones antiguas que usaban `custom_prestige.prestige_stats`, existe la importación opcional `OPTIONAL_migrate_from_custom_prestige.sql`; si ese origen no existe, informa y lo omite. No necesitas crear `custom_prestige`.

4. Crea un usuario de MySQL para el módulo con permisos **SELECT, INSERT y UPDATE solo sobre `mod_prestigio_hollow`**. No necesita acceder a la base de personajes ni crear tablas. La cuenta administradora utilizada para instalar o migrar es diferente de esta cuenta de ejecución.

5. Copia o actualiza `mod_prestigio_hollow.conf` en la carpeta de configuración de módulos que utiliza tu instalación. La plantilla es `conf/mod_prestigio_hollow.conf.dist`. Añade en el `.conf` real:

   ```ini
   PrestigioHollow.Enable = 1
   PrestigioHollow.DatabaseInfo = "127.0.0.1;3306;prestigio_user;TU_CLAVE;mod_prestigio_hollow"
   ```

   Sustituye host, puerto, usuario y clave por los de tu MySQL. No subas las credenciales a GitHub. La plantilla deja `DatabaseInfo` vacío para evitar conectarse con datos inventados.

6. Para instalar el NPC por primera vez, aplica `data/sql/db-world/base/mod_prestigio_hollow.sql` en tu base `world`. Instala la plantilla **2069426**, su texto y dos apariciones. Si el NPC ya existe, utiliza `data/sql/manual/REPAIR_npc_prestigio_hollow.sql`: conserva las apariciones existentes y repara el enlace al script. Para otro entry, cambia `@PH_NPC_ENTRY` en ese archivo.

7. Regenera CMake para incluir también `PrestigioDatabase.cpp`, recompila e instala `worldserver`, y arranca el ejecutable nuevo. En Windows usa la configuración Release que despliegas en tu servidor. Copiar SQL o usar `.reload` no actualiza el código C++.

## Error «No conoces ese idioma» después del prestigio

El reinicio de hechizos puede dejar sin el hechizo o la habilidad de idioma al personaje. Ahora se conservan y restauran ambos al prestigiar, independientemente de la opción de profesiones. Solo se preservan idiomas conocidos; no se enseñan idiomas de otras razas por defecto.

Para un personaje ya afectado, **instala el módulo actualizado y vuelve a conectar con él**, o habla con el NPC. Se reparan los idiomas iniciales de su raza y clase. No hace falta volver a prestigiar ni modificar tablas manualmente. El módulo debe estar activado; esta reparación no depende de que la base propia esté disponible. Los idiomas adicionales que se hubieran perdido antes de instalar esta corrección no pueden deducirse del estado actual del personaje.

## NPC, confirmación y navegación

El menú muestra el contador, **Volver a nivel 1** y **Cerrar**. Por debajo del nivel requerido, la opción indica el requisito. La confirmación incluye **Volver al menú principal (cancelar)**. Si falta espacio o no se cumple una condición, explica el motivo en el chat y vuelve al menú. No permite saltarse el nivel requerido.

La zona inicial se utiliza después de completar el prestigio; el NPC no ofrece un teletransporte libre. Si el prestigio termina pero el core rechaza el viaje, aparece una advertencia y un error en el log: no repitas el prestigio para intentar viajar.

Si no aparece texto ni ninguna opción, ejecuta en `world` el diagnóstico de solo lectura `data/sql/manual/DIAGNOSTIC_npc_prestigio_hollow.sql`. La plantilla necesita `npcflag & 1 = 1`, `AIName` vacío, `ScriptName = npc_prestigio_hollow` y `gossip_menu_id = 2069426`. El SQL de reparación corrige estos campos y los overrides de las apariciones. Comprueba además que el ejecutable haya cargado el módulo; debe registrar:

```text
mod-prestigio-hollow: NPC npc_prestigio_hollow registrado (menu con volver/cancelar).
mod-prestigio-hollow: base independiente mod_prestigio_hollow conectada.
```

Si solo aparece el primer mensaje, revisa los errores de configuración/conexión. Si no aparece ninguno, comprueba qué ejecutable estás arrancando y que hayas instalado el módulo compilado.

## Errores de enlace y equipo inicial

AzerothCore deriva la función de entrada del nombre de la carpeta. Esta versión exporta las cinco variantes admitidas; cada función llama **una sola vez** a `AddPrestigioHollowScripts()`. Si aparece LNK2019/LNK1120 con una función `Add...Scripts`, revisa el nombre de la carpeta, actualiza `src/PrestigioHollowLoader.cpp` y regenera CMake antes de recompilar. No añadas llamadas duplicadas ni código de pruebas dentro del módulo.

La opción antigua `PrestigioHollow.MailEquippedItems` ya no se lee; puedes retirarla del `.conf`. El equipo siempre va a las bolsas.

El objeto `6948` es la piedra de hogar. El error de inventario `17` indica que ya se alcanzó el máximo permitido del objeto. La entrega de equipo inicial ahora cuenta los objetos conservados en inventario y banco para no duplicar objetos únicos. Si falla otro objeto inicial, el jugador recibe un mensaje concreto.

## Consultar el contador y el historial

Estas consultas solo necesitan acceso a la base propia:

```sql
USE mod_prestigio_hollow;
SELECT player_guid, prestige_count
FROM mod_prestigio_hollow_stats
ORDER BY prestige_count DESC, player_guid;

SELECT player_guid, prestige_number, prestiged_at, previous_level, class, race
FROM mod_prestigio_hollow_history
ORDER BY prestiged_at DESC, history_id DESC;
```

Los contadores antiguos se conservan al migrar. Si antes no había historial, no se inventan fechas de prestigios anteriores. Para añadir únicamente la tabla de historial en una base propia ya existente, puedes usar `data/sql/manual/UPGRADE_add_prestige_history.sql`.

## Verificación

Pruebas locales sin un servidor completo:

```sh
g++ -std=c++20 -Wall -Wextra -Werror tests/equipment_storage_test.cpp -o /tmp/prestigio-equipment-test
/tmp/prestigio-equipment-test
g++ -std=c++20 -Wall -Wextra -Werror tests/module_loader_test.cpp src/PrestigioHollowLoader.cpp -o /tmp/prestigio-loader-test
/tmp/prestigio-loader-test
g++ -std=c++20 -Wall -Wextra -Werror -Itests/npc_stubs tests/npc_navigation_test.cpp -o /tmp/prestigio-npc-test
/tmp/prestigio-npc-test
g++ -std=c++20 -Wall -Wextra -Werror tests/languages_test.cpp -o /tmp/prestigio-languages-test
/tmp/prestigio-languages-test
python3 tests/starting_outfit_test.py
```

`tests/database_integration.cpp` prueba el conector real, enlazado con las bibliotecas database/common de AzerothCore. Requiere un MySQL de pruebas desechable; los modos `unavailable`, `normal` y `rollback` comprueban conexión fallida, contador/historial y reversión de una transacción fallida. No lo ejecutes sobre datos reales.

Se ha comprobado la compilación de los cuatro archivos C++ contra AzerothCore del 15 de agosto de 2026, commit `5fa7cb00fa04814c4afe6701f0c6c09e9fb96cea`, en Linux. Las pruebas con MySQL 8 verifican la instalación repetible, migraciones, una cuenta limitada a la base propia y errores de conexión/esquema sin terminar el proceso. No equivalen a compilar tu solución completa de Windows ni a probar en tu servidor en vivo.

En un servidor de desarrollo, comprueba también bolsas llenas/espacio justo/bolsas especiales, idiomas después del prestigio y tras reconectar, y persistencia de misiones, contador e historial.
