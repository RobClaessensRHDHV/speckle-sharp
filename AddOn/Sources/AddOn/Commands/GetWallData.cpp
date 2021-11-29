#include "GetWallData.hpp"
#include "ResourceIds.hpp"
#include "ObjectState.hpp"
#include "SchemaDefinitionBuilder.hpp"
#include "Utility.hpp"
#include "Objects/Point.hpp"
#include "RealNumber.h"


namespace AddOnCommands {

static const char* ElementIdsFieldName = "elementIds";
static const char* WallsFieldName = "walls";
static const char* ElementIdFieldName = "elementId";
static const char* StartPointFieldName = "startPoint";
static const char* EndPointFieldName = "endPoint";
static const char* HeightFieldName = "height";
static const char* ThicknessFieldName = "thickness";
static const char* FirstThicknessFieldName = "firstThickness";
static const char* SecondThicknessFieldName = "secondThickness";
static const char* FloorIndexFieldName = "floorIndex";
static const char* ArcAngleFieldName = "arcAngle";
static const char* StructureFieldName = "structure";
static const char* GeometryMethodFieldName = "geometryMethod";
static const char* WallComplexityFieldName = "wallComplexity";
static const char* OutsideSlantAngleFieldName = "outsideSlantAngle";
static const char* InsideSlantAngleFieldName = "insideSlantAngle";


const GS::HashTable<API_WallTypeID, GS::UniString> wallTypeNames
{
	{ APIWtyp_Normal,		"Straight"},
	{ APIWtyp_Trapez,		"Trapezoid"},
	{ APIWtyp_Poly,			"Polygonal"}
};


const GS::HashTable<short, GS::UniString> profileTypeNames
{
	{ APISect_Normal,		"Straight"},
	{ APISect_Poly,			"Profiled"},
	{ APISect_Slanted,		"Slanted"},
	{ APISect_Trapez,		"Double Slanted"}
};


GS::ObjectState SerializeWallType (const API_WallType& wall)
{
	GS::ObjectState os;

	//Wall GUID
	os.Add (ElementIdFieldName, APIGuidToString (wall.head.guid));

	//Floor index of the wall
	os.Add (FloorIndexFieldName, wall.head.floorInd);

	//Wall start and end points
	double z = Utility::GetStoryLevel (wall.head.floorInd) + wall.bottomOffset;
	os.Add (StartPointFieldName, Objects::Point3D (wall.begC.x, wall.begC.y, z));
	os.Add (EndPointFieldName, Objects::Point3D (wall.endC.x, wall.endC.y, z));

	//Arc angle of a curved wall
	if (abs (wall.angle) > EPS)
		os.Add (ArcAngleFieldName, wall.angle);

	//Height of the wall
	os.Add (HeightFieldName, wall.height);

	//Structure type of the wall
	os.Add (StructureFieldName, Utility::structureTypeNames.Get (wall.modelElemStructureType));

	//Geometry type of the wall
	os.Add (GeometryMethodFieldName, wallTypeNames.Get (wall.type));

	//Profile type of the wall
	os.Add (WallComplexityFieldName, profileTypeNames.Get (wall.profileType));

	//Thicknesses of the wall
	if (wall.type == APIWtyp_Trapez) {
		os.Add (FirstThicknessFieldName, wall.thickness);
		os.Add (SecondThicknessFieldName, wall.thickness1);
	} else {
		os.Add (ThicknessFieldName, wall.thickness);
	}

	//wall slanted angles
	os.Add (OutsideSlantAngleFieldName, wall.slantAlpha);
	if (wall.profileType == APISect_Trapez)
		os.Add (InsideSlantAngleFieldName, wall.slantBeta);

	return os;
}

GS::String GetWallData::GetNamespace () const
{
	return CommandNamespace;
}


GS::String GetWallData::GetName () const
{
	return GetWallDataCommandName;
}
	
		
GS::Optional<GS::UniString> GetWallData::GetSchemaDefinitions () const
{
	Json::SchemaDefinitionBuilder builder;
	builder.Add (Json::SchemaDefinitionProvider::ElementIdsSchema ());
	builder.Add (Json::SchemaDefinitionProvider::WallDataSchema ());
	return builder.Build();
}


GS::Optional<GS::UniString>	GetWallData::GetInputParametersSchema () const
{
	return R"(
		{
			"type": "object",
			"properties" : {
				"elementIds": { "$ref": "#/definitions/ElementIds" }
			},
			"additionalProperties" : false,
			"required" : [ "elementIds" ]
		}
	)";
}


GS::Optional<GS::UniString> GetWallData::GetResponseSchema () const
{
	return R"(
		{
			"type": "object",
			"properties" : {
				"walls": {
					"type": "array",
					"items": { "$ref": "#/definitions/WallData" }
				}
			},
			"additionalProperties" : false,
			"required" : [ "walls" ]
		}
	)";
}


API_AddOnCommandExecutionPolicy GetWallData::GetExecutionPolicy () const
{
	return API_AddOnCommandExecutionPolicy::ScheduleForExecutionOnMainThread; 
}


GS::ObjectState GetWallData::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
	GS::Array<GS::UniString> ids;
	parameters.Get (ElementIdsFieldName, ids);
	GS::Array<API_Guid>	elementGuids = ids.Transform<API_Guid> ([] (const GS::UniString& idStr) { return APIGuidFromString (idStr.ToCStr ()); });

	GS::ObjectState result;
	
	API_Element element;
	GSErrCode err;
	const auto& listAdder = result.AddList<GS::ObjectState> (WallsFieldName);
	for (const API_Guid& guid : elementGuids) {

		BNZeroMemory (&element, sizeof (API_Element));
		element.header.guid = guid;
		err = ACAPI_Element_Get (&element);
		if (err != NoError) continue;

		if (element.header.typeID != API_WallID) continue;	//a security check

		listAdder (SerializeWallType (element.wall));
	}

	return result;
}


void GetWallData::OnResponseValidationFailed (const GS::ObjectState& /*response*/) const
{
}


}