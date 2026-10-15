# mod-prestigio-hollow

Prestigio para AzerothCore 3.3.5a: vuelve del nivel requerido (80 por defecto) al nivel 1, también con Caballero de la Muerte. El módulo no modifica el source del core ni depende de `mod-habilidades-random`.

## Comportamiento

- Comprueba un hueco libre compatible por cada pieza equipada y mueve las piezas originales a las bolsas. Conserva GUID, encantamientos, gemas y durabilidad. No usa correo ni retira las bolsas equipadas.
- Conserva inventario, banco, oro, correo, logros, raza y clase.
- Reinicia misiones activas/completadas, XP, hechizos de combate, talentos de las dos especializaciones y todas sus barras.
- Conserva idiomas. Los idiomas iniciales ausentes se reparan al conectar o hablar con el NPC, sin repetir el prestigio.
- Con `PreservePermanentSpells = 1`, conserva profesiones y recetas, sus puntos actuales/máximo/rango, plazas de profesión, equitación, monturas, compañeros y forja de runas.
- Si otro módulo rechaza la bajada de nivel, cancela sin borrar misiones ni incrementar el contador.

**Al confirmar se vuelve a la selección de personajes mientras se guarda.** Al entrar de nuevo, el personaje aparece al nivel 1 en la zona inicial de su raza. Durante un guardado pendiente no se puede entrar ni borrar ese personaje; se puede reintentar cuando termine. Si falla el reinicio y se confirma su reversión, se restaura el estado original y no se cuenta un prestigio.

## Base propia y guardado atómico

Las tablas propias viven exclusivamente en **`mod_prestigio_hollow`**:

| Tabla | Datos |
| --- | --- |
| `mod_prestigio_hollow_stats` | Contador por GUID. |
| `mod_prestigio_hollow_history` | Número, fecha, nivel anterior, clase, raza e identificador de operación. |
| `mod_prestigio_hollow_operations` | Recibo de cada operación completada o cancelada, para reintentos seguros. |

Esta base debe estar en **la misma instancia MySQL que la base de personajes**, con tablas InnoDB. El nivel, inventario, misiones, hechizos y habilidades siguen guardándose en las tablas nativas mediante `Player::SaveToDB` y las consultas preparadas del core. El NPC utiliza registros de `world`. No se añaden tablas del módulo a `characters`, `world` ni `auth`.

Un trabajador propio conecta usando `CharacterDatabaseInfo`. Así ejecuta el guardado nativo y el contador/historial/recibo en **una sola transacción entre las dos bases**. Si el proceso cae, MySQL confirma todo o revierte todo. El recibo se comprueba bajo bloqueo del contador antes de reintentar, de modo que una respuesta de COMMIT perdida no cuenta dos prestigios. Se comprueba explícitamente el resultado de COMMIT.

Antes de cambiar al personaje se verifican esquema, permisos e InnoDB. También se captura su estado original, incluidos objetos nuevos todavía sin guardar. El personaje se desconecta sin un guardado automático adicional; el trabajador guarda esa copia y el reinicio juntos. Una cancelación confirmada guarda únicamente la copia original, con su recibo de cancelación. El anuncio de éxito se hace después de confirmar el guardado.

La conexión, consultas y reconexiones se ejecutan fuera del hilo del mundo. Se configuran límites de conexión/lectura/escritura de 3 segundos y mantenimiento periódico. Una operación cuyo resultado aún se desconoce sigue pendiente hasta resolverse; no se presenta como completada ni se desbloquea un reingreso que pueda sobrescribirla. El NPC muestra «contador no disponible» ante un fallo, nunca un cero inventado.

La antigua opción **`PrestigioHollow.DatabaseInfo` ya no se lee**: una conexión a otro servidor no permitiría esta transacción atómica. `Enable = 0` no abre la conexión al arrancar; una operación ya aceptada termina de resolverse si se desactiva durante su guardado.

## Instalar o actualizar

1. Detén `worldserver` e instala una sola copia en `modules/mod-prestigio-hollow`.

   ```sh
   git clone https://github.com/milyssd/prestigio.git mod-prestigio-hollow
   ```

   Se admiten también `mod-prestigio`, `prestigio`, `prestigio-main` y `mod-prestigio-hollow-main`, con sus archivos `.cmake` y funciones de entrada correspondientes. No instales copias duplicadas. Al actualizar mediante ZIP sustituye la carpeta completa; retirar únicamente archivos coincidentes puede dejar SQL antiguo.

2. En la instancia MySQL de personajes, ejecuta `data/sql/db-prestigio/base/mod_prestigio_hollow.sql` con una cuenta administradora. Crea la base propia y las tres tablas; puede repetirse. Para una base propia ya existente basta `data/sql/manual/UPGRADE_atomic_prestige.sql`, que añade el identificador y los recibos.

3. Si tus contadores e historial estaban en `characters`, ejecuta `data/sql/manual/MIGRATE_from_characters.sql`. Cambia `@PH_characters_source` si la base no se llama `acore_characters`. Conserva los datos de origen, no duplica números de prestigio ni reduce contadores existentes. Para el formato antiguo `custom_prestige.prestige_stats`, utiliza la importación opcional correspondiente. No necesitas crear `custom_prestige`.

   Las copias antiguas no se eliminan automáticamente. La versión nueva no las utiliza. Retira de la carpeta del módulo el SQL antiguo `data/sql/db-characters/base/mod_prestigio_hollow.sql` si quedó después de una copia manual.

4. Concede al **usuario indicado en `CharacterDatabaseInfo`** permisos `SELECT, INSERT, UPDATE` sobre `mod_prestigio_hollow.*`, además de sus permisos normales sobre personajes. Ejemplo administrativo, ajustando usuario y host:

   ```sql
   GRANT SELECT, INSERT, UPDATE ON mod_prestigio_hollow.* TO 'acore'@'localhost';
   ```

   Esto no cambia contraseñas ni da permisos a otros usuarios. La cuenta del servidor no necesita crear tablas ni ejecutar migraciones.

5. Actualiza el `.conf` real a partir de `conf/mod_prestigio_hollow.conf.dist`. Puedes retirar `PrestigioHollow.DatabaseInfo` y `PrestigioHollow.MailEquippedItems` de configuraciones anteriores. Las credenciales nativas permanecen en la configuración privada de tu servidor; no se suben al repositorio.

6. Si necesitas instalar el NPC, aplica en `world` el SQL base de `data/sql/db-world/base`. Para reparar el NPC **2069426** ya existente sin reemplazar sus apariciones, usa `data/sql/manual/REPAIR_npc_prestigio_hollow.sql`. Para otro entry, cambia `@PH_NPC_ENTRY` en ese archivo.

7. **Regenera CMake**, recompila e instala el `worldserver` nuevo y reinícialo. Incluye los archivos `.cmake` del módulo: enlazan la dependencia MySQL detectada por el core, también en Windows. Copiar SQL o usar `.reload` no actualiza C++.

## NPC e idiomas

El menú incluye contador, opción de prestigio, requisito de nivel, confirmación, volver/cancelar y cerrar. El contador se consulta en segundo plano; una respuesta antigua no reemplaza el menú de confirmación ni reabre el menú después de seleccionar «Cerrar».

Si el NPC no muestra texto ni opciones, ejecuta el diagnóstico de solo lectura `data/sql/manual/DIAGNOSTIC_npc_prestigio_hollow.sql`. La plantilla necesita gossip, `AIName` vacío, `ScriptName = npc_prestigio_hollow` y el menú 2069426. El SQL de reparación corrige también los overrides de las apariciones.

Busca en los logs el registro de `npc_prestigio_hollow`, el estado de configuración y los mensajes de operaciones guardadas o canceladas. Los errores del trabajador del módulo no llaman a `World::StopNow()` ni al manejador fatal de conexión del core.

Para «No conoces ese idioma», instala el módulo actualizado y reconecta o habla con el NPC. Se repara el idioma inicial de raza/clase aunque el contador no esté disponible. Los idiomas adicionales ya perdidos antes de esta corrección no pueden reconstruirse sin datos anteriores.

## Consultas

```sql
SELECT player_guid, prestige_count
FROM mod_prestigio_hollow.mod_prestigio_hollow_stats
ORDER BY prestige_count DESC;

SELECT player_guid, prestige_number, prestiged_at, previous_level, class, race
FROM mod_prestigio_hollow.mod_prestigio_hollow_history
ORDER BY history_id DESC;

SELECT operation_id, player_guid, completed, prestige_count, created_at
FROM mod_prestigio_hollow.mod_prestigio_hollow_operations
ORDER BY created_at DESC;
```

Los datos antiguos conservan sus contadores e historial disponible. No se inventan fechas ni recibos de operaciones anteriores. No borres los recibos de operaciones mientras existan guardados pendientes.

## Verificación

Pruebas locales; `CHECK` sigue evaluándose con `NDEBUG`:

```sh
python3 tests/run_local_tests.py
```

Incluyen asignación de huecos compatibles, cinco nombres de carpeta, menús y barrera de entrada, idiomas, profesiones parciales/máximas, objetos únicos y los métodos de producción del reinicio con un modelo del core. Las pruebas con interfaces simuladas no sustituyen una prueba en juego.

`tests/database_integration.cpp`, enlazado con database/common del core, comprueba contra MySQL desechable la transacción entre bases, consultas preparadas nativas, reintentos idempotentes, cancelación y restauración del original, limpieza de ambas barras, caducidad de conexión, esquema ausente y módulo desactivado. Solo debe ejecutarse con datos de prueba.

Referencia de compilación: AzerothCore `5fa7cb00fa04814c4afe6701f0c6c09e9fb96cea` del 15 de agosto de 2026, en Linux. No se ha ejecutado la solución completa de Windows ni un cliente WoW contra tu servidor. Comprueba en desarrollo el prestigio completo, bolsas, idiomas, profesiones, ambas especializaciones y reconexión.
