#include "testing/testing.hpp"

#include "indexer/data_header.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_add.hpp"

#include "std/algorithm.hpp"
#include "std/bind.hpp"
#include "std/string.hpp"

using platform::CountryFile;
using platform::LocalCountryFile;

namespace
{
class IndexTest : public MwmSet::Observer
{
public:
  IndexTest() { TEST(m_index.AddObserver(*this), ()); }

  ~IndexTest() override { TEST(m_index.RemoveObserver(*this), ()); }

  void ExpectRegistered(platform::LocalCountryFile const & localFile)
  {
    AddEvent(m_expected, MwmSet::Event::TYPE_REGISTERED, localFile);
  }

  void ExpectUpdated(platform::LocalCountryFile const & newFile,
                     platform::LocalCountryFile const & oldFile)
  {
    AddEvent(m_expected, MwmSet::Event::TYPE_UPDATED, newFile, oldFile);
  }

  void ExpectDeregistered(platform::LocalCountryFile const & localFile)
  {
    AddEvent(m_expected, MwmSet::Event::TYPE_DEREGISTERED, localFile);
  }

  // Checks expectations and clears them.
  bool CheckExpectations()
  {
    bool ok = true;
    if (m_actual != m_expected)
    {
      LOG(LINFO, ("Check failed. Expected:", m_expected, "actual:", m_actual));
      ok = false;
    }
    m_actual.clear();
    m_expected.clear();
    return ok;
  }

  // MwmSet::Observer overrides:
  void OnMapRegistered(platform::LocalCountryFile const & localFile) override
  {
    AddEvent(m_actual, MwmSet::Event::TYPE_REGISTERED, localFile);
  }

  void OnMapUpdated(platform::LocalCountryFile const & newFile,
                    platform::LocalCountryFile const & oldFile) override
  {
    AddEvent(m_actual, MwmSet::Event::TYPE_UPDATED, newFile, oldFile);
  }

  void OnMapDeregistered(platform::LocalCountryFile const & localFile) override
  {
    AddEvent(m_actual, MwmSet::Event::TYPE_DEREGISTERED, localFile);
  }

protected:
  template <typename... TArgs>
  void AddEvent(vector<MwmSet::Event> & events, TArgs... args)
  {
    events.emplace_back(forward<TArgs>(args)...);
  }

  Index m_index;
  vector<MwmSet::Event> m_expected;
  vector<MwmSet::Event> m_actual;
};
}  // namespace

UNIT_CLASS_TEST(IndexTest, Parse)
{
  UNUSED_VALUE(m_index.RegisterMap(platform::LocalCountryFile::MakeForTesting("minsk-pass")));

  // Make sure that index is actually parsed.
  NoopFunctor fn;
  m_index.ForEachInScale(fn, 15);
}

UNIT_CLASS_TEST(IndexTest, StatusNotifications)
{
  string const mapsDir = GetPlatform().WritableDir();
  CountryFile const country("minsk-pass");

  // These two classes point to the same file, but will be considered
  // by Index as distinct files because versions are artificially set
  // to different numbers.
  LocalCountryFile const file1(mapsDir, country, 1 /* version */);
  LocalCountryFile const file2(mapsDir, country, 2 /* version */);

  MwmSet::MwmId id1;

  // Checks that observers are triggered after map registration.
  {
    auto result = m_index.RegisterMap(file1);
    TEST_EQUAL(MwmSet::RegResult::Success, result.second, ());

    id1 = result.first;
    TEST(id1.IsAlive(), ());

    ExpectRegistered(file1);
    TEST(CheckExpectations(), ());
  }

  // Checks that map can't registered twice.
  {
    auto result = m_index.RegisterMap(file1);
    TEST_EQUAL(MwmSet::RegResult::VersionAlreadyExists, result.second, ());

    TEST(result.first.IsAlive(), ());
    TEST_EQUAL(id1, result.first, ());

    TEST(CheckExpectations(), ());
  }

  // Checks that observers are notified when map is updated.
  MwmSet::MwmId id2;
  {
    auto result = m_index.RegisterMap(file2);
    TEST_EQUAL(MwmSet::RegResult::Success, result.second, ());

    id2 = result.first;
    TEST(id2.IsAlive(), ());
    TEST_NOT_EQUAL(id1, id2, ());

    ExpectUpdated(file2, file1);
    TEST(CheckExpectations(), ());
  }

  // Tries to deregister a map in presence of an active handle. Map
  // should be marked "to be removed" but can't be deregistered. After
  // leaving the inner block the map should be deregistered.
  {
    {
      MwmSet::MwmHandle const handle = m_index.GetMwmHandleByCountryFile(country);
      TEST(handle.IsAlive(), ());

      TEST(!m_index.DeregisterMap(country), ());
      TEST(CheckExpectations(), ());
    }

    ExpectDeregistered(file2);
    TEST(CheckExpectations(), ());
  }
}
