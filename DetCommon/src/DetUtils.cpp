#include "DetCommon/DetUtils.h"

// DD4hep
#include "DDG4/Geant4Mapping.h"
#include "DDG4/Geant4VolumeManager.h"

// Geant
#include "G4NavigationHistory.hh"

// ROOT
#include "TGeoBBox.h"

namespace det {
namespace utils {
DD4hep::XML::Component getNodeByStrAttr(const DD4hep::XML::Handle_t& mother, const std::string& nodeName, const std::string& attrName,
                                             const std::string& attrValue) {
  for (DD4hep::XML::Collection_t xCompColl(mother, nodeName.c_str()); nullptr != xCompColl; ++xCompColl) {
    if (xCompColl.attr<std::string>(attrName.c_str()) == attrValue) {
      return static_cast<DD4hep::XML::Component>(xCompColl);
    }
  }
  // in case there was no xml daughter with matching name
  return DD4hep::XML::Component(nullptr);
}

double getAttrValueWithFallback(const DD4hep::XML::Component& node, const std::string& attrName, const double& defaultValue) {
  if (node.hasAttr(attrName.c_str())) {
    return node.attr<double>(attrName.c_str());
  } else {
    return defaultValue;
  }
}

uint64_t cellID(const DD4hep::Geometry::Segmentation& aSeg, const G4Step& aStep, bool aPreStepPoint) {
  DD4hep::Simulation::Geant4VolumeManager volMgr =
    DD4hep::Simulation::Geant4Mapping::instance().volumeManager();
  DD4hep::Geometry::VolumeManager::VolumeID volID =
    volMgr.volumeID(aStep.GetPreStepPoint()->GetTouchable());
  if (aSeg.isValid() )  {
    G4ThreeVector global;
    if (aPreStepPoint) {
      global = aStep.GetPreStepPoint()->GetPosition();
    } else {
      global = 0.5*(aStep.GetPreStepPoint()->GetPosition()+aStep.GetPostStepPoint()->GetPosition());
    }
    G4ThreeVector local  = aStep.GetPreStepPoint()->GetTouchable()->
      GetHistory()->GetTopTransform().TransformPoint(global);
    DD4hep::Geometry::Position loc(local.x()*MM_2_CM, local.y()*MM_2_CM, local.z()*MM_2_CM);
    DD4hep::Geometry::Position glob(global.x()*MM_2_CM, global.y()*MM_2_CM, global.z()*MM_2_CM);
    DD4hep::Geometry::VolumeManager::VolumeID cID = aSeg.cellID(loc,glob,volID);
    return cID;
  }
  return volID;
}

std::vector<uint64_t> neighbours(DD4hep::DDSegmentation::BitField64& aDecoder,
    const std::vector<std::string>& aDimensionNames,
    uint64_t aCellId) {
  std::vector<uint64_t> neighbours;
  aDecoder.setValue(aCellId);
  for(const auto& dim: aDimensionNames) {
    int id = aDecoder[dim];
    try {
      neighbours.emplace_back(aDecoder.getValue());
      aDecoder[dim] = id+1;
      neighbours.emplace_back(aDecoder.getValue());
    } catch (std::runtime_error e) {}
    try {
      aDecoder[dim] = id-1;
      neighbours.emplace_back(aDecoder.getValue());
    } catch (std::runtime_error e) {}
    aDecoder[dim] = id;
  }
  return std::move(neighbours);
}

std::vector<std::pair<int,int>> bitfieldExtremes(DD4hep::DDSegmentation::BitField64& aDecoder) {
  std::vector<std::pair<int,int>> extremes;
  int width = 0;
  for(uint it_fields=0;it_fields<aDecoder.size();it_fields++) {
    width = aDecoder[it_fields].width();
    if( aDecoder[it_fields].isSigned() ){
      extremes.emplace_back(std::make_pair( ( 1LL << ( width - 1 ) ) - ( 1LL << width ),
          ( 1LL << ( width - 1 ) ) - 1));
    } else {
      extremes.emplace_back(std::make_pair( 0,
         (0x0001 << width) - 1 ));
    }
  }
  return std::move(extremes);
}

CLHEP::Hep3Vector numberOfCellsInCartesian(uint64_t aVolumeId, const DD4hep::DDSegmentation::CartesianGridXYZ& aSeg) {
  DD4hep::Geometry::VolumeManager volMgr =
    DD4hep::Geometry::LCDD::getInstance().volumeManager();
  std::cout << "\t looking for id " << aVolumeId <<std::endl;
  auto pvol = volMgr.lookupPlacement(aVolumeId);
  auto vol = pvol.volume();
  auto solid = vol.solid();
  // get the envelope of the shape
  TGeoBBox* box = (dynamic_cast<TGeoBBox*>(solid.ptr()));
  // get half-widths
  double xMaxVolumeHalfSize = box->GetDX();
  double yMaxVolumeHalfSize = box->GetDY();
  double zMaxVolumeHalfSize = box->GetDZ();
  // get segmentation cell widths
  double xCellSize = aSeg.gridSizeX();
  double yCellSize = aSeg.gridSizeY();
  double zCellSize = aSeg.gridSizeZ();
  // calculate number of cells, the middle cell is centred at 0 (no offset)
  int cellsX = ceil((xMaxVolumeHalfSize-xCellSize/2.)/xCellSize)*2+1;
  int cellsY = ceil((yMaxVolumeHalfSize-yCellSize/2.)/yCellSize)*2+1;
  int cellsZ = ceil((zMaxVolumeHalfSize-zCellSize/2.)/zCellSize)*2+1;
  return CLHEP::Hep3Vector(cellsX, cellsY, cellsZ);
}
}
}
