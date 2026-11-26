#pragma once
// Infernis: the multiplayer game types of GSC's EGameIDs (IX-Ray's values; Infernis's engine keeps eGameIDNoGame and
// eGameIDSingle only). Scene files store which game types an object is used in as these bits, so the Level Editor
// reads them, shows them and writes them back unchanged.
enum EEditorMPGameIDs : u32
{
	eGameIDDeathmatch = u32(1) << 1,
	eGameIDTeamDeathmatch = u32(1) << 2,
	eGameIDArtefactHunt = u32(1) << 3,
	eGameIDCaptureTheArtefact = u32(1) << 4,
	eGameIDDominationZone = u32(1) << 5,
	eGameIDCapturePoints = u32(1) << 6,
	eGameIDFreeMP = u32(1) << 7,
};
