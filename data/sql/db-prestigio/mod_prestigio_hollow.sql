-- ==============================================================
-- Mod Prestigio Hollow - Base de Datos Exclusiva
-- Destino: Base de datos 'prestigio' (Aislada e independiente)
-- ==============================================================

CREATE DATABASE IF NOT EXISTS `prestigio` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `prestigio`;

-- 1. Estadisticas de los personajes
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_stats` (
  `player_guid` INT UNSIGNED NOT NULL,
  `prestige_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `last_prestiged_at` TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 2. Historial de prestigios completados
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_history` (
  `history_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `player_guid` INT UNSIGNED NOT NULL,
  `account_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `player_name` VARCHAR(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `prestige_number` INT UNSIGNED NOT NULL,
  `prestiged_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `previous_level` TINYINT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `race` TINYINT UNSIGNED NOT NULL,
  `total_time_played` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`history_id`),
  UNIQUE KEY `uq_prestigio_operation` (`operation_id`),
  UNIQUE KEY `uq_prestigio_player_number` (`player_guid`, `prestige_number`),
  KEY `idx_prestiged_at` (`prestiged_at`),
  KEY `idx_player_guid` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 3. Control transaccional de operaciones
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_operations` (
  `operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `player_guid` INT UNSIGNED NOT NULL,
  `completed` TINYINT UNSIGNED NOT NULL,
  `prestige_count` INT UNSIGNED NOT NULL,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`operation_id`),
  KEY `idx_operation_player` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 4. Spells identificadores de prestigio (activos permanentemente del nivel 1 al 79)
-- Al alcanzar el nivel 80, este spell se remueve automaticamente.
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_spells` (
  `prestige_level` INT UNSIGNED NOT NULL COMMENT 'Nivel de prestigio (1, 2, 3...)',
  `spell_id` INT UNSIGNED NOT NULL COMMENT 'ID del spell permanente que identifica el rango mientras levea del 1 al 79',
  PRIMARY KEY (`prestige_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 5. Recompensas configurables (permite multiples objetos por nivel de prestigio)
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_rewards` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `prestige_level` INT UNSIGNED NOT NULL COMMENT 'Nivel de prestigio al que aplica (1, 2, 3...) o 0 para default',
  `reward_item_id` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ID del objeto entregado por correo',
  `reward_item_count` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Cantidad del objeto',
  PRIMARY KEY (`id`),
  KEY `idx_prestige_level` (`prestige_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 6. Retencion de habilidades y profesiones (guardadas durante niveles 1-79 y restauradas al nivel 80)
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_retention_skills` (
  `player_guid` INT UNSIGNED NOT NULL,
  `skill_id` INT UNSIGNED NOT NULL,
  `step` SMALLINT UNSIGNED NOT NULL,
  `value` SMALLINT UNSIGNED NOT NULL,
  `maximum` SMALLINT UNSIGNED NOT NULL,
  PRIMARY KEY (`player_guid`, `skill_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7. Retencion de monturas, recetas y companeros (guardados durante niveles 1-79 y restaurados al nivel 80)
CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow_retention_spells` (
  `player_guid` INT UNSIGNED NOT NULL,
  `spell_id` INT UNSIGNED NOT NULL,
  `spell_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '1=Receta, 2=Montura, 3=Companero, 4=Runeforging',
  PRIMARY KEY (`player_guid`, `spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
