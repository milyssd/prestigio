-- OPCIONAL. Ejecutar manualmente en la base de PERSONAJES, solo para importar
-- contadores de una instalacion anterior. No es necesario para una instalacion nueva.
-- La ausencia de la base antigua se informa sin intentar consultarla ni crearla.
SET @PH_target_ok = EXISTS(
    SELECT 1 FROM information_schema.TABLES
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'mod_prestigio_hollow_stats'
);
SET @PH_legacy_ok = (
    SELECT COUNT(*) = 2 FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = 'custom_prestige' AND TABLE_NAME = 'prestige_stats'
      AND COLUMN_NAME IN ('player_id', 'prestige_level')
);
SET @PH_migration_sql = IF(
    @PH_target_ok AND @PH_legacy_ok,
    'INSERT INTO `mod_prestigio_hollow_stats` (`player_guid`,`prestige_count`) SELECT `player_id`,`prestige_level` FROM `custom_prestige`.`prestige_stats` ON DUPLICATE KEY UPDATE `prestige_count`=VALUES(`prestige_count`)',
    IF(@PH_target_ok,
       'SELECT ''Migracion omitida: no existe custom_prestige.prestige_stats con las columnas esperadas; no hace falta para instalaciones nuevas.'' AS mensaje',
       'SELECT ''Migracion omitida: selecciona la base de personajes y aplica primero el SQL base del modulo.'' AS mensaje')
);
PREPARE ph_migration FROM @PH_migration_sql;
EXECUTE ph_migration;
DEALLOCATE PREPARE ph_migration;
