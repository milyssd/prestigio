INSERT INTO `mod_prestigio_hollow_stats` (`player_guid`,`prestige_count`)
SELECT `player_id`,`prestige_level` FROM `custom_prestige`.`prestige_stats`
ON DUPLICATE KEY UPDATE `prestige_count`=VALUES(`prestige_count`);
