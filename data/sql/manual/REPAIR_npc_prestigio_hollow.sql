-- Base de datos: acore_world. NPC del módulo: entry 2069426.
-- Conserva el NPC y sus apariciones; no elimina objetos ni personajes.
-- Si usas otro NPC, cambia solo el valor de @PH_NPC_ENTRY por su entry.
SET @PH_NPC_ENTRY = 2069426;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(2069426, 'Bienvenido al Prestigio Hollow. Al alcanzar el nivel requerido puedes volver a nivel 1. Tu equipo se guardará en tus bolsas: libera un hueco por pieza equipada antes de confirmar.', '', 1)
ON DUPLICATE KEY UPDATE `text0_0`=VALUES(`text0_0`), `text0_1`=VALUES(`text0_1`), `Probability0`=VALUES(`Probability0`);

DELETE FROM `gossip_menu` WHERE `MenuID` = 2069426;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (2069426, 2069426);

UPDATE `creature_template`
SET `npcflag` = `npcflag` | 1, `AIName` = '',
    `ScriptName` = 'npc_prestigio_hollow', `gossip_menu_id` = 2069426
WHERE `entry` = @PH_NPC_ENTRY;

-- El script se resuelve desde la plantilla; elimina overrides de la aparición.
UPDATE `creature` SET `npcflag` = `npcflag` | 1, `ScriptName` = '' WHERE `id1` = @PH_NPC_ENTRY;
