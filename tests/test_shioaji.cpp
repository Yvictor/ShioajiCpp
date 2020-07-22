//
// Created by 011295 on 2020/7/1.
//

#include "gtest/gtest.h"
#include "shioaji.h"

namespace sj = shioaji;

class ShioajiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    api_logined = new sj::Shioaji(true);
    api_logined->login("PAPIUSER01", "2222");
  }
  void TearDown() override {

  }
  sj::Shioaji api = sj::Shioaji(true);
  sj::Shioaji *api_logined;
};

TEST_F(ShioajiTest, login) {
  std::vector<sj::account::Account>
      res = api.login("PAPIUSER01", "2222");
  EXPECT_NE(res.size(), 0);
}

TEST_F(ShioajiTest, fetch_contracts) {
  std::vector<sj::contracts::BaseContract>
      res2 = api_logined->fetch_contracts();
  EXPECT_NE(res2.size(), 0);
}
