USE `mod_prestigio_hollow`;

-- Ejecutar solo si ya tenías mod-prestigio-hollow instalado antes de añadir el historial.
-- Es seguro ejecutarlo más de una vez.
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
