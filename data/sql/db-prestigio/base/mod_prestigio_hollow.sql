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
 `history_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
 `player_guid` INT UNSIGNED NOT NULL,
 `prestige_number` INT UNSIGNED NOT NULL,
 `prestiged_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 `previous_level` TINYINT UNSIGNED NOT NULL,
 `class` TINYINT UNSIGNED NOT NULL,
 `race` TINYINT UNSIGNED NOT NULL,
 PRIMARY KEY (`history_id`),
 UNIQUE KEY `uq_mod_prestigio_hollow_player_number` (`player_guid`,`prestige_number`),
 KEY `idx_mod_prestigio_hollow_prestiged_at` (`prestiged_at`),
 KEY `idx_mod_prestigio_hollow_player_guid` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
