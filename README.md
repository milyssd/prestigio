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

Instalación: copiar la carpeta con el nombre `mod-prestigio-hollow` a `modules/`, reconfigurar CMake/recompilar y aplicar los SQL de `data/sql/db-characters/base` y `data/sql/db-world/base` si tu actualizador de módulos no los aplica automáticamente. Desactiva/elimina el módulo combinado antiguo.
Para obtener directamente la carpeta con el nombre correcto:

```sh
git clone https://github.com/milyssd/prestigio.git mod-prestigio-hollow
```

Si descargas un ZIP, el nombre recomendado para instalarlo sigue siendo `mod-prestigio-hollow`. También se admite `mod-prestigio-hollow-main`, el nombre que genera GitHub al descargar la rama `main` de un repositorio llamado `mod-prestigio-hollow`.

## Actualizar desde la versión que enviaba equipo al buzón

1. Sustituye los archivos en `modules/mod-prestigio-hollow/`. El nombre recomendado es `mod-prestigio-hollow`; también se admiten `mod-prestigio`, `prestigio` (clonación del repositorio) y `prestigio-main` (ZIP de GitHub). Instala una sola copia del módulo.
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

### Menú, confirmación y vuelta al inicio

El menú incluye **Volver a nivel 1**, **Cerrar** y, en la confirmación, **Volver al menú principal (cancelar)**. La opción de prestigio permanece visible por debajo del nivel requerido e indica el requisito; seleccionarla no permite saltárselo. Si falta espacio o no se cumple una condición, se explica el motivo en el chat y vuelve a mostrarse el menú. La zona inicial por raza se utiliza después de completar el prestigio; no es un teletransporte libre desde el NPC.

Antes de modificar el personaje se comprueba que las tablas de contador e historial puedan consultarse. Si falta SQL o hay un fallo de conexión/permisos, se cancela con un mensaje. Si el prestigio se completa pero AzerothCore rechaza el teletransporte, se muestra una advertencia y se registra el fallo; no repitas el prestigio para intentar viajar.

Para comprobar qué versión está ejecutándose, busca en los logs de inicio:

```text
mod-prestigio-hollow: NPC npc_prestigio_hollow registrado (menu con volver/cancelar).
```

Si no aparece, revisa que hayas compilado, instalado y reiniciado el `worldserver` que realmente ejecutas, con una sola copia del módulo y una sola llamada al registro desde la función de entrada correspondiente a su carpeta. Si aparece pero el NPC no abre el menú, ejecuta el SQL de solo lectura `data/sql/manual/DIAGNOSTIC_npc_prestigio_hollow.sql` y revisa el enlace del script. La configuración cargada de `Enable` y `RequiredLevel` se registra también al leerla.

### Error de enlace LNK2019 / LNK1120

AzerothCore genera el nombre de la función de entrada a partir de la carpeta del módulo. La carpeta `prestigio` requiere `AddprestigioScripts`; `mod-prestigio-hollow` requiere `Addmod_prestigio_hollowScripts`. Esta versión incluye también `Addmod_prestigioScripts` para `mod-prestigio`, ambos nombres anteriores, `Addprestigio_mainScripts` para ZIPs del nombre anterior y `Addmod_prestigio_hollow_mainScripts` para ZIPs del repositorio renombrado. Si un binario anterior no resuelve `AddprestigioScripts`, actualiza `src/PrestigioHollowLoader.cpp`, vuelve a generar CMake y recompila `worldserver`. También puedes usar el nombre recomendado `mod-prestigio-hollow` y volver a generar CMake.

### Comprobaciones

El planificador de espacio tiene pruebas locales:

```sh
g++ -std=c++20 -Wall -Wextra -Werror tests/equipment_storage_test.cpp -o /tmp/prestigio-equipment-test
/tmp/prestigio-equipment-test
g++ -std=c++20 -Wall -Wextra -Werror tests/module_loader_test.cpp src/PrestigioHollowLoader.cpp -o /tmp/prestigio-loader-test
/tmp/prestigio-loader-test
g++ -std=c++20 -Wall -Wextra -Werror -Itests/npc_stubs tests/npc_navigation_test.cpp -o /tmp/prestigio-npc-test
/tmp/prestigio-npc-test
python tests/starting_outfit_test.py
```

La prueba de navegación ejecuta el script real con interfaces mínimas y un gestor simulado; comprueba los menús, no el reinicio completo de un personaje. Prueba además en un servidor de desarrollo: bolsas llenas (cancelación sin cambios), espacio justo (equipo en bolsas), bolsas especiales (solo cuentan huecos compatibles), conversación/confirmación y persistencia tras reconectar. La compilación y las pruebas SQL no sustituyen estas comprobaciones en juego.

### Mensajes sobre `custom_prestige` y la piedra de hogar

El módulo guarda contadores e historial en `mod_prestigio_hollow_stats` y `mod_prestigio_hollow_history`, dentro de la base de personajes. La base `custom_prestige` solo se menciona en `OPTIONAL_migrate_from_custom_prestige.sql`, para importar contadores de una instalación antigua. En instalaciones nuevas ese archivo no es necesario; ahora comprueba la existencia del origen antes de migrar y muestra un aviso si falta. Si el error MySQL 1049 sigue apareciendo durante el juego con esta versión, hay que localizar otro SQL o módulo que siga intentando acceder a esa base; crear una base vacía no reconstruye los datos antiguos.

El objeto `6948` es la piedra de hogar. El error de inventario `17` significa que se alcanzó el máximo permitido de ese objeto. Al entregar el equipo inicial se tienen en cuenta los objetos que ya existen en inventario y banco: se omiten las copias únicas que ya se conservan. Si falla otro objeto inicial, el jugador recibe un mensaje específico en el chat.

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
