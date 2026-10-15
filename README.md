<div align="center">

# Prestigio Hollow

Una nueva vuelta al nivel 1 para tu servidor AzerothCore.

![AzerothCore 3.3.5a](https://img.shields.io/badge/AzerothCore-3.3.5a-2563eb?style=flat-square)
![MySQL InnoDB](https://img.shields.io/badge/MySQL-InnoDB-7c3aed?style=flat-square)
![NPC 2069426](https://img.shields.io/badge/NPC-2069426-d4a650?style=flat-square)

[Instalación](#instalación) · [Configuración](#configuración) · [Ayuda](#ayuda) · [Detalles técnicos](#detalles-técnicos)

</div>

**`mod-prestigio-hollow`** permite volver del nivel requerido —80 por defecto— al nivel 1 y comenzar de nuevo en la zona inicial de tu raza. También admite Caballeros de la Muerte.

El equipo se guarda en tus propias bolsas y cada prestigio queda registrado en una base de datos del módulo. Funciona sin modificar el código del core y sin depender de `mod-habilidades-random`.

## Cómo funciona

Habla con el NPC, revisa los requisitos y confirma el prestigio. El menú permite volver atrás, cancelar o cerrar en cualquier momento antes de confirmar.

| Elemento | Resultado al prestigiar |
| :--- | :--- |
| Nivel y experiencia | Nivel 1 y XP a cero. |
| Ubicación | Zona inicial de la raza al volver a entrar. |
| Equipo puesto | Piezas originales a las bolsas, con sus GUID, encantamientos, gemas y durabilidad. Las bolsas equipadas permanecen en su sitio. |
| Inventario y progreso personal | Conserva inventario, banco, oro, correo, logros, raza y clase. |
| Idiomas | Se conservan; los idiomas iniciales ausentes se reparan al conectar o hablar con el NPC. |
| Profesiones y habilidades permanentes | Con `PreservePermanentSpells = 1`, conserva recetas, rango, puntos actuales y máximos, plazas de profesión, equitación, monturas, compañeros y forja de runas. |
| Misiones y combate | Reinicia misiones activas y completadas, hechizos de combate, talentos de ambas especializaciones y todas sus barras. |

Necesitas **un hueco libre compatible por cada pieza equipada**. El módulo comprueba el espacio antes de mover el equipo y lo guarda directamente en las bolsas, sin enviarlo por correo.

> [!IMPORTANT]
> **Al confirmar volverás a la selección de personajes mientras se guarda.** Puedes entrar de nuevo cuando termine. Durante un guardado pendiente se bloquean la entrada y el borrado de ese personaje. Si el reinicio falla y se confirma su reversión, se restaura el estado original sin sumar un prestigio.

Si otro módulo impide bajar al nivel 1, el prestigio se cancela antes de borrar misiones o incrementar el contador.

## Instalación

Necesitas AzerothCore 3.3.5a y acceso administrativo a MySQL para preparar la base de datos.

> [!IMPORTANT]
> La base propia **`mod_prestigio_hollow` debe estar en la misma instancia MySQL que la base de personajes**, con tablas InnoDB. El usuario de `CharacterDatabaseInfo` necesita acceso a ambas para guardar el personaje y su prestigio en una sola transacción.

### 1. Instala o actualiza el módulo

Detén `worldserver`. Desde la carpeta raíz de AzerothCore, instala una sola copia:

```sh
git clone https://github.com/milyssd/prestigio.git modules/mod-prestigio-hollow
```

Si ya lo instalaste con Git, actualiza esa copia. Si usas el [ZIP de GitHub](https://github.com/milyssd/prestigio/archive/refs/heads/main.zip), sustituye la carpeta completa para evitar que queden archivos SQL antiguos.

<details>
<summary>Nombres de carpeta compatibles</summary>

El nombre recomendado es `mod-prestigio-hollow`. También se admiten `mod-prestigio`, `prestigio`, `prestigio-main` y `mod-prestigio-hollow-main`, con sus archivos `.cmake` y funciones de entrada correspondientes.

Instala una sola copia y conserva todos los archivos del módulo.

</details>

### 2. Prepara la base de datos

Ejecuta el archivo que corresponda con una cuenta administradora, en la instancia MySQL de personajes:

| Tu instalación | Archivo SQL |
| :--- | :--- |
| Primera instalación de la base propia | [Crear la base y las tres tablas](data/sql/db-prestigio/base/mod_prestigio_hollow.sql). Puede repetirse. |
| Ya tienes la base propia | [Actualizar el guardado atómico](data/sql/manual/UPGRADE_atomic_prestige.sql). Añade el identificador de operación y los recibos. |

<details>
<summary>Migrar datos de versiones anteriores</summary>

Si tus contadores e historial estaban en `characters`, ejecuta [MIGRATE_from_characters.sql](data/sql/manual/MIGRATE_from_characters.sql) después de preparar la base propia. Ajusta `@PH_characters_source` si tu base no se llama `acore_characters`.

La migración conserva el origen, evita duplicar números de prestigio y no reduce contadores existentes. Las copias antiguas no se eliminan automáticamente; la versión nueva deja de utilizarlas.

Para el formato antiguo `custom_prestige.prestige_stats`, utiliza [la importación opcional](data/sql/manual/OPTIONAL_migrate_from_custom_prestige.sql). No necesitas crear `custom_prestige` para una instalación nueva.

Si quedó tras una copia manual, retira el archivo antiguo `data/sql/db-characters/base/mod_prestigio_hollow.sql` de la carpeta del módulo.

</details>

### 3. Concede los permisos

El usuario indicado en **`CharacterDatabaseInfo`** necesita `SELECT`, `INSERT` y `UPDATE` sobre `mod_prestigio_hollow.*`, además de sus permisos normales sobre personajes.

Ejecuta este ejemplo con una cuenta administradora, sustituyendo usuario y host por los de tu servidor:

```sql
GRANT SELECT, INSERT, UPDATE ON mod_prestigio_hollow.* TO 'acore'@'localhost';
```

La cuenta del servidor no necesita crear tablas ni ejecutar migraciones. Sus credenciales permanecen en la configuración privada del servidor.

### 4. Configura el módulo y el NPC

Actualiza tu archivo `.conf` a partir de [mod_prestigio_hollow.conf.dist](conf/mod_prestigio_hollow.conf.dist).

| Estado del NPC | Qué ejecutar en la base `world` |
| :--- | :--- |
| Primera instalación | [SQL base del NPC](data/sql/db-world/base/mod_prestigio_hollow.sql). |
| El NPC 2069426 ya existe | [Reparación del NPC](data/sql/manual/REPAIR_npc_prestigio_hollow.sql), sin reemplazar sus apariciones. Para otro entry, ajusta `@PH_NPC_ENTRY`. |

### 5. Compila y reinicia

**Regenera CMake, recompila e instala el nuevo `worldserver` y reinícialo.** Conserva los archivos `.cmake` del módulo: enlazan la dependencia MySQL detectada por el core, también en Windows.

Copiar SQL o ejecutar `.reload` no actualiza el código C++.

## Configuración

Valores predeterminados en [el archivo de ejemplo](conf/mod_prestigio_hollow.conf.dist):

| Opción | Valor | Función |
| :--- | :---: | :--- |
| `PrestigioHollow.Enable` | `1` | Activa el módulo. |
| `PrestigioHollow.RequiredLevel` | `80` | Nivel mínimo para prestigiar. |
| `PrestigioHollow.AnnounceWorld` | `1` | Anuncia el prestigio después de confirmar el guardado. |
| `PrestigioHollow.GiveStartingOutfit` | `1` | Entrega objetos iniciales respetando los límites de objetos únicos. |
| `PrestigioHollow.PreservePermanentSpells` | `1` | Conserva profesiones y habilidades permanentes con su progreso. |

Puedes retirar estas opciones de configuraciones antiguas:

- `PrestigioHollow.DatabaseInfo`: la conexión ahora utiliza `CharacterDatabaseInfo` para permitir la transacción entre ambas bases.
- `PrestigioHollow.MailEquippedItems`: el equipo se guarda siempre en las bolsas.

Con `Enable = 0` no se abre la conexión al arrancar. Si lo desactivas durante un guardado ya aceptado, esa operación termina de resolverse.

## Ayuda

<details>
<summary>El NPC no muestra texto ni opciones</summary>

Ejecuta primero el [diagnóstico de solo lectura](data/sql/manual/DIAGNOSTIC_npc_prestigio_hollow.sql). La plantilla debe tener gossip activado, `AIName` vacío, `ScriptName = npc_prestigio_hollow` y el menú `2069426`.

El [SQL de reparación](data/sql/manual/REPAIR_npc_prestigio_hollow.sql) corrige la plantilla y las configuraciones específicas de sus apariciones. Revisa también en los logs el registro de `npc_prestigio_hollow` y el estado de configuración del módulo.

</details>

<details>
<summary>Aparece «No conoces ese idioma»</summary>

Instala el módulo actualizado y reconecta o habla con el NPC. Se reparan los idiomas iniciales de raza y clase aunque el contador de prestigios no esté disponible.

Los idiomas adicionales perdidos antes de esta corrección requieren datos anteriores para poder recuperarse.

</details>

<details>
<summary>El contador no está disponible o el guardado sigue pendiente</summary>

Comprueba que la base propia exista en la instancia de personajes, que se haya aplicado la actualización SQL, que las tablas sean InnoDB y que el usuario de `CharacterDatabaseInfo` tenga los permisos indicados.

El NPC consulta el contador en segundo plano y muestra «contador no disponible» ante un fallo. Una respuesta antigua no reemplaza el menú de confirmación ni reabre el menú después de seleccionar «Cerrar».

Una operación con resultado desconocido permanece pendiente hasta resolverse. Revisa los mensajes de operaciones guardadas o canceladas en los logs. Los errores del trabajador del módulo no llaman a `World::StopNow()` ni al manejador fatal de conexión del core.

</details>

## Detalles técnicos

### Datos del módulo

Las tres tablas propias viven exclusivamente en **`mod_prestigio_hollow`**:

| Tabla | Contenido |
| :--- | :--- |
| `mod_prestigio_hollow_stats` | Contador de prestigios por GUID. |
| `mod_prestigio_hollow_history` | Número de prestigio, fecha, nivel anterior, clase, raza e identificador de operación. |
| `mod_prestigio_hollow_operations` | Recibos de operaciones completadas o canceladas para resolver reintentos. |

El estado del personaje se guarda en las tablas nativas mediante `Player::SaveToDB` y las consultas preparadas del core. El NPC utiliza registros de `world`. No se añaden tablas del módulo a `characters`, `world` ni `auth`.

<details>
<summary>Cómo se protege el guardado</summary>

Un trabajador propio usa `CharacterDatabaseInfo` y guarda el personaje, el contador, el historial y el recibo en una sola transacción entre ambas bases. Si el proceso cae, MySQL confirma todo o revierte todo.

Antes de cambiar al personaje, se verifican el esquema, los permisos e InnoDB y se captura su estado original, incluidos los objetos nuevos aún sin guardar. Después se vuelve a selección de personajes sin lanzar otro guardado automático del reinicio. El trabajador guarda la copia original y el reinicio juntos; una cancelación confirmada guarda solo el original y su recibo. El anuncio de éxito se envía al confirmar el guardado.

El resultado de `COMMIT` se comprueba explícitamente. Antes de reintentar, el recibo se consulta bajo bloqueo del contador: perder la respuesta de una confirmación no cuenta dos prestigios.

Las conexiones, consultas y reconexiones se ejecutan fuera del hilo del mundo, con mantenimiento periódico y límites de conexión, lectura y escritura configurados a 3 segundos. Una operación con resultado desconocido sigue pendiente y mantiene bloqueado el reingreso que pudiera sobrescribirla.

</details>

<details>
<summary>Consultas de administración</summary>

**Contadores**

```sql
SELECT player_guid, prestige_count
FROM mod_prestigio_hollow.mod_prestigio_hollow_stats
ORDER BY prestige_count DESC;
```

**Historial**

```sql
SELECT player_guid, prestige_number, prestiged_at, previous_level, class, race
FROM mod_prestigio_hollow.mod_prestigio_hollow_history
ORDER BY history_id DESC;
```

**Recibos de operaciones**

```sql
SELECT operation_id, player_guid, completed, prestige_count, created_at
FROM mod_prestigio_hollow.mod_prestigio_hollow_operations
ORDER BY created_at DESC;
```

Los datos antiguos conservan sus contadores e historial disponible. No se inventan fechas ni recibos de operaciones anteriores. No borres recibos mientras existan guardados pendientes.

</details>

### Verificación y compatibilidad

Para ejecutar las pruebas locales:

```sh
python3 tests/run_local_tests.py
```

Cubren huecos compatibles, cinco nombres de carpeta, navegación del NPC y barrera de entrada, idiomas, profesiones parciales y máximas, objetos únicos y métodos de producción del reinicio con un modelo del core. Las comprobaciones `CHECK` también se evalúan con `NDEBUG`.

La [prueba de integración](tests/database_integration.cpp), enlazada con `database/common` del core, utiliza MySQL desechable para verificar la transacción entre bases, consultas preparadas nativas, reintentos sin duplicados, cancelación y restauración del original, limpieza de ambas barras, caducidad de conexión, esquema ausente y módulo desactivado. Ejecútala únicamente con datos de prueba.

**Referencia de compilación:** AzerothCore [`5fa7cb0`](https://github.com/azerothcore/azerothcore-wotlk/commit/5fa7cb00fa04814c4afe6701f0c6c09e9fb96cea), del 15 de agosto de 2026, en Linux.

Las pruebas con interfaces simuladas no sustituyen una prueba en juego. No se ha ejecutado la solución completa de Windows ni un cliente WoW contra tu servidor. Antes de usarlo en producción, comprueba en desarrollo el prestigio completo, bolsas, idiomas, profesiones, ambas especializaciones y reconexión.
