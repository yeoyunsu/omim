#pragma once

#include "indexer/feature_decl.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/point2d.hpp"

#include <string>
#include <utility>
#include <vector>

class FeatureBuilder1;
class FeatureType;

namespace osm
{
class Editor;
}

namespace generator
{
namespace tests_support
{
class TestFeature
{
public:
  virtual ~TestFeature() = default;

  bool Matches(FeatureType const & feature) const;
  inline void SetPostcode(std::string const & postcode) { m_postcode = postcode; }
  inline uint64_t GetId() const { return m_id; }
  inline std::string const & GetName() const { return m_name; }

  virtual void Serialize(FeatureBuilder1 & fb) const;
  virtual std::string ToString() const = 0;

protected:
  TestFeature(std::string const & name, std::string const & lang);
  TestFeature(m2::PointD const & center, std::string const & name, std::string const & lang);

  uint64_t const m_id;
  m2::PointD const m_center;
  bool const m_hasCenter;
  std::string const m_name;
  std::string const m_lang;
  std::string m_postcode;
};

class TestCountry : public TestFeature
{
public:
  TestCountry(m2::PointD const & center, std::string const & name, std::string const & lang);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;
};

class TestCity : public TestFeature
{
public:
  TestCity(m2::PointD const & center, std::string const & name, std::string const & lang, uint8_t rank);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

private:
  uint8_t const m_rank;
};

class TestVillage : public TestFeature
{
public:
  TestVillage(m2::PointD const & center, std::string const & name, std::string const & lang, uint8_t rank);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

private:
  uint8_t const m_rank;
};

class TestStreet : public TestFeature
{
public:
  TestStreet(std::vector<m2::PointD> const & points, std::string const & name, std::string const & lang);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

private:
  std::vector<m2::PointD> m_points;
};

class TestPOI : public TestFeature
{
public:
  TestPOI(m2::PointD const & center, std::string const & name, std::string const & lang);

  static std::pair<TestPOI, FeatureID> AddWithEditor(osm::Editor & editor, MwmSet::MwmId const & mwmId,
                                                std::string const & enName, m2::PointD const & pt);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

  inline void SetHouseNumber(std::string const & houseNumber) { m_houseNumber = houseNumber; }
  inline void SetStreet(TestStreet const & street) { m_streetName = street.GetName(); }
  inline void SetTypes(std::vector<std::vector<std::string>> const & types) { m_types = types; }

private:
  std::string m_houseNumber;
  std::string m_streetName;
  std::vector<std::vector<std::string>> m_types;
};

class TestBuilding : public TestFeature
{
public:
  TestBuilding(m2::PointD const & center, std::string const & name, std::string const & houseNumber,
               std::string const & lang);
  TestBuilding(m2::PointD const & center, std::string const & name, std::string const & houseNumber,
               TestStreet const & street, std::string const & lang);
  TestBuilding(std::vector<m2::PointD> const & boundary, std::string const & name, std::string const & houseNumber,
               TestStreet const & street, std::string const & lang);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

private:
  std::vector<m2::PointD> const m_boundary;
  std::string const m_houseNumber;
  std::string const m_streetName;
};

class TestPark : public TestFeature
{
public:
  TestPark(std::vector<m2::PointD> const & boundary, std::string const & name, std::string const & lang);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

private:
  std::vector<m2::PointD> m_boundary;
};

class TestRoad : public TestFeature
{
public:
  TestRoad(std::vector<m2::PointD> const & points, std::string const & name, std::string const & lang);

  // TestFeature overrides:
  void Serialize(FeatureBuilder1 & fb) const override;
  std::string ToString() const override;

private:
  std::vector<m2::PointD> m_points;
};

std::string DebugPrint(TestFeature const & feature);
}  // namespace tests_support
}  // namespace generator
