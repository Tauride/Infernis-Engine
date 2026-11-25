#pragma once
#include "Nodes.h"

class CNodeBone:
	public INodeUnknown
{
	CBone* BonePtr = nullptr;

	xr_string BoneName;
	float Mass = 0;

	Fvector3 MassPos = {};
	Fvector3 Pos = {};
	Fvector3 Rot = {};

public:
	CNodeBone(const char* Name);
	void Draw() override;

	void SetBone(CBone* Ptr);
};

class CNodeShape :
	public INodeUnknown
{
	bool Pickable = true;
	bool Physics = true;
	bool RemoveAfterBreak = false;
	bool FogCollider = true;
	int Type = SBoneShape::stNone;

	SBoneShape* ShapePtr = nullptr;

public:
	CNodeShape();
	void Draw() override;

	void SetShape(SBoneShape* Ptr);

public:
	// Infernis: the engine's SBoneShape has no operator== (IX-Ray added one); the same test here: type, flags and
	// the shape of that type (plain floats).
	inline bool operator==(const SBoneShape& Left)
	{
		const SBoneShape& Right = *ShapePtr;
		if (Left.type != Right.type || Left.flags.get() != Right.flags.get())
			return false;
		switch (Left.type)
		{
		case SBoneShape::stBox:			return 0 == memcmp(&Left.box, &Right.box, sizeof(Left.box));
		case SBoneShape::stSphere:		return 0 == memcmp(&Left.sphere, &Right.sphere, sizeof(Left.sphere));
		case SBoneShape::stCylinder:	return 0 == memcmp(&Left.cylinder, &Right.cylinder, sizeof(Left.cylinder));
		default:						return false;
		}
	}
};