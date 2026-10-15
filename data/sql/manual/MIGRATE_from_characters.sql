-- OPCIONAL: conservar contadores e historial de versiones anteriores del modulo.
-- Ejecutar con worldserver detenido y una cuenta que pueda LEER la base antigua
-- y ESCRIBIR la base propia. Primero aplicar db-prestigio/base/mod_prestigio_hollow.sql.
-- Cambiar este nombre si la base de personajes tiene otro nombre.
SET @PH_characters_source = 'acore_characters';
USE `mod_prestigio_hollow`;
SET @PH_source_quoted = REPLACE(@PH_characters_source, '`', '``');
SET @PH_has_stats = (
    SELECT COUNT(*) = 2 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = @PH_characters_source AND TABLE_NAME = 'mod_prestigio_hollow_stats'
      AND COLUMN_NAME IN ('player_guid','prestige_count')
);
SET @PH_copy_stats = IF(@PH_has_stats,
    CONCAT('INSERT INTO `mod_prestigio_hollow`.`mod_prestigio_hollow_stats` (`player_guid`,`prestige_count`) SELECT `player_guid`,`prestige_count` FROM `', @PH_source_quoted,
        '`.`mod_prestigio_hollow_stats` ON DUPLICATE KEY UPDATE `prestige_count`=GREATEST(`mod_prestigio_hollow`.`mod_prestigio_hollow_stats`.`prestige_count`,VALUES(`prestige_count`))'),
    'SELECT ''No hay contadores antiguos que copiar en la base indicada.'' AS mensaje');
PREPARE ph_copy_stats FROM @PH_copy_stats;
EXECUTE ph_copy_stats;
DEALLOCATE PREPARE ph_copy_stats;

SET @PH_has_history = (
    SELECT COUNT(*) = 6 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = @PH_characters_source AND TABLE_NAME = 'mod_prestigio_hollow_history'
      AND COLUMN_NAME IN ('player_guid','prestige_number','prestiged_at','previous_level','class','race')
);
SET @PH_copy_history = IF(@PH_has_history,
    CONCAT('INSERT INTO `mod_prestigio_hollow`.`mod_prestigio_hollow_history` (`player_guid`,`prestige_number`,`prestiged_at`,`previous_level`,`class`,`race`) SELECT `player_guid`,`prestige_number`,`prestiged_at`,`previous_level`,`class`,`race` FROM `', @PH_source_quoted,
        '`.`mod_prestigio_hollow_history` ON DUPLICATE KEY UPDATE `prestige_number`=VALUES(`prestige_number`)'),
    'SELECT ''No hay historial antiguo que copiar en la base indicada.'' AS mensaje');
PREPARE ph_copy_history FROM @PH_copy_history;
EXECUTE ph_copy_history;
DEALLOCATE PREPARE ph_copy_history;
-- No elimina ni altera las tablas de origen. Conserva esas copias hasta verificar la migracion.
