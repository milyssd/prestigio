-- ==============================================================
-- Chromie NPC (Prestigio Hollow) for AzerothCore
-- Target Database: acore_world (DBUpdater de AzerothCore)
-- Entry: 2069426 | ScriptName: npc_prestigio_hollow
-- ==============================================================

DELETE FROM `creature_template` WHERE `entry` = 2069426;
INSERT INTO `creature_template` (
    `entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`,
    `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
    `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`,
    `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`,
    `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`,
    `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`,
    `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`,
    `flags_extra`, `ScriptName`, `VerifiedBuild`
) VALUES (
    2069426, 0, 0, 0, 0, 0,
    'Chromie', 'Embajadora de Prestigio Hollow', '', 2069426, 80, 80, 0, 35, 1,
    1, 1.14286, 1, 1, 20, 0, 0,
    1, 2000, 2000, 1, 1,
    1, 0, 0, 0, 0, 7, 0,
    0, 0, 0, 0, 0, 0, 0,
    '', 0, 1, 1, 1, 1,
    1, 0, 0, 1, 0,
    0, 'npc_prestigio_hollow', 12340
);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 2069426;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(2069426, 0, 24877, 1.0, 1.0, 12340);

DELETE FROM `creature` WHERE `id1` = 2069426;

-- Spawn: Fuera de Orgrimmar
INSERT INTO `creature` (`id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `Comment`) VALUES
(2069426, 1, 14, 0, 1, 1, 0, 1378.98, -4356.43, 26.1507, 4.88122, 300, 0, 0, 1000, 1000, 0, 1, 0, 0, 'npc_prestigio_hollow', 'Chromie Orgrimmar');

-- Spawn: Fuera de Forjaz (Ironforge)
INSERT INTO `creature` (`id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `Comment`) VALUES
(2069426, 0, 1, 0, 1, 1, 0, -5025.6, -813.51, 495.406, 3.55579, 300, 0, 0, 1000, 1000, 0, 1, 0, 0, 'npc_prestigio_hollow', 'Chromie Ironforge');

-- Spawn: Dalaran
INSERT INTO `creature` (`id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `Comment`) VALUES
(2069426, 571, 4395, 0, 1, 1, 0, 5818.81, 426.866, 658.778, 0, 300, 0, 0, 1000, 1000, 0, 1, 0, 0, 'npc_prestigio_hollow', 'Chromie Dalaran');

-- Base de datos: acore_world. NPC del módulo: entry 2069426.
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(2069426, 'Bienvenido al Prestigio Hollow. Al alcanzar el nivel requerido puedes volver a nivel 1 (los Caballeros de la Muerte no son elegibles). Tu equipo se guardará en tus bolsas: libera un hueco por pieza equipada antes de confirmar.', '', 1)
ON DUPLICATE KEY UPDATE `text0_0`=VALUES(`text0_0`), `text0_1`=VALUES(`text0_1`), `Probability0`=VALUES(`Probability0`);

DELETE FROM `gossip_menu` WHERE `MenuID` = 2069426;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (2069426, 2069426);

UPDATE `creature_template`
SET `npcflag` = `npcflag` | 1, `AIName` = '',
    `ScriptName` = 'npc_prestigio_hollow', `gossip_menu_id` = 2069426
    WHERE `entry` = 2069426;

UPDATE `creature` SET `npcflag` = `npcflag` | 1, `ScriptName` = '' WHERE `id1` = 2069426;
