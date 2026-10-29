<div align="center">

# Prestigio Hollow

Una nueva vuelta al nivel 1 para tu servidor AzerothCore.

![AzerothCore 3.3.5a](https://img.shields.io/badge/AzerothCore-3.3.5a-2563eb?style=flat-square)
![MySQL InnoDB](https://img.shields.io/badge/MySQL-InnoDB-7c3aed?style=flat-square)
![NPC 2069426](https://img.shields.io/badge/NPC-2069426-d4a650?style=flat-square)

[Instalación](#instalación) · [Configuración](#configuración) · [Ayuda](#ayuda) · [Detalles técnicos](#detalles-técnicos)

</div>

**`mod-prestigio-hollow`** permite volver del nivel requerido —80 por defecto— al nivel 1 y comenzar de nuevo en la zona inicial de tu raza.

> [!NOTE]
> **Restricción de clase:** Los Caballeros de la Muerte (Death Knights) **no son elegibles** para prestigiar debido a que reiniciarlos al nivel 1 corrompe sus árboles de talentos, runas y progresión de clase única en WotLK.

El equipo se guarda en tus propias bolsas y cada prestigio queda registrado en la base de datos de personajes. Funciona de manera limpia, estándar con el sistema DBUpdater de AzerothCore.

## Cómo funciona

Habla con el NPC Chromie (disponible en Orgrimmar, Forjaz y Dalaran), revisa los requisitos y confirma el prestigio. El menú permite volver atrás, cancelar o cerrar en cualquier momento antes de confirmar.

| Elemento | Resultado al prestigiar |
| :--- | :--- |
| Nivel y experiencia | Nivel 1 y XP a cero. |
| Ubicación | Zona inicial de la raza al volver a entrar. |
| Equipo puesto | Piezas originales a las bolsas, con sus GUID, encantamientos, gemas y durabilidad. Las bolsas equipadas permanecen en su sitio. |
| Inventario y progreso personal | Conserva inventario, banco, oro, correo, logros, raza y clase. |
| Idiomas | Se conservan; los idiomas iniciales se garantizan y reparan al conectar o hablar con el NPC. |
| Profesiones y habilidades permanentes | Con `PreservePermanentSpells = 1`, conserva recetas, rango, puntos actuales y máximos, plazas de profesión, equitación, monturas y compañeros. |
| Misiones y combate | Reinicia misiones activas y completadas, hechizos de combate de nivel superior, talentos de ambas especializaciones y todas sus barras. |

Necesitas **un hueco libre compatible por cada pieza equipada**. El módulo comprueba el espacio antes de mover el equipo y lo guarda directamente en las bolsas, sin enviarlo por correo.

> [!IMPORTANT]
> **Al confirmar volverás a la selección de personajes mientras se guarda.** Puedes entrar de nuevo cuando termine. Durante un guardado pendiente se bloquean la entrada y el borrado de ese personaje. Si el reinicio falla, se restaura el estado original sin sumar un prestigio.

Si otro módulo impide bajar al nivel 1, el prestigio se cancela antes de borrar misiones o incrementar el contador.

## Instalación

Compatible de forma estándar con AzerothCore 3.3.5a y el sistema DBUpdater.

### 1. Ubicación del módulo

Coloca el módulo en la carpeta `modules/` de tu servidor AzerothCore:

```sh
# Ejemplo de clonación en modules/
git clone https://github.com/milyssd/prestigio.git modules/prestigio
```

### 2. Base de datos (DBUpdater automático)

El módulo sigue el estándar oficial de AzerothCore:
- Las tablas de personajes se encuentran en [`data/sql/db-characters/base/mod_prestigio_hollow.sql`](data/sql/db-characters/base/mod_prestigio_hollow.sql) y se aplican automáticamente a `acore_characters`.
- El NPC Chromie y sus apariciones se encuentran en [`data/sql/db-world/base/mod_prestigio_hollow.sql`](data/sql/db-world/base/mod_prestigio_hollow.sql) y se aplican automáticamente a `acore_world`.

No se requieren bases de datos externas ni permisos manuales especiales.

### 3. Configuración

Copia [`conf/mod_prestigio_hollow.conf.dist`](conf/mod_prestigio_hollow.conf.dist) a la carpeta de configuraciones de tu servidor (`configs/modules/mod_prestigio_hollow.conf`) y ajusta las opciones deseadas.

### 4. Compilar y reiniciar

Recompila AzerothCore e inicia `worldserver`.

## Configuración

Valores en [`conf/mod_prestigio_hollow.conf.dist`](conf/mod_prestigio_hollow.conf.dist):

| Opción | Valor por defecto | Función |
| :--- | :---: | :--- |
| `PrestigioHollow.Enable` | `1` | Activa o desactiva el módulo. |
| `PrestigioHollow.RequiredLevel` | `80` | Nivel mínimo requerido para prestigiar. |
| `PrestigioHollow.MaxPrestige` | `0` | Límite máximo de prestigios (`0` = sin límite / infinito). |
| `PrestigioHollow.AnnounceWorld` | `1` | Anuncia en el mundo cuando un jugador prestigia. |
| `PrestigioHollow.GiveStartingOutfit` | `1` | Entrega equipamiento inicial de nivel 1 en bolsas. |
| `PrestigioHollow.PreservePermanentSpells` | `1` | Conserva profesiones, monturas y habilidades permanentes. |
| `PrestigioHollow.Reward.X.ItemId` | `0` | ID del objeto recompensa enviado por correo en el prestigio X (1, 2, 3, 4, 5...). |
| `PrestigioHollow.Reward.X.Count` | `1` | Cantidad del objeto recompensa para el prestigio X. |
| `PrestigioHollow.Reward.Default.ItemId` | `0` | Recompensa por defecto para prestigios superiores no configurados individualmente. |

## Detalles técnicos

### Estructura en Base de Datos

Las tablas residen directamente en la base de datos de personajes (`acore_characters`):

| Tabla | Contenido |
| :--- | :--- |
| `mod_prestigio_hollow_stats` | Contador de prestigios por GUID. |
| `mod_prestigio_hollow_history` | Historial de prestigios: número, fecha, nivel anterior, clase, raza y operación. |
| `mod_prestigio_hollow_operations` | Recibos de operaciones atómicas para garantizar consistencia. |

### Consultas de administración útiles

**Top jugadores con más prestigios:**
```sql
SELECT c.name, s.prestige_count, c.race, c.class
FROM acore_characters.mod_prestigio_hollow_stats s
JOIN acore_characters.characters c ON c.guid = s.player_guid
ORDER BY s.prestige_count DESC;
```

**Historial reciente de prestigios:**
```sql
SELECT player_name, account_id, prestige_number, prestiged_at, previous_level, total_time_played, reward_item_id, reward_count
FROM acore_characters.mod_prestigio_hollow_history
ORDER BY history_id DESC LIMIT 50;
```
