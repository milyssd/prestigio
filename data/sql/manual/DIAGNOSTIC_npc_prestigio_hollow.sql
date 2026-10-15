-- Solo lectura. Ejecutar en acore_world para revisar el NPC 2069426.
SELECT `entry`, `name`, `npcflag`, (`npcflag` & 1) AS `tiene_gossip`,
       `AIName`, `ScriptName`, `gossip_menu_id`
FROM `creature_template` WHERE `entry` = 2069426;

SELECT `guid`, `id1`, `npcflag`, `ScriptName`, `map`, `phaseMask`
FROM `creature` WHERE `id1` = 2069426;

SELECT g.`MenuID`, g.`TextID`, t.`text0_0`
FROM `gossip_menu` g LEFT JOIN `npc_text` t ON t.`ID` = g.`TextID`
WHERE g.`MenuID` = 2069426;
