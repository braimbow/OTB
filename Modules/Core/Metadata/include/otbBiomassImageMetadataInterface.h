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

#ifndef otbBiomassImageMetadataInterface_h
#define otbBiomassImageMetadataInterface_h

#include "OTBMetadataExport.h"
#include "otbSarImageMetadataInterface.h"

namespace otb
{

/** \class BiomassImageMetadataInterface
 *
 * \brief Metadata reader for ESA BIOMASS L1 SCS and STA products.
 *
 * The current support targets non-ground-projected SCS products and L1C/STA
 * stack products represented by the official measurement VRT. BIOMASS images
 * are handled as single-burst slant-range SAR images, similar to TSX/PAZ/TDX
 * or CSK in OTB. For STA stacks, the primary annotation provides the common
 * stack geometry and the coregistered annotation provides product-level LUTs.
 *
 * \ingroup OTBMetadata
 */
class OTBMetadata_EXPORT BiomassImageMetadataInterface : public SarImageMetadataInterface
{
public:
  typedef BiomassImageMetadataInterface Self;
  typedef SarImageMetadataInterface     Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  itkNewMacro(Self);
  itkTypeMacro(BiomassImageMetadataInterface, SarImageMetadataInterface);

  double GetCenterIncidenceAngle(const MetadataSupplierInterface&) const override;

  void Parse(ImageMetadata& imd) override;
  void ParseGdal(ImageMetadata& imd) override;
  void ParseGeom(ImageMetadata& imd) override;

protected:
  BiomassImageMetadataInterface() = default;
  ~BiomassImageMetadataInterface() override = default;

private:
  BiomassImageMetadataInterface(const Self&) = delete;
  void operator=(const Self&) = delete;
};

} // end namespace otb

#endif
