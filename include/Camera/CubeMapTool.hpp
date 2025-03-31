#ifndef CAMERA_CUBE_MAP_TOOL_HPP
#define CAMERA_CUBE_MAP_TOOL_HPP

#include <JSystem/JGeometry.hpp>
#include <JSystem/JDrama/JDRNameRef.hpp>

class TCubeGeneralInfo : public JDrama::TNameRef {
public:
	TCubeGeneralInfo()
	    : JDrama::TNameRef("<TCubeGeneralInfo>")
	    , unkC(0.0f, 0.0f, 0.0f)
	    , unk18(0.0f, 0.0f, 0.0f)
	    , unk24(1.0f, 1.0f, 1.0f)
	    , unk30(0x80)
	    , unk32(2)
	    , unk34(0)
	{
	}

	const Vec& getUnkC() const { return unkC; }
	const Vec& getUnk18() const { return unk18; }
	const Vec& getUnk24() const { return unk24; }

	virtual void load(JSUMemoryInputStream&);

public:
	/* 0xC */ JGeometry::TVec3<f32> unkC;
	/* 0x18 */ JGeometry::TVec3<f32> unk18;
	/* 0x24 */ JGeometry::TVec3<f32> unk24;
	/* 0x30 */ s16 unk30;
	/* 0x32 */ u16 unk32;
	/* 0x34 */ s32 unk34;
};

class TCubeStreamInfo : public TCubeGeneralInfo {
public:
	virtual ~TCubeStreamInfo() { }
	virtual void load(JSUMemoryInputStream&);

public:
	/* 0x38 */ u32 unk38;
	/* 0x3C */ f32 unk3C;
	/* 0x40 */ f32 unk40;
};

class TCubeCameraInfo : public TCubeGeneralInfo {
public:
	virtual ~TCubeCameraInfo() { }
	virtual void load(JSUMemoryInputStream&);

public:
	/* 0x38 */ JDrama::TNameRef* unk38;
};

#endif
