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

#ifndef otbCapellaImageMetadataInterface_h
#define otbCapellaImageMetadataInterface_h

#include "OTBMetadataExport.h"
#include "otbSarImageMetadataInterface.h"

namespace otb
{

/** \class CapellaImageMetadataInterface
 *
 * \brief Metadata reader for CAPELLA SLC GeoTIFF products.
 *
 * CAPELLA GeoTIFF SLCs expose the extended product JSON in
 * TIFFTAG_IMAGEDESCRIPTION and an additional SICD metadata JSON block in
 * SICD_METADATA. This reader uses the embedded TIFF metadata first and only
 * falls back to the *_extended.json sidecar when the TIFF tag is unavailable.
 *
 * \ingroup OTBMetadata
 */
class OTBMetadata_EXPORT CapellaImageMetadataInterface : public SarImageMetadataInterface
{
public:
  typedef CapellaImageMetadataInterface Self;
  typedef SarImageMetadataInterface     Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  itkNewMacro(Self);
  itkTypeMacro(CapellaImageMetadataInterface, SarImageMetadataInterface);

  double GetCenterIncidenceAngle(const MetadataSupplierInterface&) const override;

  void Parse(ImageMetadata& imd) override;
  void ParseGdal(ImageMetadata& imd) override;
  void ParseGeom(ImageMetadata& imd) override;

protected:
  CapellaImageMetadataInterface() = default;
  ~CapellaImageMetadataInterface() override = default;

private:
  CapellaImageMetadataInterface(const Self&) = delete;
  void operator=(const Self&) = delete;
};

} // end namespace otb

#endif
