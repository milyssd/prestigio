-- Ejecutar una vez con una cuenta administradora de MySQL.
-- Este archivo solo crea la base propia y sus tablas; no modifica las del core.
CREATE DATABASE IF NOT EXISTS `mod_prestigio_hollow` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `mod_prestigio_hollow`;

CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow`.`mod_prestigio_hollow_stats` (
 `player_guid` INT UNSIGNED NOT NULL,
 `prestige_count` INT UNSIGNED NOT NULL DEFAULT 0,
 PRIMARY KEY (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow`.`mod_prestigio_hollow_history` (
 `operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NULL,
 `history_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
 `player_guid` INT UNSIGNED NOT NULL,
 `prestige_number` INT UNSIGNED NOT NULL,
 `prestiged_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 `previous_level` TINYINT UNSIGNED NOT NULL,
 `class` TINYINT UNSIGNED NOT NULL,
 `race` TINYINT UNSIGNED NOT NULL,
 PRIMARY KEY (`history_id`),
 UNIQUE KEY `uq_prestigio_operation` (`operation_id`),
 UNIQUE KEY `uq_mod_prestigio_hollow_player_number` (`player_guid`,`prestige_number`),
 KEY `idx_mod_prestigio_hollow_prestiged_at` (`prestiged_at`),
 KEY `idx_mod_prestigio_hollow_player_guid` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Ejecutar en la MISMA instancia MySQL que la base de personajes.
-- No modifica tablas ni procedimientos del core.
USE `mod_prestigio_hollow`;
SET @PH_has_operation = EXISTS(SELECT 1 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA='mod_prestigio_hollow' AND TABLE_NAME='mod_prestigio_hollow_history' AND COLUMN_NAME='operation_id');
SET @PH_upgrade = IF(@PH_has_operation, 'SELECT ''operation_id ya existe'' AS mensaje',
    'ALTER TABLE `mod_prestigio_hollow`.`mod_prestigio_hollow_history` ADD COLUMN `operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NULL, ADD UNIQUE KEY `uq_prestigio_operation` (`operation_id`)');
PREPARE ph_upgrade FROM @PH_upgrade;
EXECUTE ph_upgrade;
DEALLOCATE PREPARE ph_upgrade;

CREATE TABLE IF NOT EXISTS `mod_prestigio_hollow`.`mod_prestigio_hollow_operations` (
 `operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `player_guid` INT UNSIGNED NOT NULL,
 `completed` TINYINT UNSIGNED NOT NULL,
 `prestige_count` INT UNSIGNED NOT NULL,
 `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`operation_id`),
 KEY `idx_prestigio_operation_player` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
