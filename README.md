# mod-prestigio-hollow

Prestigio independiente para AzerothCore 3.3.5a.

- Nivel 80 -> nivel 1, incluidos Caballeros de la Muerte.
- Borra misiones activas y completadas para permitir un nuevo recorrido 1-80.
- Conserva inventario, bolsas, banco, oro, correo y logros.
- Antes de bajar de nivel, comprueba que haya un hueco libre compatible por pieza equipada y guarda el equipo original en tus bolsas. Si no cabe todo, cancela sin cambiar nada. No utiliza el buzón ni retira las bolsas equipadas.
- Conserva por defecto profesiones/recetas, equitación, monturas, compañeros y forja de runas.
- Reinicia hechizos normales de combate, talentos, XP y barras de acción.
- No cambia raza ni clase.
- No depende de mod-habilidades-random.

Instalación: copiar la carpeta a `modules/`, reconfigurar CMake/recompilar y aplicar los SQL de `data/sql/db-characters/base` y `data/sql/db-world/base` si tu actualizador de módulos no los aplica automáticamente. Desactiva/elimina el módulo combinado antiguo.
## Actualizar desde la versión que enviaba equipo al buzón

1. Sustituye los archivos en `modules/mod-prestigio-hollow/`. Conserva ese nombre de carpeta para que el cargador registre `Addmod_prestigio_hollowScripts`.
2. Reconfigura CMake, recompila e instala `worldserver`. Reinicia el proceso con el binario nuevo; recargar SQL no actualiza código C++.
3. En `acore_world`, ejecuta `data/sql/manual/REPAIR_npc_prestigio_hollow.sql`. El script conserva las apariciones y añade el texto de bienvenida y el enlace al script. Para un NPC propio, cambia `@PH_NPC_ENTRY` por su entry antes de ejecutarlo. Este SQL no modifica personajes.
4. Puedes quitar `PrestigioHollow.MailEquippedItems` de tu configuración antigua: ya no se lee. El equipo siempre se guarda en las bolsas y nunca se envía por correo.

### Si el NPC no muestra texto ni opciones

El script C++ añade opciones incluso por debajo del nivel requerido. Si no aparece ninguna, comprueba el enlace al script y que `worldserver` haya cargado el módulo:

```sql
-- Sustituye 2069426 por el entry de tu NPC si es diferente.
SELECT entry, name, npcflag, AIName, ScriptName, gossip_menu_id
FROM creature_template WHERE entry = 2069426;
SELECT guid, id1, npcflag, ScriptName FROM creature WHERE id1 = 2069426;
```

La plantilla debe tener el bit de gossip (`npcflag & 1 = 1`), `AIName` vacío y `ScriptName = npc_prestigio_hollow`. El SQL de reparación elimina los overrides de script en las apariciones. Revisa también los errores de carga de scripts en los logs del servidor. Si tras aplicar SQL aparece texto pero no opciones, comprueba la compilación, instalación y reinicio del binario con el módulo.

### Comprobaciones

El planificador de espacio tiene pruebas locales:

```sh
g++ -std=c++20 -Wall -Wextra -Werror tests/equipment_storage_test.cpp -o /tmp/prestigio-equipment-test
/tmp/prestigio-equipment-test
```

Prueba además en un servidor de desarrollo: bolsas llenas (cancelación sin cambios), espacio justo (equipo en bolsas), bolsas especiales (solo cuentan huecos compatibles), conversación/confirmación y persistencia tras reconectar. La compilación y las pruebas SQL no sustituyen estas comprobaciones en juego.

## Historial de prestigios

Cada prestigio completado registra una fila independiente en `mod_prestigio_hollow_history` con:

- GUID del personaje.
- Numero de prestigio.
- Fecha y hora.
- Nivel anterior.
- Clase.
- Raza.

La tabla `mod_prestigio_hollow_stats` sigue guardando el total actual de prestigios de cada personaje.

### Ver quienes han prestigado

```sql
SELECT
    c.guid,
    c.name AS personaje,
    s.prestige_count AS prestigios
FROM mod_prestigio_hollow_stats s
INNER JOIN characters c ON c.guid = s.player_guid
WHERE s.prestige_count > 0
ORDER BY s.prestige_count DESC, c.name ASC;
```

### Ver el historial completo

```sql
SELECT
    h.history_id,
    c.name AS personaje,
    h.player_guid,
    h.prestige_number,
    h.prestiged_at,
    h.previous_level,
    h.class,
    h.race
FROM mod_prestigio_hollow_history h
LEFT JOIN characters c ON c.guid = h.player_guid
ORDER BY h.prestiged_at DESC, h.history_id DESC;
```

Si ya tenias una version anterior de este modulo instalada, ejecuta una vez `data/sql/manual/UPGRADE_add_prestige_history.sql`. En una instalacion nueva, la tabla se crea desde `data/sql/db-characters/base/mod_prestigio_hollow.sql`.

Los prestigios antiguos que solo existan en `mod_prestigio_hollow_stats` conservan su contador, pero no se inventan fechas historicas. El historial detallado comienza a registrarse desde esta version.
