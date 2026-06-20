/*
 * Copyright (C) 2005-2024 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "otbCapellaImageMetadataInterface.h"

#include "otbCapellaCalibrationLookupData.h"
#include "otbMacro.h"
#include "otbMissingMetadataException.h"
#include "itksys/SystemTools.hxx"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <numeric>
#include <sstream>

namespace
{
using PTree = boost::property_tree::ptree;

constexpr double SpeedOfLight = 299792458.0;

struct Polynomial2D
{
  std::vector<double> coefficients;
  unsigned int rowCoefficientCount = 0;
  unsigned int columnCoefficientCount = 0;
};

bool EndsWith(const std::string& value, const std::string& suffix)
{
  return value.size() >= suffix.size()
         && std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

std::string StripLastExtension(const std::string& path)
{
  const auto slash = path.find_last_of("/\\");
  const auto dot = path.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
  {
    return path.substr(0, dot);
  }
  return path;
}

std::string ReadFile(const std::string& path)
{
  std::ifstream input(path.c_str());
  if (!input)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Cannot open CAPELLA sidecar metadata file " << path);
  }

  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

std::string FindSidecarMetadata(const otb::MetadataSupplierInterface& mds)
{
  const auto imageFile = mds.GetResourceFile();
  if (!imageFile.empty())
  {
    const auto candidate = StripLastExtension(imageFile) + "_extended.json";
    if (itksys::SystemTools::FileExists(candidate))
    {
      return candidate;
    }
  }

  for (const auto& resource : mds.GetResourceFiles())
  {
    if (EndsWith(resource, "_extended.json") && itksys::SystemTools::FileExists(resource))
    {
      return resource;
    }
  }

  return std::string();
}

std::string ReadEmbeddedOrSidecarDescription(const otb::MetadataSupplierInterface& mds)
{
  bool hasValue = false;
  const auto embedded = mds.GetMetadataValue("TIFFTAG_IMAGEDESCRIPTION", hasValue);
  if (hasValue && !embedded.empty())
  {
    return embedded;
  }

  const auto sidecar = FindSidecarMetadata(mds);
  if (!sidecar.empty())
  {
    return ReadFile(sidecar);
  }

  otbGenericExceptionMacro(otb::MissingMetadataException,
                           << "Missing CAPELLA metadata: neither TIFFTAG_IMAGEDESCRIPTION nor *_extended.json sidecar is available");
  return std::string();
}

PTree ReadJsonString(const std::string& payload, const std::string& sourceName)
{
  try
  {
    std::istringstream input(payload);
    PTree root;
    boost::property_tree::read_json(input, root);
    return root;
  }
  catch (const boost::property_tree::ptree_error& err)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Cannot parse CAPELLA JSON metadata from " << sourceName << ": " << err.what());
  }
  return PTree();
}

PTree ReadCapellaDescription(const otb::MetadataSupplierInterface& mds)
{
  auto root = ReadJsonString(ReadEmbeddedOrSidecarDescription(mds), "TIFFTAG_IMAGEDESCRIPTION");

  const auto productType = root.get<std::string>("product_type", "");
  const auto platform = itksys::SystemTools::UpperCase(root.get<std::string>("collect.platform", ""));
  if (productType != "SLC" || platform.find("CAPELLA") != 0)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Not a CAPELLA SLC product");
  }

  const auto geometry = root.get<std::string>("collect.image.image_geometry.type", "");
  if (geometry != "slant_plane")
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Unsupported CAPELLA image geometry " << geometry << " (expected slant_plane)");
  }

  return root;
}

template <typename T>
T RequiredValue(const PTree& root, const std::string& path)
{
  try
  {
    return root.get<T>(path);
  }
  catch (const boost::property_tree::ptree_error& err)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Missing or invalid CAPELLA metadata " << path << ": " << err.what());
  }
  return T();
}

const PTree& RequiredChild(const PTree& root, const std::string& path)
{
  try
  {
    return root.get_child(path);
  }
  catch (const boost::property_tree::ptree_error& err)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Missing CAPELLA metadata " << path << ": " << err.what());
  }
  return root;
}

PTree ReadSicdMetadata(const otb::MetadataSupplierInterface& mds)
{
  bool hasValue = false;
  const auto sicdPayload = mds.GetMetadataValue("SICD_METADATA", hasValue);
  if (!hasValue || sicdPayload.empty())
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Missing SICD_METADATA required for CAPELLA radiometric calibration");
  }

  auto root = ReadJsonString(sicdPayload, "SICD_METADATA");
  RequiredChild(root, "metadata.Radiometric.BetaZeroSFPoly.Coefs");
  return root;
}

Polynomial2D ReadPolynomial2D(const PTree& root, const std::string& path)
{
  Polynomial2D poly;
  for (const auto& row : RequiredChild(root, path + ".Coefs"))
  {
    unsigned int columnCount = 0;
    for (const auto& column : row.second)
    {
      poly.coefficients.push_back(column.second.get_value<double>());
      ++columnCount;
    }

    if (columnCount == 0)
    {
      poly.coefficients.push_back(row.second.get_value<double>());
      columnCount = 1;
    }

    if (poly.rowCoefficientCount == 0)
    {
      poly.columnCoefficientCount = columnCount;
    }
    else if (poly.columnCoefficientCount != columnCount)
    {
      otbGenericExceptionMacro(otb::MissingMetadataException,
                               << "Invalid CAPELLA radiometric polynomial shape in " << path);
    }
    ++poly.rowCoefficientCount;
  }

  if (poly.coefficients.empty())
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "Empty CAPELLA radiometric polynomial in " << path);
  }
  return poly;
}

otb::MetaData::TimePoint ReadCapellaTime(const std::string& value)
{
  return otb::MetaData::ReadFormattedDate(value);
}

std::string GetPolarization(const PTree& root)
{
  const auto tx = RequiredValue<std::string>(root, "collect.radar.transmit_polarization");
  const auto rx = RequiredValue<std::string>(root, "collect.radar.receive_polarization");
  return itksys::SystemTools::UpperCase(tx + rx);
}

double AveragePrf(const PTree& root)
{
  double sum = 0.0;
  std::size_t count = 0;

  try
  {
    for (const auto& item : root.get_child("collect.radar.prf"))
    {
      sum += item.second.get<double>("prf");
      ++count;
    }
  }
  catch (const boost::property_tree::ptree_error&)
  {
    return 0.0;
  }

  return count == 0 ? 0.0 : sum / static_cast<double>(count);
}

std::vector<otb::Orbit> ReadOrbits(const PTree& root)
{
  std::vector<otb::Orbit> orbits;
  for (const auto& item : RequiredChild(root, "collect.state.state_vectors"))
  {
    const auto& vector = item.second;
    otb::Orbit orbit;
    orbit.time = ReadCapellaTime(RequiredValue<std::string>(vector, "time"));

    auto positionIt = RequiredChild(vector, "position").begin();
    auto velocityIt = RequiredChild(vector, "velocity").begin();
    for (unsigned int i = 0; i < 3; ++i)
    {
      if (positionIt == RequiredChild(vector, "position").end()
          || velocityIt == RequiredChild(vector, "velocity").end())
      {
        otbGenericExceptionMacro(otb::MissingMetadataException,
                                 << "Invalid CAPELLA state vector size");
      }
      orbit.position[i] = positionIt->second.get_value<double>();
      orbit.velocity[i] = velocityIt->second.get_value<double>();
      ++positionIt;
      ++velocityIt;
    }
    orbits.push_back(orbit);
  }

  if (orbits.size() < 2)
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "CAPELLA metadata contains fewer than two state vectors");
  }

  return orbits;
}

void FillGCPTimes(otb::SARParam& sarParam,
                  const otb::Projection::GCPParam& gcpParam,
                  const otb::MetaData::TimePoint& firstAzimuthTime,
                  double azimuthTimeInterval,
                  double firstSlantRangeTime,
                  double rangeTimeInterval)
{
  for (const auto& gcp : gcpParam.GCPs)
  {
    otb::GCPTime gcpTime;
    gcpTime.azimuthTime = firstAzimuthTime + otb::MetaData::Duration::Seconds(gcp.m_GCPRow * azimuthTimeInterval);
    gcpTime.slantRangeTime = firstSlantRangeTime + gcp.m_GCPCol * rangeTimeInterval;
    sarParam.gcpTimes[gcp.m_Id] = gcpTime;
  }
}

const otb::GCP& FindNearestGCP(const otb::Projection::GCPParam& gcpParam, double col, double row)
{
  if (gcpParam.GCPs.empty())
  {
    otbGenericExceptionMacro(otb::MissingMetadataException,
                             << "CAPELLA product has no GCP metadata");
  }

  return *std::min_element(gcpParam.GCPs.begin(), gcpParam.GCPs.end(),
                           [col, row](const otb::GCP& lhs, const otb::GCP& rhs)
                           {
                             const double lhsDist = std::pow(lhs.m_GCPCol - col, 2.0) + std::pow(lhs.m_GCPRow - row, 2.0);
                             const double rhsDist = std::pow(rhs.m_GCPCol - col, 2.0) + std::pow(rhs.m_GCPRow - row, 2.0);
                             return lhsDist < rhsDist;
                           });
}

otb::InfoSceneCoord MakeSceneCoord(const otb::GCP& gcp,
                                   const otb::MetaData::TimePoint& firstAzimuthTime,
                                   double azimuthTimeInterval,
                                   double firstSlantRangeTime,
                                   double rangeTimeInterval,
                                   double incidenceAngle)
{
  otb::InfoSceneCoord coord;
  coord.referenceRow = static_cast<unsigned long>(std::max(0.0, gcp.m_GCPRow));
  coord.referenceColumn = static_cast<unsigned long>(std::max(0.0, gcp.m_GCPCol));
  coord.latitude = gcp.m_GCPY;
  coord.longitude = gcp.m_GCPX;
  coord.azimuthTime = firstAzimuthTime + otb::MetaData::Duration::Seconds(gcp.m_GCPRow * azimuthTimeInterval);
  coord.rangeTime = firstSlantRangeTime + gcp.m_GCPCol * rangeTimeInterval;
  coord.incidenceAngle = incidenceAngle;
  return coord;
}

double ReadAverageSceneHeight(const PTree& root,
                              const otb::MetadataSupplierInterface& mds,
                              const otb::Projection::GCPParam& gcpParam,
                              double centerCol,
                              double centerRow)
{
  bool hasValue = false;
  const auto sicd = mds.GetMetadataValue("SICD_METADATA", hasValue);
  if (hasValue && !sicd.empty())
  {
    try
    {
      const auto sicdRoot = ReadJsonString(sicd, "SICD_METADATA");
      return sicdRoot.get<double>("metadata.GeoData.SCP.LLH.HAE");
    }
    catch (const boost::property_tree::ptree_error&)
    {
      // Fall through to the GCP-derived value.
    }
  }

  (void)root;
  const auto& centerGcp = FindNearestGCP(gcpParam, centerCol, centerRow);
  return centerGcp.m_GCPZ;
}

void FillSarCalibration(otb::SARCalib& sarCalib,
                        const otb::MetadataSupplierInterface& mds,
                        const otb::MetaData::TimePoint& startTime,
                        const otb::MetaData::TimePoint& stopTime)
{
  const auto sicd = ReadSicdMetadata(mds);

  const double rowReferencePixel = RequiredValue<double>(sicd, "metadata.ImageData.SCPPixel.Row");
  const double columnReferencePixel = RequiredValue<double>(sicd, "metadata.ImageData.SCPPixel.Col");
  const double rowSpacing = RequiredValue<double>(sicd, "metadata.Grid.Row.SS");
  const double columnSpacing = RequiredValue<double>(sicd, "metadata.Grid.Col.SS");

  sarCalib.calibrationLookupFlag = true;
  sarCalib.rescalingFactor = 1.0;
  sarCalib.calibrationStartTime = startTime;
  sarCalib.calibrationStopTime = stopTime;
  sarCalib.radiometricCalibrationNoisePolynomialDegree.fill(0);
  sarCalib.radiometricCalibrationAntennaPatternNewGainPolynomialDegree.fill(0);
  sarCalib.radiometricCalibrationAntennaPatternOldGainPolynomialDegree.fill(0);
  sarCalib.radiometricCalibrationIncidenceAnglePolynomialDegree.fill(0);
  sarCalib.radiometricCalibrationRangeSpreadLossPolynomialDegree.fill(0);

  auto makeLut = [&](short type, const std::string& polynomialPath)
  {
    const auto poly = ReadPolynomial2D(sicd, polynomialPath);
    auto lut = otb::CapellaCalibrationLookupData::New();
    lut->Initialize(type, poly.coefficients, poly.rowCoefficientCount, poly.columnCoefficientCount,
                    rowReferencePixel, columnReferencePixel, rowSpacing, columnSpacing);
    sarCalib.calibrationLookupData[type] = lut;
  };

  makeLut(otb::SarCalibrationLookupData::SIGMA, "metadata.Radiometric.SigmaZeroSFPoly");
  makeLut(otb::SarCalibrationLookupData::BETA, "metadata.Radiometric.BetaZeroSFPoly");
  makeLut(otb::SarCalibrationLookupData::GAMMA, "metadata.Radiometric.GammaZeroSFPoly");
  makeLut(otb::SarCalibrationLookupData::NOISE, "metadata.Radiometric.NoiseLevel.NoisePoly");

  auto dn = otb::SarCalibrationLookupData::New();
  dn->SetType(otb::SarCalibrationLookupData::DN);
  sarCalib.calibrationLookupData[otb::SarCalibrationLookupData::DN] = dn;
}

} // end anonymous namespace

namespace otb
{

double CapellaImageMetadataInterface::GetCenterIncidenceAngle(const MetadataSupplierInterface& mds) const
{
  const auto root = ReadCapellaDescription(mds);
  return RequiredValue<double>(root, "collect.image.center_pixel.incidence_angle");
}

void CapellaImageMetadataInterface::Parse(ImageMetadata& imd)
{
  this->ParseGdal(imd);
}

void CapellaImageMetadataInterface::ParseGeom(ImageMetadata& imd)
{
  this->ParseGdal(imd);
}

void CapellaImageMetadataInterface::ParseGdal(ImageMetadata& imd)
{
  if (m_MetadataSupplierInterface == nullptr)
  {
    otbGenericExceptionMacro(MissingMetadataException,
                             << "No metadata supplier available for CAPELLA metadata parsing");
  }

  const auto root = ReadCapellaDescription(*m_MetadataSupplierInterface);

  const auto platform = itksys::SystemTools::UpperCase(RequiredValue<std::string>(root, "collect.platform"));
  const auto productType = itksys::SystemTools::UpperCase(RequiredValue<std::string>(root, "product_type"));
  const auto mode = itksys::SystemTools::UpperCase(RequiredValue<std::string>(root, "collect.mode"));
  const auto polarization = GetPolarization(root);

  const unsigned long numberOfLines = RequiredValue<unsigned long>(root, "collect.image.rows");
  const unsigned long numberOfSamples = RequiredValue<unsigned long>(root, "collect.image.columns");
  const double azimuthTimeInterval = RequiredValue<double>(root, "collect.image.image_geometry.delta_line_time");
  const double rangeToFirstSample = RequiredValue<double>(root, "collect.image.image_geometry.range_to_first_sample");
  const double deltaRangeSample = RequiredValue<double>(root, "collect.image.image_geometry.delta_range_sample");
  const double firstSlantRangeTime = 2.0 * rangeToFirstSample / SpeedOfLight;
  const double rangeTimeInterval = 2.0 * deltaRangeSample / SpeedOfLight;
  const double rangeSamplingRate = 1.0 / rangeTimeInterval;
  const auto firstAzimuthTime = ReadCapellaTime(RequiredValue<std::string>(root, "collect.image.image_geometry.first_line_time"));
  const auto lastAzimuthTime = firstAzimuthTime + MetaData::Duration::Seconds((numberOfLines - 1) * azimuthTimeInterval);
  const double centerIncidenceAngle = RequiredValue<double>(root, "collect.image.center_pixel.incidence_angle");
  const double centerCol = static_cast<double>(numberOfSamples - 1) / 2.0;
  const double centerRow = static_cast<double>(numberOfLines - 1) / 2.0;

  imd.Add(MDStr::SensorID, platform);
  imd.Add(MDStr::Mission, "CAPELLA");
  imd.Add(MDStr::Instrument, "CAPELLA-SAR");
  imd.Add(MDStr::ProductType, productType);
  imd.Add(MDStr::Mode, mode);
  imd.Add(MDStr::BeamMode, mode);
  imd.Add(MDStr::Polarization, polarization);
  imd.Add(MDNum::NumberOfLines, static_cast<double>(numberOfLines));
  imd.Add(MDNum::NumberOfColumns, static_cast<double>(numberOfSamples));
  imd.Add(MDNum::LineSpacing, root.get<double>("collect.image.pixel_spacing_row", azimuthTimeInterval));
  imd.Add(MDNum::PixelSpacing, deltaRangeSample);
  imd.Add(MDNum::RangeTimeFirstPixel, firstSlantRangeTime);
  imd.Add(MDNum::RangeTimeLastPixel, firstSlantRangeTime + (numberOfSamples - 1) * rangeTimeInterval);
  imd.Add(MDNum::RSF, rangeSamplingRate);
  imd.Add(MDNum::PRF, AveragePrf(root));
  imd.Add(MDNum::RadarFrequency, root.get<double>("collect.radar.center_frequency", 0.0));
  imd.Add(MDNum::CenterIncidenceAngle, centerIncidenceAngle);
  imd.Add(MDNum::CalScale, 1.0);
  imd.Add(MDNum::CalFactor, 1.0);
  imd.Add(MDTime::AcquisitionStartTime, firstAzimuthTime);
  imd.Add(MDTime::AcquisitionStopTime, lastAzimuthTime);
  imd.Add(MDTime::AcquisitionDate, firstAzimuthTime);

  const auto processingTime = root.get<std::string>("processing_time", "");
  if (!processingTime.empty())
  {
    imd.Add(MDTime::ProductionDate, ReadCapellaTime(processingTime));
  }

  const auto& gcpParam = imd.GetGCPParam();
  imd.Add(MDNum::AverageSceneHeight,
          ReadAverageSceneHeight(root, *m_MetadataSupplierInterface, gcpParam, centerCol, centerRow));

  SARParam sarParam;
  sarParam.azimuthTimeInterval = MetaData::Duration::Seconds(azimuthTimeInterval);
  sarParam.nearRangeTime = firstSlantRangeTime;
  sarParam.rangeSamplingRate = rangeSamplingRate;
  sarParam.rangeResolution = deltaRangeSample;
  sarParam.numberOfLinesPerBurst = numberOfLines;
  sarParam.numberOfSamplesPerBurst = numberOfSamples;
  sarParam.rangeBandwidth = root.get<double>("collect.image.processed_range_bandwidth", 0.0);
  sarParam.azimuthBandwidth = root.get<double>("collect.image.processed_azimuth_bandwidth", 0.0);
  sarParam.azimuthSteeringRate = 0.0;
  sarParam.rightLookingFlag = itksys::SystemTools::UpperCase(root.get<std::string>("collect.radar.pointing", "right")) == "RIGHT";

  BurstRecord burst;
  burst.azimuthStartTime = firstAzimuthTime;
  burst.azimuthStopTime = lastAzimuthTime;
  burst.startLine = 0;
  burst.endLine = numberOfLines - 1;
  burst.startSample = 0;
  burst.endSample = numberOfSamples - 1;
  burst.azimuthAnxTime = 0.0;
  sarParam.burstRecords.push_back(burst);

  sarParam.orbits = ReadOrbits(root);

  DopplerCentroid centroid;
  centroid.azimuthTime = firstAzimuthTime;
  centroid.t0 = firstSlantRangeTime;
  centroid.dopCoef = {root.get<double>("collect.image.reference_doppler_centroid", 0.0), 0.0, 0.0};
  centroid.geoDopCoef = {0.0, 0.0, 0.0};
  sarParam.dopplerCentroids.push_back(centroid);

  FillGCPTimes(sarParam, gcpParam, firstAzimuthTime, azimuthTimeInterval,
               firstSlantRangeTime, rangeTimeInterval);

  const auto& ulGcp = FindNearestGCP(gcpParam, 0.0, 0.0);
  const auto& urGcp = FindNearestGCP(gcpParam, static_cast<double>(numberOfSamples - 1), 0.0);
  const auto& llGcp = FindNearestGCP(gcpParam, 0.0, static_cast<double>(numberOfLines - 1));
  const auto& lrGcp = FindNearestGCP(gcpParam, static_cast<double>(numberOfSamples - 1), static_cast<double>(numberOfLines - 1));
  const auto& centerGcp = FindNearestGCP(gcpParam, centerCol, centerRow);

  sarParam.ulSceneCoord = MakeSceneCoord(ulGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.urSceneCoord = MakeSceneCoord(urGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.llSceneCoord = MakeSceneCoord(llGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.lrSceneCoord = MakeSceneCoord(lrGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);
  sarParam.centerSceneCoord = MakeSceneCoord(centerGcp, firstAzimuthTime, azimuthTimeInterval, firstSlantRangeTime, rangeTimeInterval, centerIncidenceAngle);

  imd.Add(MDGeom::SAR, sarParam);

  SARCalib sarCalib;
  FillSarCalibration(sarCalib, *m_MetadataSupplierInterface, firstAzimuthTime, lastAzimuthTime);
  imd.Add(MDGeom::SARCalib, sarCalib);
}

} // end namespace otb
